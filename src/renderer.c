#include "renderer.h"

Color palette[4] = {
    {255, 255, 255, 255}, 
    {200, 200, 200, 255}, 
    {80, 80, 80, 255}, 
    {0, 0, 0, 255}
};

const int defaultScreenWidth = 160;
const int defaultScreenHeight = 144;
int screenScaler = 4;

void InitEmulatorWindow()
{
    InitWindow(defaultScreenWidth * screenScaler, defaultScreenHeight * screenScaler, "GameMan");
    SetTargetFPS(60);
}

void CloseEmulatorWindow()
{
    CloseWindow();
}

void DrawGBPixel(int x, int y, int colorIndex)
{
    int realX = x * screenScaler;
    int realY = y * screenScaler;

    for(int i = 0; i < screenScaler; i++)
    {
        for(int j = 0; j < screenScaler; j++)
        {
            DrawPixel(realX + i, realY + j, palette[colorIndex]);
        }
    }
}