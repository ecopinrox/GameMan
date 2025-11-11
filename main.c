#include <stdio.h>
#include "memory.h"
#include "renderer.h"

int main()
{
    const int screenWidth = 160;
    const int screenHeight = 144;

    //window tile from Pokemon R/B
    uint8_t tiles[16] = {0xFF, 0x00, 0x7E, 0xFF, 0x85, 0x81, 0x89, 0x83, 0x93, 0x85, 0xA5, 0x8B, 0xC9, 0x97, 0x7E, 0xFF};

    InitGBScreen();

    while(!WindowShouldClose())
    {
        BeginDrawing();

        ClearBackground(WHITE);
        for(int i = 0; i < 20; i++)
        {
            for(int j = 0; j < 18; j++)
            {
                DrawGBTile(i, j, tiles);
            }
        }

        EndDrawing();
    }

    CloseGBScreen();

    return 0;
}