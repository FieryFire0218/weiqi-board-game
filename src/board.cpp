#include "board.hpp"
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
                    isBlackTurn = !isBlackTurn;
                }
            }
        }
    }
}