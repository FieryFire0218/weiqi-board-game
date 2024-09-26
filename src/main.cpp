#include <SFML/Graphics.hpp>
using namespace std;
using namespace sf;

int cellSize = 40;

int main(void)
{
    RenderWindow window(VideoMode(cellSize * 19, cellSize * 19), "Go/Weiqi/Baduk");

    window.setPosition(Vector2i(250, 80));

    window.setSize(Vector2u(800, 600));

    View view(FloatRect(0, 0, cellSize * 19, cellSize * 19));
    window.setView(view);

    while (window.isOpen())
    {
        Event event;
        while (window.pollEvent(event))
        {
            if (event.type == Event::Closed)
                window.close();
        }

        window.clear(Color::Yellow);
        auto drawBoard = [&]() 
        {
            float halfCell = 1.0 * cellSize / 2;
            for (int y = 0; y < 19; y++) 
            {
                Vertex hline[] = {
                    Vertex(Vector2f(halfCell, halfCell + y * cellSize)),
                    Vertex(Vector2f(cellSize * 19 - halfCell, halfCell + y * cellSize))
                };
                hline[0].color = Color::Black;
                hline[1].color = Color::Black;
                window.draw(hline, 2, Lines);
            } 
            for (int x = 0; x < 19; x++) {
                Vertex vline[] = {
                    Vertex(Vector2f(halfCell + x * cellSize, halfCell)),
                    Vertex(Vector2f(halfCell + x * cellSize, cellSize * 19 - halfCell))
                };
                vline[0].color = Color::Black;
                vline[1].color = Color::Black;
                window.draw(vline, 2, Lines);
            }
            //Start points
            float startR = cellSize / 6;
            CircleShape circle(startR);
            circle.setFillColor(Color::Black);
            for (int y = 0; y < 3; y++) 
                for (int x = 0; x < 3; x++) {
                    circle.setPosition(halfCell + (3+6*x) * cellSize - startR, halfCell + (3+6*y) * cellSize - startR);
                    window.draw(circle);
                }
        };
        drawBoard();

        auto drawStones = [&]() {
            CircleShape stone(1.0 *cellSize / 2);
            stone.setFillColor(Color::White);
            stone.setOutlineColor(Color::Black);
            stone.setOutlineThickness(-2);
            
            stone.setPosition(0, 0);
            window.draw(stone);
        };
        drawStones();

        window.display();
    }

    return 0;
}