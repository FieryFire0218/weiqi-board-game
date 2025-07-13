#include "stone.hpp"
using namespace std;
using namespace sf;

void placeStone(RenderWindow &window, int x, int y, bool isBlack) {
    extern const int cellSize;
    extern const int halfCell;
    CircleShape stone(1.0 * cellSize / 2.5);
    if (isBlack) {
        stone.setFillColor(Color::Black);
    } else {
        stone.setFillColor(Color::White);
    }
    stone.setOutlineColor(Color(128, 128, 128));
    stone.setOutlineThickness(-2);

    stone.setPosition(halfCell + x * cellSize - stone.getRadius(), halfCell + y * cellSize - stone.getRadius());
    window.draw(stone);
}