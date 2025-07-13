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
void handleMouseClick(RenderWindow &window, vector<Stone> &stonePositions, bool &isBlackTurn);