# Weiqi Board Game

A simple Go/Weiqi/Baduk board game rendered with SFML.

## Features

- 19×19 board with grid and star points ([src/board.cpp](src/board.cpp), [`drawBoard`](src/board.hpp))
- Stone rendering ([src/stone.cpp](src/stone.cpp), [`placeStone`](src/stone.hpp))
- Turn-based mouse input with snapping to nearest intersection ([src/board.cpp](src/board.cpp), [`handleMouseClick`](src/board.hpp))
- Capture resolution via group/liberty check ([src/board.cpp](src/board.cpp), [`checkAndCaptureStones`](src/board.hpp))
- No-suicide rule: illegal self-capture moves are rejected; turn does not toggle ([src/board.cpp](src/board.cpp))
- Basic ko (immediate ko): prevents recreating the position from two plies ago ([src/board.cpp](src/board.cpp))

Entry point: [src/main.cpp](src/main.cpp)

## Build and Run

- With Makefile:
  - Build: `make`
  - Run: `make run`
  - Clean: `make clean`

- Without Makefile (manual):
  ```
  g++ src/main.cpp src/board.cpp src/stone.cpp -o bin/sfml-app -lsfml-graphics -lsfml-window -lsfml-system
  ./bin/sfml-app
  ```

## Controls

- Left-click: place a stone on the nearest intersection (must be within a tolerance of the intersection center)
- P: pass. The game ends after two consecutive passes.
- Window close button: exit

## Project Structure

- [src/board.hpp](src/board.hpp): declarations for board drawing and rules
- [src/board.cpp](src/board.cpp): board rendering, input handling, ko/no-suicide, capture logic
- [src/stone.hpp](src/stone.hpp) / [src/stone.cpp](src/stone.cpp): stone rendering
- [src/main.cpp](src/main.cpp): main loop and rendering
- [docs/plan.txt](docs/plan.txt): roadmap
- [Makefile](Makefile): build script
- Output: [bin/sfml-app](bin/sfml-app)

## Roadmap (not yet implemented)

From [docs/plan.txt](docs/plan.txt):
1. Undo/redo (maintain move history; apply/revert captures)
2. Board coordinates on the side of the window
3. Clear board / start new game
4. Handicap stones
5. In-game menu (new game, undo/redo, pass, settings)
6. Multiplayer via sockets (local network or internet; consider SFML-network or BSD sockets)
7. KataGo API for AI analysis

## Notes on Scoring (planned)

- Basic area scoring can be implemented via flood-fill of empty regions:
  - If an empty region is adjacent only to black stones, it is black territory; only white stones → white territory; mixed → neutral.
  - Score = stones on board + territory; add komi to white.
- Alternatively, integration with a GTP engine (e.g., KataGo) for robust scoring and analysis, at the cost of an external dependency.

## License

MIT License. See [LICENSE](LICENSE).