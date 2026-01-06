#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <vector>
#include "board.hpp"
#include "stone.hpp"
#include <string>
#include <iostream>
#include <sstream>
using namespace std;
using namespace sf;

static const unsigned short kPort = 53000;

static string pendingState;
static void enqueueState(const vector<Stone> &stones, bool isBlackTurn, int consecutivePasses, bool gameEnded) {
    pendingState = "STATE " + serializeBoard(stones) + " " +
                   (isBlackTurn ? "1" : "0") + " " +
                   to_string(consecutivePasses) + " " +
                   (gameEnded ? "1" : "0") + "\n";
}

static void pumpSend(TcpSocket &sock) {
    while (!pendingState.empty()) {
        size_t sent = 0;
        Socket::Status st = sock.send(pendingState.data(), pendingState.size(), sent);
        if (st == Socket::Done || st == Socket::Partial) {
            pendingState.erase(0, sent);
        } else if (st == Socket::NotReady) {
            break;
        } else {
            pendingState.clear();
            break;
        }
    }
}

static bool recvState(TcpSocket &sock, string &buffer, vector<Stone> &stones, bool &isBlackTurn, int &consecutivePasses, bool &gameEnded) {
    char data[2048];
    size_t received = 0;
    if (sock.receive(data, sizeof(data), received) != Socket::Done) return false;
    buffer.append(data, received);
    size_t pos;
    while ((pos = buffer.find('\n')) != string::npos) {
        string line = buffer.substr(0, pos);
        buffer.erase(0, pos + 1);
        if (line.rfind("STATE ", 0) == 0) {
            string key; string turnStr; string passStr; string endedStr;
            istringstream iss(line.substr(6));
            iss >> key >> turnStr >> passStr >> endedStr;
            if (!key.empty()) {
                deserializeBoard(key, stones);
                isBlackTurn = (turnStr == "1");
                consecutivePasses = stoi(passStr);
                gameEnded = (endedStr == "1");
                return true;
            }
        }
    }
    return false;
}

int main(int argc, char** argv) {
    ContextSettings settings;
    settings.antialiasingLevel = 4;
    RenderWindow window(VideoMode(700, 700), "Go/Weiqi/Baduk", Style::Default, settings);
    window.setPosition(Vector2i(300, 10));
    window.setVerticalSyncEnabled(true);

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
    string lastTitle;
    bool myTurn = !networked || isHost;
    const bool myColorIsBlack = !networked || isHost;

    while (window.isOpen()) {
        size_t prevStones = stonePositions.size();
        int prevPass = consecutivePasses;
        bool prevEnded = gameEnded;
        bool prevTurn = isBlackTurn;

        handleMouseClick(window, view, stonePositions, isBlackTurn, consecutivePasses, gameEnded, myTurn);

        if (networked &&
            (prevStones != stonePositions.size() ||
             prevPass != consecutivePasses ||
             prevEnded != gameEnded ||
             prevTurn != isBlackTurn)) {
            enqueueState(stonePositions, isBlackTurn, consecutivePasses, gameEnded);
            myTurn = (isBlackTurn == myColorIsBlack);
        }

        if (networked) {
            pumpSend(socket);
            if (recvState(socket, recvBuffer, stonePositions, isBlackTurn, consecutivePasses, gameEnded)) {
                myTurn = (isBlackTurn == myColorIsBlack);
            }
        }

        window.clear();
        drawBoard(window);
        for (const auto &stone : stonePositions) {
            placeStone(window, stone.x, stone.y, stone.isBlack);
        }

        ScoreResult scores = calculateScores(stonePositions);
        char winner = determineWinner(scores);
        string title =
            "Go/Weiqi/Baduk — B: " + to_string(scores.black) +
            "  W: " + to_string(scores.white) +
            "  Winner: " + (winner == 'T' ? string("Tie") : (winner == 'B' ? string("Black") : string("White")));
        if (!gameEnded) {
            title += "  Turn: " + string(isBlackTurn ? "Black" : "White");
        } else {
            title += "  Ended";
        }
        if (title != lastTitle) {
            window.setTitle(title);
            lastTitle = title;
        }

        window.display();
    }
    return 0;
}