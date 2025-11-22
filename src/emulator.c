#include "memory.h"
#include "renderer.h"
#include "common.h"

int main()
{
    InitEmulatorWindow();

    SetTargetFPS(60);

    while(!WindowShouldClose())
    {
        BeginDrawing();

        ClearBackground(WHITE);

        EndDrawing();
    }

    CloseEmulatorWindow();

    return 0;
}