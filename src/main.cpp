#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <vector>
#include "board.hpp"
#include "stone.hpp"
#include <string>
#include <iostream>
#include <sstream>
#include <deque>
#include <future>
#include <chrono>
using namespace std;
using namespace sf;

static const unsigned short kPort = 53000;

static deque<string> pendingStates;

static void enqueueState(
    const vector<Stone>& stones,
    bool isBlackTurn,
    int consecutivePasses,
    bool gameEnded,
    unsigned long long revision
) {
    pendingStates.push_back(
        "STATE " +
        to_string(revision) + " " +
        serializeBoard(stones) + " " +
        (isBlackTurn ? "1" : "0") + " " +
        to_string(consecutivePasses) + " " +
        (gameEnded ? "1" : "0") + "\n"
    );
}

static Socket::Status pumpSend(TcpSocket& socket) {
    // Bound network work so it cannot monopolize a frame.
    for (int attempt = 0;
         attempt < 8 && !pendingStates.empty();
         ++attempt) {
        string& message = pendingStates.front();
        size_t sent = 0;

        const Socket::Status status =
            socket.send(message.data(), message.size(), sent);

        if (status == Socket::Done || status == Socket::Partial) {
            message.erase(0, sent);

            if (message.empty()) {
                pendingStates.pop_front();
            }

            if (sent == 0) {
                break;
            }
        } else if (status == Socket::NotReady) {
            break;
        } else {
            return status;
        }
    }

    return Socket::Done;
}

static bool applyRemoteState(
    const string& line,
    vector<Stone>& stones,
    bool& isBlackTurn,
    int& consecutivePasses,
    bool& gameEnded,
    RuleHistory& history,
    unsigned long long& revision,
    bool myColorIsBlack
) {
    string tag;
    string boardKey;
    string extra;
    unsigned long long incomingRevision = 0;
    int turnBit = -1;
    int incomingPasses = -1;
    int endedBit = -1;

    istringstream input(line);

    if (!(input >> tag
                >> incomingRevision
                >> boardKey
                >> turnBit
                >> incomingPasses
                >> endedBit)) {
        return false;
    }

    if (input >> extra) {
        return false;
    }

    if (tag != "STATE" ||
        incomingRevision != revision + 1 ||
        boardKey.size() !=
            static_cast<size_t>(boardSize * boardSize) ||
        (turnBit != 0 && turnBit != 1) ||
        incomingPasses < 0 ||
        incomingPasses > 2 ||
        (endedBit != 0 && endedBit != 1)) {
        return false;
    }

    for (char cell : boardKey) {
        if (cell != '0' && cell != 'B' && cell != 'W') {
            return false;
        }
    }

    // The remote player may only act on their own turn.
    if (isBlackTurn == myColorIsBlack || gameEnded) {
        return false;
    }

    const string before = serializeBoard(stones);

    vector<Stone> candidateStones = stones;
    bool candidateTurn = isBlackTurn;
    int candidatePasses = consecutivePasses;
    bool candidateEnded = gameEnded;
    RuleHistory candidateHistory = history;

    bool accepted = false;

    if (boardKey == before) {
        // A legal pass leaves the board unchanged.
        accepted = tryPass(
            candidateStones,
            candidateTurn,
            candidatePasses,
            candidateEnded,
            candidateHistory
        );
    } else {
        // Identify the single newly placed stone.
        const char movingColor = isBlackTurn ? 'B' : 'W';
        int placedIndex = -1;

        for (size_t i = 0; i < boardKey.size(); ++i) {
            if (before[i] == '0' && boardKey[i] == movingColor) {
                if (placedIndex != -1) {
                    return false;
                }

                placedIndex = static_cast<int>(i);
            }
        }

        if (placedIndex == -1) {
            return false;
        }

        accepted = tryPlaceStone(
            candidateStones,
            candidateTurn,
            candidatePasses,
            candidateEnded,
            candidateHistory,
            placedIndex % boardSize,
            placedIndex / boardSize
        );
    }

    // The received snapshot must exactly match the result of a legal action.
    if (!accepted ||
        serializeBoard(candidateStones) != boardKey ||
        candidateTurn != (turnBit == 1) ||
        candidatePasses != incomingPasses ||
        candidateEnded != (endedBit == 1)) {
        return false;
    }

    stones.swap(candidateStones);
    isBlackTurn = candidateTurn;
    consecutivePasses = candidatePasses;
    gameEnded = candidateEnded;
    history = candidateHistory;
    revision = incomingRevision;

    return true;
}

