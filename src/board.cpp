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
    RectangleShape cell(Vector2f(cellSize, cellSize));
    cell.setFillColor(Color(181, 136, 99));
    cell.setOutlineColor(Color::Black);
    cell.setOutlineThickness(1);

    CircleShape starPoint(0.35 * cellSize / 2.5);
    starPoint.setFillColor(Color::Black);
    starPoint.setOutlineColor(Color::Black);
    starPoint.setOutlineThickness(-2);

    for (int i = 0; i < boardSize; i++) {
        for (int j = 0; j < boardSize; j++) {
            cell.setPosition(halfCell + i * cellSize, halfCell + j * cellSize);
            window.draw(cell);

            if (isStarPoint(i, j)) {
                starPoint.setPosition(halfCell + i * cellSize - starPoint.getRadius(), halfCell + j * cellSize - starPoint.getRadius());
                window.draw(starPoint);
            }
        }
    }
}

static Stone* getStoneAt(vector<Stone> &stonePositions, int x, int y) {
    for (auto &stone : stonePositions) {
        if (stone.x == x && stone.y == y) {
            return &stone;
        }
    }
    return nullptr;
}

static string boardKey(const vector<Stone> &stones) {
    string key;
    key.assign(boardSize * boardSize, '0'); // 0 = empty, B = black, W = white
    for (const auto &s : stones) {
        if (s.x >= 0 && s.x < boardSize && s.y >= 0 && s.y < boardSize) {
            key[s.y * boardSize + s.x] = s.isBlack ? 'B' : 'W';
        }
    }
    return key;
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

void handleMouseClick(RenderWindow &window, vector<Stone> &stonePositions, bool &isBlackTurn) {
    static string prevKey; // S_{t-1}
    static string currKey; // S_t
    Event event;
    while (window.pollEvent(event)) {
        if (event.type == Event::Closed) {
            window.close();
        } else if (event.type == Event::MouseButtonPressed) {
            if (event.mouseButton.button == Mouse::Left) {
                Vector2i mousePos = Mouse::getPosition(window);

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
                    string init = boardKey(stonePositions);
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
                string newKey = boardKey(temp);
                if (newKey == prevKey) {
                    continue;
                }

                // Accept move
                stonePositions.swap(temp);
                prevKey = currKey;
                currKey = newKey;
                isBlackTurn = !isBlackTurn;
            }
        }
    }
}