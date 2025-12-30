#include <SFML/Graphics.hpp>
#include <vector>
#include "board.hpp"
#include "stone.hpp"
#include <string>
using namespace std;
using namespace sf;

int main() {
    ContextSettings settings;
    settings.antialiasingLevel = 4;
    RenderWindow window(VideoMode(700, 700), "Go/Weiqi/Baduk", Style::Default, settings);
    window.setPosition(Vector2i(300, 10));
    window.setVerticalSyncEnabled(true);

    View view = createBoardView();
    updateViewForWindow(window, view);

    vector<Stone> stonePositions;
    bool isBlackTurn = true;
    int consecutivePasses = 0;
    bool gameEnded = false;
    string lastTitle;

    while (window.isOpen()) {
        handleMouseClick(window, view, stonePositions, isBlackTurn, consecutivePasses, gameEnded);
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