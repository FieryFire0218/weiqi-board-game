#include "board.hpp"
#include <queue>
#include <set>
#include <algorithm>
#include <string>
#include <cmath>
using namespace std;
using namespace sf;

const int cellSize = 35;
const int halfCell = cellSize / 2;
const int boardSize = 19;

bool isStarPoint(int x, int y) {
    vector<pair<int, int>> starPoints = {
        {3, 3}, {3, 9}, {3, 15},
        {9, 3}, {9, 9}, {9, 15},
        {15, 3}, {15, 9}, {15, 15}
    };
    for (const auto &point : starPoints) {
        if (point.first == x && point.second == y) {
            return true;
        }
    }
    return false;
}

void drawBoard(RenderWindow &window) {
    // Board background
    RectangleShape bg(Vector2f(boardSize * cellSize, boardSize * cellSize));
    bg.setFillColor(Color(181, 136, 99));
    bg.setPosition(0.f, 0.f);
    window.draw(bg);

    // Grid lines 
    Color lineColor = Color::Black;

    // Horizontal lines
    for (int j = 0; j < boardSize; ++j) {
        RectangleShape line(Vector2f((boardSize - 1) * cellSize, 2.f));
        line.setFillColor(lineColor);
        line.setPosition(halfCell, halfCell + j * cellSize);
        window.draw(line);
    }
    // Vertical lines
    for (int i = 0; i < boardSize; ++i) {
        RectangleShape line(Vector2f(2.f, (boardSize - 1) * cellSize));
        line.setFillColor(lineColor);
        line.setPosition(halfCell + i * cellSize, halfCell);
        window.draw(line);
    }

    // Star points
    CircleShape starPoint(0.35f * cellSize / 2.5f);
    starPoint.setFillColor(Color::Black);
    starPoint.setOutlineThickness(0); 
    for (int i = 0; i < boardSize; i++) {
        for (int j = 0; j < boardSize; j++) {
            if (isStarPoint(i, j)) {
                starPoint.setPosition(
                    halfCell + i * cellSize - starPoint.getRadius(),
                    halfCell + j * cellSize - starPoint.getRadius()
                );
                window.draw(starPoint);
            }
        }
    }
}

View createBoardView() {
    float size = static_cast<float>(boardSize * cellSize); // square logical board
    View view(FloatRect(0.f, 0.f, size, size));
    view.setCenter(size / 2.f, size / 2.f);
    return view;
}

void updateViewForWindow(RenderWindow &window, View &view) {
    Vector2u ws = window.getSize();
    float windowRatio = static_cast<float>(ws.x) / static_cast<float>(ws.y);
    float viewRatio = view.getSize().x / view.getSize().y;

    FloatRect viewport;
    if (windowRatio > viewRatio) {
        float width = viewRatio / windowRatio;
        viewport = FloatRect((1.f - width) / 2.f, 0.f, width, 1.f);
    } else {
        float height = windowRatio / viewRatio;
        viewport = FloatRect(0.f, (1.f - height) / 2.f, 1.f, height);
    }
    view.setViewport(viewport);
    window.setView(view);
}

static Stone* getStoneAt(vector<Stone> &stonePositions, int x, int y) {
    for (auto &stone : stonePositions) {
        if (stone.x == x && stone.y == y) {
            return &stone;
        }
    }
    return nullptr;
}

string serializeBoard(const vector<Stone> &stones) {
    string key;
    key.assign(boardSize * boardSize, '0'); 
    for (const auto &s : stones) {
        if (s.x >= 0 && s.x < boardSize && s.y >= 0 && s.y < boardSize) {
            key[s.y * boardSize + s.x] = s.isBlack ? 'B' : 'W';
        }
    }
    return key;
}

void deserializeBoard(const string &key, vector<Stone> &stones) {
    stones.clear();
    if (static_cast<int>(key.size()) < boardSize * boardSize) return;
    for (int y = 0; y < boardSize; ++y) {
        for (int x = 0; x < boardSize; ++x) {
            char c = key[y * boardSize + x];
            if (c == 'B' || c == 'W') {
                stones.push_back(Stone{x, y, c == 'B'});
            }
        }
    }
}

