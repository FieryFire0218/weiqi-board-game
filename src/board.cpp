#include "board.hpp"
#include <queue>
#include <set>
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
                std::remove_if(stonePositions.begin(), stonePositions.end(),
                    [&](const Stone &s) { return s.x == pos.first && s.y == pos.second; }),
                stonePositions.end()
            );
        }
    }
}

void handleMouseClick(RenderWindow &window, vector<Stone> &stonePositions, bool &isBlackTurn) {
    Event event;
    while (window.pollEvent(event)) {
        if (event.type == Event::Closed) {
            window.close();
        } else if (event.type == Event::MouseButtonPressed) {
            if (event.mouseButton.button == Mouse::Left) {
                Vector2i mousePos = Mouse::getPosition(window);
                int x = mousePos.x / cellSize;
                int y = mousePos.y / cellSize;

                bool stoneAlreadyExists = false;
                for (const auto &stone : stonePositions) {
                    if (stone.x == x && stone.y == y) {
                        stoneAlreadyExists = true;
                        break;
                    }
                }

                if (!stoneAlreadyExists) {
                    Stone stone;
                    stone.x = x;
                    stone.y = y;
                    stone.isBlack = isBlackTurn;
                    stonePositions.push_back(stone);

                    // Check for captures for both colors
                    int dx[] = {1, -1, 0, 0};
                    int dy[] = {0, 0, 1, -1};
                    for (int d = 0; d < 4; ++d) {
                        int nx = x + dx[d], ny = y + dy[d];
                        Stone* neighbor = getStoneAt(stonePositions, nx, ny);
                        if (neighbor && neighbor->isBlack != isBlackTurn) {
                            checkAndCaptureStones(stonePositions, nx, ny);
                        }
                    }
                    // Also check self-capture (suicide)
                    checkAndCaptureStones(stonePositions, x, y);

                    isBlackTurn = !isBlackTurn;
                }
            }
        }
    }
}