static Socket::Status recvState(
    TcpSocket& socket,
    string& buffer,
    vector<Stone>& stones,
    bool& isBlackTurn,
    int& consecutivePasses,
    bool& gameEnded,
    RuleHistory& history,
    unsigned long long& revision,
    bool myColorIsBlack
) {
    for (int reads = 0; ; ++reads) {
        // Process buffered messages BEFORE attempting another receive.
        size_t newline;

        while ((newline = buffer.find('\n')) != string::npos) {
            const string line = buffer.substr(0, newline);
            buffer.erase(0, newline + 1);

            if (!applyRemoteState(
                    line,
                    stones,
                    isBlackTurn,
                    consecutivePasses,
                    gameEnded,
                    history,
                    revision,
                    myColorIsBlack)) {
                return Socket::Error;
            }
        }

        if (reads >= 8) {
            return Socket::Done;
        }

        char data[2048];
        size_t received = 0;

        const Socket::Status status =
            socket.receive(data, sizeof(data), received);

        if (status == Socket::NotReady) {
            return Socket::Done;
        }

        if (status != Socket::Done) {
            return status;
        }

        buffer.append(data, received);

        // Prevent an invalid peer from growing the buffer indefinitely.
        if (buffer.size() > 16384) {
            return Socket::Error;
        }
    }
}

static bool parseHostAddress(
    const string& text,
    IpAddress& result
) {
    if (text == "localhost") {
        result = IpAddress::LocalHost;
        return true;
    }

    istringstream input(text);
    int octets[4];

    for (int i = 0; i < 4; ++i) {
        if (!(input >> octets[i]) ||
            octets[i] < 0 ||
            octets[i] > 255) {
            return false;
        }

        if (i < 3) {
            char separator;

            if (!(input >> separator) || separator != '.') {
                return false;
            }
        }
    }

    input >> ws;

    if (!input.eof()) {
        return false;
    }

    result = IpAddress(
        static_cast<Uint8>(octets[0]),
        static_cast<Uint8>(octets[1]),
        static_cast<Uint8>(octets[2]),
        static_cast<Uint8>(octets[3])
    );

    return true;
}

