#include <SFML/Graphics.hpp>
#include <vector>
#include "board.hpp"
#include "stone.hpp"
using namespace std;
using namespace sf;

int main() {
    RenderWindow window(VideoMode(1450, 1500), "Go/Weiqi/Baduk");
    window.setPosition(Vector2i(500, 20));

    vector<Stone> stonePositions;
    bool isBlackTurn = true;

    while (window.isOpen()) {
        handleMouseClick(window, stonePositions, isBlackTurn);
        window.clear();
        drawBoard(window);
        for (const auto &stone : stonePositions) {
            placeStone(window, stone.x, stone.y, stone.isBlack);
        }
        window.display();
    }
    return 0;
}