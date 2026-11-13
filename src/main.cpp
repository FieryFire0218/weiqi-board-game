#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <vector>
#include "board.hpp"
#include "stone.hpp"
#include <string>
#include <iostream>
#include <sstream>
#include <deque>
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

int main(int argc, char** argv) {
    ContextSettings settings;
    settings.antialiasingLevel = 4;
    RenderWindow window(VideoMode(400, 400), "Go/Weiqi/Baduk", Style::Default, settings);
    window.setPosition(Vector2i(300, 10));
    window.setVerticalSyncEnabled(true);
    window.setKeyRepeatEnabled(false);

    bool networked = false;
    bool isHost = false;
    TcpListener listener;
    TcpSocket socket;
    string recvBuffer;

    if (argc >= 2) {
        string mode = argv[1];
        if (mode == "--host") {
            isHost = true;
            if (listener.listen(kPort) != Socket::Done) {
                cerr << "Failed to listen on port " << kPort << "\n";
                return 1;
            }
            cout << "Waiting for peer on port " << kPort << "...\n";
            if (listener.accept(socket) != Socket::Done) {
                cerr << "Failed to accept connection\n";
                return 1;
            }
            socket.setBlocking(false);
            networked = true;
            cout << "Client connected\n";
        } else if (mode == "--join" && argc >= 3) {
            IpAddress hostIp(argv[2]);
            socket.setBlocking(true);
            const int maxTries = 20;
            bool connected = false;
            for (int i = 0; i < maxTries; ++i) {
                if (socket.connect(hostIp, kPort, seconds(1)) == Socket::Done) {
                    connected = true;
                    break;
                }
                sleep(milliseconds(500));
            }
            if (!connected) {
                cerr << "Failed to connect to host after retries\n";
                return 1;
            }
            socket.setBlocking(false);
            networked = true;
            isHost = false;
            cout << "Connected to host\n";
        }
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
    bool myTurn = !networked || isHost;
    const bool myColorIsBlack = !networked || isHost;

    while (window.isOpen()) {
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

            if (networked && !connectionLost) {
                enqueueState(
                    stonePositions,
                    isBlackTurn,
                    consecutivePasses,
                    gameEnded,
                    revision
                );
            }
        }

        if (networked && !connectionLost) {
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
                pendingStates.clear();
                socket.disconnect();

                cerr << "Connection ended or an invalid move was received. "
                    << "Restart both games to reconnect.\n";
            }
        }

        myTurn =
            !connectionLost &&
            (!networked || isBlackTurn == myColorIsBlack);

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
            title += " | Disconnected";
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