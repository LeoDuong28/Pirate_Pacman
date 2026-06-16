#include <SDL2/SDL.h>
#include <cstdio>
#include "Game.h"

int main(int /*argc*/, char** /*argv*/) {
    Game game;
    if (!game.init()) {
        printf("Failed to initialize game.\n");
        return 1;
    }
    game.run();
    return 0;
}
