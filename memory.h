#pragma once

#include <stdint.h>

uint8_t ROM0[16384];        //0x0000 - 0x3FFF, from cartridge
uint8_t ROM1[16384];        //0x4000 - 0x7FFF, from cartridge (switchable via mapper)
uint8_t VRAM[8192];         //0x8000 - 0x9FFF
uint8_t ExtRAM[8192];       //0xA000 - 0xBFFF
uint8_t WRAM[8192];         //0xC000 - 0xDFFF
//Echo of WRAM              //0xE000 - 0xFDFF   
uint8_t OAM[160];           //0xFE00 - 0xFE9F
//Unused                    //0xFEA0 - 0xFEFF
uint8_t IO[128];            //0xFF00 - 0xFF7F
uint8_t HRAM[127];          //0xFF80 - 0xFFFE
uint8_t InterruptEnableRegister;    //0xFFFF


uint8_t* GetMemorySpace(uint16_t address, int bytes)
{
    static uint16_t limits[10] = {0x4000, 0x8000, 0xA000, 0xC000, 0xE000, 0xFE00, 0xFEA0, 0xFF00, 0xFF80, 0xFFFF};
    static uint8_t* spaces[11] = {ROM0, ROM1, VRAM, ExtRAM, WRAM, WRAM, OAM, NULL, IO, HRAM, &InterruptEnableRegister};

    if(bytes < 1) 
    {
        return NULL;
    }

    int index = 0;
    for(; index < 10; index++)
    {
        if(address < limits[index]) break;
    }

    uint8_t* space = spaces[index];
    if(index == 10 && bytes == 1) 
    {
        return space;
    }
    
    if(address + bytes > limits[index])
    {
        return NULL;
    }
    
    uint16_t offset;
    if(index == 0) offset = address;
    else offset = address - limits[index - 1];

    return space + offset;
}

int ReadMem(uint16_t address, uint8_t* buf, int bytes)
{
    uint8_t* space = GetMemorySpace(address, bytes);
    if(space == NULL) 
    {
        return 0;
    }

    for(int i = 0; i < bytes; i++)
    {
        buf[i] = space[i];
    }

    return 1;
}

int WriteMem(uint16_t address, const uint8_t* buf, int bytes)
{
    uint8_t* space = GetMemorySpace(address, bytes);
    if(space == NULL) 
    {
        return 0;
    }

    for(int i = 0; i < bytes; i++)
    {
        space[i] = buf[i];
    }

    return 1;
}