void checkAndCaptureStones(vector<Stone> &stonePositions, int x, int y) {
    Stone* start = getStoneAt(stonePositions, x, y);
    if (!start) return;

    bool isBlack = start->isBlack;
    queue<pair<int, int>> q;
    set<pair<int, int>> visited;
    vector<pair<int, int>> group;
    bool hasLiberty = false;

    q.push({x, y});
    visited.insert({x, y});

    int dx[] = {1, -1, 0, 0};
    int dy[] = {0, 0, 1, -1};

    while (!q.empty()) {
        auto [cx, cy] = q.front(); q.pop();
        group.push_back({cx, cy});
        for (int d = 0; d < 4; ++d) {
            int nx = cx + dx[d], ny = cy + dy[d];
            if (nx < 0 || ny < 0 || nx >= boardSize || ny >= boardSize) continue;
            if (visited.count({nx, ny})) continue;
            Stone* neighbor = getStoneAt(stonePositions, nx, ny);
            if (!neighbor) {
                hasLiberty = true;
            } else if (neighbor->isBlack == isBlack) {
                q.push({nx, ny});
                visited.insert({nx, ny});
            }
        }
    }

    if (!hasLiberty) {
        // Remove all stones in group
        for (auto &pos : group) {
            stonePositions.erase(
                remove_if(stonePositions.begin(), stonePositions.end(),
                    [&](const Stone &s) { return s.x == pos.first && s.y == pos.second; }),
                stonePositions.end()
            );
        }
    }
}

bool tryPlaceStone(
    vector<Stone>& stones,
    bool& isBlackTurn,
    int& consecutivePasses,
    bool& gameEnded,
    RuleHistory& history,
    int x,
    int y
) {
    if (gameEnded) return false;

    if (x < 0 || y < 0 || x >= boardSize || y >= boardSize) {
        return false;
    }

    if (getStoneAt(stones, x, y)) {
        return false;
    }

    const string before = serializeBoard(stones);

    // Validate on a copy so rejected moves cannot alter the real board.
    vector<Stone> next = stones;
    next.push_back(Stone{x, y, isBlackTurn});

    const int offX[] = {1, -1, 0, 0};
    const int offY[] = {0, 0, 1, -1};

    for (int d = 0; d < 4; ++d) {
        const int nx = x + offX[d];
        const int ny = y + offY[d];

        Stone* neighbor = getStoneAt(next, nx, ny);

        if (neighbor && neighbor->isBlack != isBlackTurn) {
            checkAndCaptureStones(next, nx, ny);
        }
    }

    // Reject suicide after resolving opponent captures.
    checkAndCaptureStones(next, x, y);

    if (!getStoneAt(next, x, y)) {
        return false;
    }

    const string nextBoard = serializeBoard(next);

    // Simple ko: cannot recreate the position before the last action
    if (!history.previousBoard.empty() &&
        nextBoard == history.previousBoard) {
        return false;
    }

    history.previousBoard = before;
    stones.swap(next);
    isBlackTurn = !isBlackTurn;
    consecutivePasses = 0;

    return true;
}

bool tryPass(
    const vector<Stone>& stones,
    bool& isBlackTurn,
    int& consecutivePasses,
    bool& gameEnded,
    RuleHistory& history
) {
    if (gameEnded) return false;

    history.previousBoard = serializeBoard(stones);
    isBlackTurn = !isBlackTurn;
    ++consecutivePasses;
    gameEnded = consecutivePasses >= 2;

    return true;
}