int main(int argc, char** argv) {
    ContextSettings settings;
    settings.antialiasingLevel = 4;
    RenderWindow window(VideoMode(400, 400), "Go/Weiqi/Baduk", Style::Default, settings);
    window.setPosition(Vector2i(300, 10));
    window.setVerticalSyncEnabled(true);
    window.setKeyRepeatEnabled(false);

    bool networked = false;
    bool isHost = false;
    bool connected = false;

    TcpListener listener;
    TcpSocket socket;
    string recvBuffer;

    // Declare this AFTER socket so the background task finishes before
    // socket is destroyed when main() exits.
    future<Socket::Status> connectTask;

    IpAddress hostIp;
    int connectAttempts = 0;
    const int maxConnectAttempts = 20;
    Clock retryClock;
    string connectionStatus;

    if (argc == 2 && string(argv[1]) == "--host") {
        networked = true;
        isHost = true;

        listener.setBlocking(false);

        if (listener.listen(kPort) != Socket::Done) {
            cerr << "Failed to listen on port " << kPort
                << ". Is another host already running?\n";
            return 1;
        }

        connectionStatus = "Waiting for White";
        cout << "Waiting for peer on port " << kPort << "...\n";
    } else if (
        argc == 3 &&
        string(argv[1]) == "--join"
    ) {
        networked = true;
        isHost = false;

        if (!parseHostAddress(argv[2], hostIp)) {
            cerr << "Use an IPv4 address, such as 127.0.0.1, "
                << "or localhost.\n";
            return 1;
        }

        connectionStatus = "Connecting";
    } else if (argc != 1) {
        cerr << "Usage:\n"
            << "  " << argv[0] << "\n"
            << "  " << argv[0] << " --host\n"
            << "  " << argv[0] << " --join <IPv4-or-localhost>\n";
        return 1;
    }

    View view = createBoardView();
    updateViewForWindow(window, view);

    vector<Stone> stonePositions;
    bool isBlackTurn = true;
    int consecutivePasses = 0;
    bool gameEnded = false;
    RuleHistory history;
    unsigned long long revision = 0;
    bool connectionLost = false;
    string lastTitle;
    bool myTurn = !networked;
    const bool myColorIsBlack = !networked || isHost;

    while (window.isOpen()) {
        if (networked && !connected && !connectionLost) {
            if (isHost) {
                const Socket::Status status = listener.accept(socket);

                if (status == Socket::Done) {
                    socket.setBlocking(false);
                    connected = true;
                    connectionStatus = "Connected";

                    listener.close();

                    cout << "Client connected\n";
                } else if (status != Socket::NotReady) {
                    connectionLost = true;
                    connectionStatus = "Accept failed";
                    listener.close();

                    cerr << "Failed to accept a connection.\n";
                }
            } else {
                if (
                    connectTask.valid() &&
                    connectTask.wait_for(chrono::milliseconds(0)) ==
                        future_status::ready
                ) {
                    const Socket::Status status = connectTask.get();

                    if (status == Socket::Done) {
                        // The background task is finished. The main thread
                        // can now safely use this socket.
                        socket.setBlocking(false);
                        connected = true;
                        connectionStatus = "Connected";

                        cout << "Connected to host\n";
                    } else if (connectAttempts >= maxConnectAttempts) {
                        connectionLost = true;
                        connectionStatus = "Connection failed";

                        cerr << "Failed to connect after "
                            << maxConnectAttempts << " attempts.\n";
                    } else {
                        retryClock.restart();
                        connectionStatus = "Retrying connection";
                    }
                }

                const bool retryDue =
                    connectAttempts == 0 ||
                    retryClock.getElapsedTime() >= milliseconds(500);

                if (
                    !connected &&
                    !connectionLost &&
                    !connectTask.valid() &&
                    retryDue
                ) {
                    ++connectAttempts;

                    connectionStatus =
                        "Connecting " +
                        to_string(connectAttempts) + "/" +
                        to_string(maxConnectAttempts);

                    connectTask = std::async(
                        std::launch::async,
                        [&socket, hostIp]() -> Socket::Status {
                            // Only this task accesses the socket until
                            // the main thread collects its result.
                            socket.setBlocking(true);

                            const Socket::Status status =
                                socket.connect(hostIp, kPort, seconds(1));

                            if (status != Socket::Done) {
                                socket.disconnect();
                            }

                            return status;
                        }
                    );
                }
            }
        }

        // Update permission before processing this frame's input.
        myTurn =
            !connectionLost &&
            (!networked ||
                (connected && isBlackTurn == myColorIsBlack));
        const size_t prevStones = stonePositions.size();
        const int prevPass = consecutivePasses;
        const bool prevEnded = gameEnded;
        const bool prevTurn = isBlackTurn;

        handleMouseClick(
            window,
            view,
            stonePositions,
            isBlackTurn,
            consecutivePasses,
            gameEnded,
            history,
            !connectionLost && (!networked || myTurn),
            networked
        );

        if (!window.isOpen()) {
            break;
        }

        const bool changed =
            prevStones != stonePositions.size() ||
            prevPass != consecutivePasses ||
            prevEnded != gameEnded ||
            prevTurn != isBlackTurn;

        if (changed) {
            ++revision;

            if (networked && connected && !connectionLost) {
                enqueueState(
                    stonePositions,
                    isBlackTurn,
                    consecutivePasses,
                    gameEnded,
                    revision
                );
            }
        }

        if (networked && connected && !connectionLost) {
            const Socket::Status sendStatus = pumpSend(socket);

            Socket::Status receiveStatus = Socket::Done;

            if (sendStatus == Socket::Done) {
                receiveStatus = recvState(
                    socket,
                    recvBuffer,
                    stonePositions,
                    isBlackTurn,
                    consecutivePasses,
                    gameEnded,
                    history,
                    revision,
                    myColorIsBlack
                );
            }

            if (sendStatus != Socket::Done ||
                receiveStatus != Socket::Done) {
                connectionLost = true;
                connected = false;
                connectionStatus = "Disconnected";
                pendingStates.clear();
                socket.disconnect();

                cerr << "Connection ended or an invalid move was received. "
                    << "Restart both games to reconnect.\n";
            }
        }

        myTurn =
            !connectionLost &&
            (!networked ||
                (connected && isBlackTurn == myColorIsBlack));

        window.clear();
        window.setView(view);
        drawBoard(window);
        for (const auto &stone : stonePositions) {
            placeStone(window, stone.x, stone.y, stone.isBlack);
        }

        string title = "Go";

        if (networked) {
            title += myColorIsBlack ? " | You: Black" : " | You: White";
        } else {
            title += " | Local";
        }

        if (connectionLost) {
            title += " | " + connectionStatus;
        } else if (networked && !connected) {
            title += " | " + connectionStatus;
        } else if (gameEnded) {
            const ScoreResult scores = calculateScores(stonePositions);

            title += " | Ended";
            title += " | Raw area B:" + to_string(scores.black);
            title += " W:" + to_string(scores.white);
        } else {
            title += " | Turn: ";
            title += isBlackTurn ? "Black" : "White";

            if (networked) {
                title += myTurn ? " | Your move" : " | Waiting";
            }
        }

        if (title != lastTitle) {
            window.setTitle(title);
            lastTitle = title;
        }

        window.display();
    }
    return 0;
}