#pragma once

#include <SFML/Graphics.hpp>
using namespace std;
using namespace sf;

struct Stone {
    int x, y;
    bool isBlack;
};

void placeStone(RenderWindow &window, int x, int y, bool isBlack);