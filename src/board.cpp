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

void updateViewForWindow(RenderWindow& window, View& view) {
    const Vector2u size = window.getSize();

    if (size.x == 0 || size.y == 0) {
        return;
    }

    const float width = static_cast<float>(size.x);
    const float height = static_cast<float>(size.y);

    // Reserve space above the board for status and controls.
    const float header = min(statusPanelHeight, height * 0.3f);
    const float availableHeight = height - header;

    // The board always occupies a square viewport.
    const float side = min(width, availableHeight);

    const float left = (width - side) / 2.f;
    const float top = header + (availableHeight - side) / 2.f;

    view.setViewport(FloatRect(
        left / width,
        top / height,
        side / width,
        side / height
    ));

    window.setView(view);
}

FloatRect getPassButtonBounds(const RenderWindow& window) {
    const float width =
        static_cast<float>(window.getSize().x);

    return FloatRect(
        max(8.f, width - 76.f),
        8.f,
        68.f,
        28.f
    );
}

void drawStatusPanel(
    RenderWindow& window,
    const Font& font,
    const string& playerLine,
    const string& statusLine,
    const string& detailLine,
    bool canPass,
    const string& buttonLabel
) {
    const Vector2u size = window.getSize();

    if (size.x == 0 || size.y == 0) {
        return;
    }

    // Save the board view, then draw the UI in window-pixel coordinates.
    const View savedView = window.getView();

    window.setView(View(FloatRect(
        0.f,
        0.f,
        static_cast<float>(size.x),
        static_cast<float>(size.y)
    )));

    RectangleShape background(Vector2f(
        static_cast<float>(size.x),
        statusPanelHeight
    ));

    background.setFillColor(Color(28, 32, 38));
    window.draw(background);

    const FloatRect buttonBounds = getPassButtonBounds(window);

    // Fit long messages into their available horizontal space.
    auto drawLine = [&](
        const string& value,
        float y,
        unsigned int characterSize,
        Color color,
        float availableWidth
    ) {
        Text text(value, font, characterSize);
        text.setFillColor(color);

        FloatRect bounds = text.getLocalBounds();

        if (bounds.width > availableWidth &&
            bounds.width > 0.f) {
            const float scale =
                max(1.f, availableWidth) / bounds.width;

            text.setScale(scale, scale);
        }

        text.setPosition(8.f, y);
        window.draw(text);
    };

    const float textWidth =
        max(1.f, buttonBounds.left - 16.f);

    drawLine(
        playerLine,
        4.f,
        12,
        Color::White,
        textWidth
    );

    drawLine(
        statusLine,
        23.f,
        11,
        Color(160, 214, 246),
        textWidth
    );

    drawLine(
        detailLine,
        44.f,
        10,
        Color(205, 210, 218),
        max(1.f, static_cast<float>(size.x) - 16.f)
    );

    RectangleShape button(Vector2f(
        buttonBounds.width,
        buttonBounds.height
    ));

    button.setPosition(buttonBounds.left, buttonBounds.top);

    button.setFillColor(
        canPass
            ? Color(55, 112, 155)
            : Color(55, 60, 68)
    );

    window.draw(button);

    Text label(buttonLabel, font, 13);

    label.setFillColor(
        canPass
            ? Color::White
            : Color(145, 150, 158)
    );

    const FloatRect labelBounds = label.getLocalBounds();

    label.setPosition(
        buttonBounds.left +
            (buttonBounds.width - labelBounds.width) / 2.f -
            labelBounds.left,
        buttonBounds.top +
            (buttonBounds.height - labelBounds.height) / 2.f -
            labelBounds.top
    );

    window.draw(label);

    window.setView(savedView);
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

void beginScoreReview(ScoreReview& review, bool isBlackTurn) {
    review = ScoreReview{};
    review.active = true;
    review.resumeBlackTurn = isBlackTurn;
}

bool applyReviewAction(
    const vector<Stone>& stones,
    bool& isBlackTurn,
    int& consecutivePasses,
    bool& gameEnded,
    ScoreReview& review,
    char action,
    int x,
    int y
) {
    if (!gameEnded || !review.active || review.finalized) {
        return false;
    }

    // R: resume actual play without removing any stones.
    if (action == 'R') {
        isBlackTurn = review.resumeBlackTurn;
        consecutivePasses = 0;
        gameEnded = false;
        review = ScoreReview{};
        return true;
    }

    // A: accept the current dead-stone selection.
    if (action == 'A') {
        ++review.approvals;
        review.finalized = review.approvals == 2;
    } else if (action == 'T') {
        // T: toggle an entire connected group.
        if (x < 0 || y < 0 ||
            x >= boardSize || y >= boardSize) {
            return false;
        }

        const string board = serializeBoard(stones);
        const int start = y * boardSize + x;

        if (board[start] == '0') {
            return false;
        }

        const char color = board[start];
        const char mark =
            review.dead[start] == '1' ? '0' : '1';

        vector<bool> visited(boardSize * boardSize, false);
        queue<int> pending;

        pending.push(start);
        visited[start] = true;

        const int dx[] = {1, -1, 0, 0};
        const int dy[] = {0, 0, 1, -1};

        while (!pending.empty()) {
            const int position = pending.front();
            pending.pop();

            review.dead[position] = mark;

            for (int direction = 0; direction < 4; ++direction) {
                const int nx =
                    position % boardSize + dx[direction];

                const int ny =
                    position / boardSize + dy[direction];

                if (nx < 0 || ny < 0 ||
                    nx >= boardSize || ny >= boardSize) {
                    continue;
                }

                const int neighbor = ny * boardSize + nx;

                if (!visited[neighbor] &&
                    board[neighbor] == color) {
                    visited[neighbor] = true;
                    pending.push(neighbor);
                }
            }
        }

        // Any edit invalidates earlier acceptance.
        review.approvals = 0;
    } else {
        return false;
    }

    // Review actions alternate between the two players.
    isBlackTurn = !isBlackTurn;
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
    ScoreReview& review,
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

        if (!allowMove) {
            continue;
        }

        if (gameEnded && (!review.active || review.finalized)) {
            continue;
        }

        bool accepted = false;

        auto passOrAccept = [&]() {
            if (gameEnded) {
                return applyReviewAction(
                    stones,
                    isBlackTurn,
                    consecutivePasses,
                    gameEnded,
                    review,
                    'A'
                );
            }

            return tryPass(
                stones,
                isBlackTurn,
                consecutivePasses,
                gameEnded,
                history
            );
        };

        if (event.type == Event::KeyPressed) {
            if (event.key.code == Keyboard::P) {
                accepted = passOrAccept();
            } else if (
                event.key.code == Keyboard::R &&
                gameEnded
            ) {
                accepted = applyReviewAction(
                    stones,
                    isBlackTurn,
                    consecutivePasses,
                    gameEnded,
                    review,
                    'R'
                );
            }
        } else if (
            event.type == Event::MouseButtonPressed &&
            event.mouseButton.button == Mouse::Left
        ) {
            const Vector2i pixel(
                event.mouseButton.x,
                event.mouseButton.y
            );

            if (getPassButtonBounds(window).contains(
                    static_cast<float>(pixel.x),
                    static_cast<float>(pixel.y))) {
                accepted = passOrAccept();
            } else {
                if (!window.getViewport(view).contains(pixel)) {
                    continue;
                }

                const Vector2f position =
                    window.mapPixelToCoords(pixel, view);

                const int x = static_cast<int>(lround(
                    (position.x - halfCell) /
                    static_cast<float>(cellSize)
                ));

                const int y = static_cast<int>(lround(
                    (position.y - halfCell) /
                    static_cast<float>(cellSize)
                ));

                if (x < 0 || y < 0 ||
                    x >= boardSize || y >= boardSize) {
                    continue;
                }

                const float dx =
                    position.x - (halfCell + x * cellSize);

                const float dy =
                    position.y - (halfCell + y * cellSize);

                const float maxDistance = cellSize * 0.45f;

                if (dx * dx + dy * dy >
                    maxDistance * maxDistance) {
                    continue;
                }

                if (gameEnded) {
                    accepted = applyReviewAction(
                        stones,
                        isBlackTurn,
                        consecutivePasses,
                        gameEnded,
                        review,
                        'T',
                        x,
                        y
                    );
                } else {
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
            }
        }

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

FinalScore calculateFinalScore(
    const vector<Stone>& stones,
    const ScoreReview& review
) {
    vector<Stone> livingStones;

    for (const auto& stone : stones) {
        const int position = stone.y * boardSize + stone.x;

        if (!review.active || review.dead[position] != '1') {
            livingStones.push_back(stone);
        }
    }

    const ScoreResult area = calculateScores(livingStones);

    FinalScore result;
    result.black = static_cast<double>(area.black);
    result.white = static_cast<double>(area.white) + komi;

    return result;
}

void drawDeadMarkers(
    RenderWindow& window,
    const vector<Stone>& stones,
    const ScoreReview& review
) {
    if (!review.active) {
        return;
    }

    CircleShape marker(cellSize * 0.2f);
    marker.setFillColor(Color(235, 65, 65));

    for (const auto& stone : stones) {
        const int position = stone.y * boardSize + stone.x;

        if (review.dead[position] != '1') {
            continue;
        }

        marker.setPosition(
            halfCell + stone.x * cellSize - marker.getRadius(),
            halfCell + stone.y * cellSize - marker.getRadius()
        );

        window.draw(marker);
    }
}

char determineWinner(const ScoreResult &score) {
    if (score.black > score.white) return 'B';
    else if (score.white > score.black) return 'W';
    else return 'T'; // tie
}