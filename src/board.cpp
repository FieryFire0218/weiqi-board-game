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
        RectangleShape line(Vector2f((boardSize - 1) * cellSize, 1.f));
        line.setFillColor(lineColor);
        line.setPosition(halfCell, halfCell + j * cellSize);
        window.draw(line);
    }
    // Vertical lines
    for (int i = 0; i < boardSize; ++i) {
        RectangleShape line(Vector2f(1.f, (boardSize - 1) * cellSize));
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

void handleMouseClick(RenderWindow &window, View &view, vector<Stone> &stonePositions, bool &isBlackTurn, int &consecutivePasses, bool &gameEnded, bool allowMove) {
    static string prevKey; // S_{t-1}
    static string currKey; // S_t
    Event event;
    while (window.pollEvent(event)) {
        if (event.type == Event::Closed) {
            window.close();
        } else if (event.type == Event::Resized) {
            updateViewForWindow(window, view);
        } else if (!allowMove) {
            continue;
        } else if (event.type == Event::KeyPressed) {
            if (event.key.code == Keyboard::P) {
                if (gameEnded) continue;
                if (currKey.empty()) {
                    string init = serializeBoard(stonePositions);
                    prevKey = init; 
                    currKey = init; 
                }
                prevKey = currKey;
                isBlackTurn = !isBlackTurn;
                ++consecutivePasses;
                if (consecutivePasses >= 2) {
                    gameEnded = true;
                }
                continue;
            }
        } else if (event.type == Event::MouseButtonPressed) {
            if (event.mouseButton.button == Mouse::Left) {
                if (gameEnded) continue;
                Vector2i mousePosPx = Mouse::getPosition(window);
                Vector2f mousePos = window.mapPixelToCoords(mousePosPx);

                float gx = (mousePos.x - halfCell) / static_cast<float>(cellSize);
                float gy = (mousePos.y - halfCell) / static_cast<float>(cellSize);
                int x = static_cast<int>(lround(gx));
                int y = static_cast<int>(lround(gy));
                if (x < 0 || y < 0 || x >= boardSize || y >= boardSize) {
                    continue;
                }

                float centerX = halfCell + x * cellSize;
                float centerY = halfCell + y * cellSize;
                float dx = mousePos.x - centerX;
                float dy = mousePos.y - centerY;
                float dist2 = dx*dx + dy*dy;
                float maxDist = cellSize * 0.45f; // ~half a cell
                if (dist2 > maxDist * maxDist) {
                    // Too far from intersection, ignore
                    continue;
                }

                if (getStoneAt(stonePositions, x, y)) {
                    continue;
                }

                if (currKey.empty()) {
                    string init = serializeBoard(stonePositions);
                    prevKey = init; // S_{t-1} == S_t at start
                    currKey = init; // S_t
                }

                // Simulate the move
                vector<Stone> temp = stonePositions;
                temp.push_back(Stone{x, y, isBlackTurn});

                // Capture any adjacent opponent groups with no liberties
                int offX[] = {1, -1, 0, 0};
                int offY[] = {0, 0, 1, -1};
                for (int d = 0; d < 4; ++d) {
                    int nx = x + offX[d], ny = y + offY[d];
                    Stone* neighbor = getStoneAt(temp, nx, ny);
                    if (neighbor && neighbor->isBlack != isBlackTurn) {
                        checkAndCaptureStones(temp, nx, ny);
                    }
                }

                // Suicide check
                checkAndCaptureStones(temp, x, y);
                if (!getStoneAt(temp, x, y)) {
                    // Suicide -> reject, do not toggle turn
                    continue;
                }

                // Ko check
                string newKey = serializeBoard(temp);
                if (newKey == prevKey) {
                    continue;
                }

                // Accept move
                stonePositions.swap(temp);
                prevKey = currKey;
                currKey = newKey;
                isBlackTurn = !isBlackTurn;
                consecutivePasses = 0;
            }
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