#pragma once

#include "common.h"
#include "raylib.h"

void InitEmulatorWindow();

void CloseEmulatorWindow();

void DrawGBPixel(int x, int y, int colorIndex);