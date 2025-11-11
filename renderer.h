#pragma once

#include "raylib.h"

Color palette[4] = {
    {255, 255, 255, 255}, 
    {200, 200, 200, 255}, 
    {80, 80, 80, 255}, 
    {0, 0, 0, 255}
};

const int defaultScreenWidth = 160;
const int defaultScreenHeight = 144;
int screenScaler = 4;

void InitGBScreen()
{
    InitWindow(defaultScreenWidth * screenScaler, defaultScreenHeight * screenScaler, "GameMan");
    SetTargetFPS(60);
}

void CloseGBScreen()
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

void DrawGBTile(int tileX, int tileY, const uint8_t* tileData)
{
    int pixelX = tileX * 8;
    int pixelY = tileY * 8;

    for(int row = 0; row < 8; row++)
    {
        uint8_t rowLSBs = tileData[row * 2];
        uint8_t rowMSBs = tileData[row * 2 + 1];

        for(int col = 7; col >= 0; col--)
        {
            int LSB = rowLSBs % 2;
            int MSB = rowMSBs % 2;
            int colorIndex = MSB * 2 + LSB;

            DrawGBPixel(pixelX + col, pixelY + row, colorIndex);

            rowLSBs >>= 1;
            rowMSBs >>= 1;
        }
    }
}