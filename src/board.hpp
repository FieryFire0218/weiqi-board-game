#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include "stone.hpp"
using namespace std;
using namespace sf;

extern const int cellSize;
extern const int halfCell;
extern const int boardSize;

bool isStarPoint(int x, int y);
void drawBoard(RenderWindow &window);
void checkAndCaptureStones(vector<Stone> &stonePositions, int x, int y);
View createBoardView();
void updateViewForWindow(RenderWindow &window, View &view);
void drawTurnIndicator(RenderWindow &window, bool isBlackTurn, bool gameEnded);
void handleMouseClick(RenderWindow &window, View &view, vector<Stone> &stonePositions, bool &isBlackTurn, int &consecutivePasses, bool &gameEnded);

struct ScoreResult {
    int black; // B stones + territory
    int white; // W stones + territory
};

ScoreResult calculateScores(const vector<Stone> &stonePositions);

char determineWinner(const ScoreResult &score);