void handleMouseClick(
    RenderWindow& window,
    View& view,
    vector<Stone>& stones,
    bool& isBlackTurn,
    int& consecutivePasses,
    bool& gameEnded,
    RuleHistory& history,
    bool allowMove,
    bool singleMove
) {
    Event event;

    while (window.pollEvent(event)) {
        if (event.type == Event::Closed) {
            window.close();
            continue;
        }

        if (event.type == Event::Resized) {
            updateViewForWindow(window, view);
            continue;
        }

        // Still process window events when moves are disabled.
        if (!allowMove || gameEnded) {
            continue;
        }

        bool accepted = false;

        if (event.type == Event::KeyPressed &&
            event.key.code == Keyboard::P) {
            accepted = tryPass(
                stones,
                isBlackTurn,
                consecutivePasses,
                gameEnded,
                history
            );
        } else if (
            event.type == Event::MouseButtonPressed &&
            event.mouseButton.button == Mouse::Left
        ) {
            const Vector2i pixel(
                event.mouseButton.x,
                event.mouseButton.y
            );

            // Ignore clicks in any letterboxed area outside the board view.
            if (!window.getViewport(view).contains(pixel)) {
                continue;
            }

            const Vector2f position =
                window.mapPixelToCoords(pixel, view);

            const float gx =
                (position.x - halfCell) / static_cast<float>(cellSize);

            const float gy =
                (position.y - halfCell) / static_cast<float>(cellSize);

            const int x = static_cast<int>(lround(gx));
            const int y = static_cast<int>(lround(gy));

            if (x < 0 || y < 0 || x >= boardSize || y >= boardSize) {
                continue;
            }

            const float centerX = halfCell + x * cellSize;
            const float centerY = halfCell + y * cellSize;

            const float dx = position.x - centerX;
            const float dy = position.y - centerY;
            const float maxDistance = cellSize * 0.45f;

            if (dx * dx + dy * dy > maxDistance * maxDistance) {
                continue;
            }

            accepted = tryPlaceStone(
                stones,
                isBlackTurn,
                consecutivePasses,
                gameEnded,
                history,
                x,
                y
            );
        }

        // In multiplayer, one successful action ends this player's turn.
        // Remaining events may close/resize the window but cannot play again.
        if (accepted && singleMove) {
            allowMove = false;
        }
    }
}

ScoreResult calculateScores(const vector<Stone> &stonePositions) {
    // 0 = empty, 1 = black, 2 = white
    vector<vector<int>> occ(boardSize, vector<int>(boardSize, 0));
    int blackStones = 0, whiteStones = 0;

    for (const auto &s : stonePositions) {
        if (s.x < 0 || s.y < 0 || s.x >= boardSize || s.y >= boardSize) continue;
        occ[s.y][s.x] = s.isBlack ? 1 : 2;
        if (s.isBlack) ++blackStones; else ++whiteStones;
    }

    vector<vector<uint8_t>> visited(boardSize, vector<uint8_t>(boardSize, 0));
    int blackTerritory = 0, whiteTerritory = 0;

    auto inBounds = [&](int x, int y) {
        return x >= 0 && y >= 0 && x < boardSize && y < boardSize;
    };

    int dx[4] = {1, -1, 0, 0};
    int dy[4] = {0, 0, 1, -1};

    for (int y = 0; y < boardSize; ++y) {
        for (int x = 0; x < boardSize; ++x) {
            if (occ[y][x] != 0 || visited[y][x]) continue;

            // BFS over empty region
            queue<pair<int,int>> q;
            q.push({x, y});
            visited[y][x] = 1;

            int regionSize = 0;
            bool adjBlack = false, adjWhite = false;

            while (!q.empty()) {
                auto [cx, cy] = q.front(); q.pop();
                ++regionSize;

                for (int d = 0; d < 4; ++d) {
                    int nx = cx + dx[d], ny = cy + dy[d];
                    if (!inBounds(nx, ny)) continue;

                    if (occ[ny][nx] == 0) {
                        if (!visited[ny][nx]) {
                            visited[ny][nx] = 1;
                            q.push({nx, ny});
                        }
                    } else if (occ[ny][nx] == 1) {
                        adjBlack = true;
                    } else if (occ[ny][nx] == 2) {
                        adjWhite = true;
                    }
                }
            }

            if (adjBlack && !adjWhite) blackTerritory += regionSize;
            else if (adjWhite && !adjBlack) whiteTerritory += regionSize;
            // mixed adjacency = neutral, ignored
        }
    }

    ScoreResult res;
    res.black = blackStones + blackTerritory;
    res.white = whiteStones + whiteTerritory;
    return res;
}

char determineWinner(const ScoreResult &score) {
    if (score.black > score.white) return 'B';
    else if (score.white > score.black) return 'W';
    else return 'T'; // tie
}