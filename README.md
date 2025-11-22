# GameMan
attempting to make a gameboy emulator on windows for fun (it's called gameman because i could not think of a better name)

The contents of this README will serve as my personal notes on the technical details of the Game Boy synthesized from multiple sources (linked in the references), along with design details pertaining to my implementation. 

This project uses raylib 5.5.

## Preliminaries
This is an emulator specifically for the first Game Boy version, codenamed DMG. This will be implicitly assumed throughout this file and later versions will not be mentioned at all.  

> This project does not aim to emulate DMG0, an early variant of DMG.

This file is also focused on emulator development, so information related to ROM development will not be focused on.

In descriptions of I/O register bits, [xxxx / yyyy] means xxxx when the bit is 0 and yyyy when the bit is high.   
More generally, for any group of n bits, [aaa / bbb / ccc / ... 2^n] enumerates the actions taken as the value increases from 0 to 2^n - 1.

A __dot__ is one time unit of 2^22 MHz (4 MiHz). This is the frequency of the master clock. However, different components operate at different frequencies.
*   The CPU and the PPU run at 4 MiHz
*   The VRAM runs at 2 MiHz
*   The RAM runs at 1 MiHz

Since the CPU is slowed down by RAM, the CPU is considered to run at 1 MiHz. Thus, __M-Cycles__ (machine cycles) refer to 4-dot cycles.

## Memory Map
The Game Boy has a 16-bit address bus used to address ROM, RAM and I/O. Those regions not mentioned here will be explained as and when needed.

| Address range (Hex) | Description                   | 
|---------------------|-------------------------------|
| 0000 - 3FFF         | ROM Bank 00                   |
| 4000 - 7FFF         | ROM Bank 01 - NN              |
| 8000 - 9FFF         | VRAM                          |
| A000 - BFFF         | External RAM                  |
| C000 - DFFF         | Work RAM  (WRAM)              |
| E000 - FDFF         | Echo RAM                      |
| FE00 - FE9F         | Object Attribute Memory (OAM) |
| FEA0 - FEFF         | Unusable                      |
| FF00 - FF7F         | I/O Registers                 |
| FF80 - FFFE         | High RAM (HRAM)               |
| FFFF                | Interrupt Enable Register     |

The ranges 0x0000 - 0x7FFF and 0xA000 - 0xBFFF address external cartridge hardware.

The range 0x4000 - 0x7FFF can be switched across multiple ROM banks via a MBC (Memory Bank Controller) on the cartridge. The range 0xA000 - 0xBFFF can also be switched.

Echo RAM (0xE000 - 0xFDFF) is mapped to WRAM, but only the lower 13 bits of the address bus are connected - the rest being set internally in the memory controller by a bank swap register. All reads and writes to this range have the same effect as reads and writes to 0xC000 - 0xDFFF. Nintendo prohibits developers from using this range.

The use of the range 0xFEA0 - 0xFEFF is prohibited.

Reads and writes from 0xFF00 - 0xFFFF are optimised (see [this section](https://youtu.be/HyzD8pNlpwI?si=Q4hCk4CNdCEjELwu&t=860))

### I/O Registers
The Game Boy uses memory-mapped I/O, with the registers occupying the address range 0xFF00 - 0xFF7F. The I/O ranges are given below.

| Address range (Hex) | Purpose                  |
|---------------------|--------------------------|
| FF00                | Joypad input             |
| FF01 - FF02         | Serial transfer          |
| FF04 - FF07         | Timer and divider        |
| FF0F                | Interrupts               |
| FF10 - FF26         | Audio                    |
| FF30 - FF3F         | Wave pattern             |
| FF40 - FF4B         | LCD                      |
| FF50                | Boot ROM mapping control |

## CPU
The Game Boy is an 8-bit machine with 16-bit addressing. It is little-endian, which means that the least significant byte of data is stored at the lowest memory address, and vice versa.

### Registers and Flags
The CPU's registers are all 16 bits long, but can also be accessed as two separate 8-bit halves.

| Register (16-bit) | Hi | Lo | Function              |
|-------------------|----|----|-----------------------|
| AF                | A  | -  | Accumulator and flags |
| BC                | B  | C  | General purpose       |
| DE                | D  | E  | General purpose       |
| HL                | H  | L  | General purpose       |
| SP                | -  | -  | Stack pointer         |
| PC                | -  | -  | Program counter       |

#### Register F
| Bit | Name | Purpose                |
|-----|------|------------------------|
| 7   | Z    | Zero flag              |
| 6   | N    | Subtraction flag (BCD) |
| 5   | H    | Half carry flag (BCD)  |
| 4   | C    | Carry flag             |

- Z - set when the result of the operation is 0, cleared otherwise.
- C - set when 
    - the result of an 8-bit addition is greater than 0xFF
    - the result of a 16-bit addition is greater than 0xFFFF
    - the result of a subtraction or comparison is less than 0
    - a rotate/shift instruction shifts out a 1
- N, H - used only by the DAA instruction.
    - N indicates whether the previous instruction was a subtraction
    - H indicates carry for the lower 4 bits of the result

### Instruction set
The Game Boy's CPU has a CISC instruction set. The first byte of the instruction is called the _opcode_.  
Many opcodes perform the same instruction with different registers; hence, they will be grouped together. For example, an `inc r16` takes one 16-bit register as a parameter (which can be BC, DE, HL or SP), and `add a, r8` takes one 8-bit register as a parameter. The exact register used is defined in the opcode itself.

#### Opcode Argument Format and Possible Values
Arguments surrounded by [] indicate dereferencing - the value at the address pointed to by the argument is taken.

| Argument | 0   | 1   | 2   | 3   | 4 | 5 | 6    | 7 |
|----------|-----|-----|-----|-----|---|---|------|---|
| r8       | b   | c   | d   | e   | h | l | [hl] | a |
| r16      | bc  | de  | hl  | sp  |    
| r16stk   | bc  | de  | hl  | af  |
| r16mem   | bc  | de  | hl+ | hl- |
| cond     | nz  | z   | nc  | c   |

- b3 is a 3-bit index
- tgt3 is `rst`'s target address divided by 8
- imm8 is the following byte (adding 1 extra byte to the instruction)
- imm16 is the following two bytes (adding 2 extra bytes)
- ims8, ims16 are _signed_

Opcode arguments will be specified by placeholder 'xxx's and 'yyy's signifying the first and second arguments respectively.

#### Load Instructions
| Instruction      | Opcode    | Flags   |
|------------------|-----------|---------|
| ld r16, imm16    | 00xx 0001 | - - - - |
| ld [r16mem], a   | 00xx 0010 | - - - - |
| ld a, [r16mem]   | 00xx 1010 | - - - - |
| ld [imm16], sp   | 0000 1000 | - - - - |
| ld r8, imm8      | 00xx x110 | - - - - |
| ld r8, r8 (1)    | 00xx xyyy | - - - - |
| ldh [c], a       | 1110 0010 | - - - - |
| ldh [imm8], a    | 1110 0000 | - - - - |
| ld [imm16], a    | 1110 1010 | - - - - |
| ldh a, [c]       | 1111 0010 | - - - - |
| ldh a, [imm8]    | 1111 0000 | - - - - |
| ld [imm16], a    | 1111 1010 | - - - - |
| ld hl, sp + ims8 | 1111 1000 | 0 0 H C |
| ld sp, hl        | 1111 1001 | - - - - |

> 1- Trying to encode `ld [HL], [HL]` instead yields the `halt` instruction.

> `ldh [v], a` and `ldh a, [v]` use the address 0xFF00 + [v], as [v] is 8 bits.

#### Arithmetic/Logical Instructions
| Instruction      | Opcode    | Flags   |
|------------------|-----------|---------|
| inc r16          | 00xx 0011 | - - - - |
| dec r16          | 00xx 1011 | - - - - |
| add hl, r16      | 00xx 1001 | - 0 H C |
| inc r8           | 00xx x100 | Z 0 H - |
| dec r8           | 00xx x101 | Z 1 H - |
| add a, r8        | 1000 0xxx | Z 0 H C |
| adc a, r8        | 1000 1xxx | Z 0 H C |
| sub a, r8        | 1001 0xxx | Z 1 H C |
| sbc a, r8        | 1001 1xxx | Z 1 H C |
| and a, r8        | 1010 0xxx | Z 0 1 0 |
| xor a, r8        | 1010 1xxx | Z 0 0 0 |
| or a, r8         | 1011 0xxx | Z 0 0 0 |
| cp a, r8         | 1011 1xxx | Z 1 H C |
| add a, imm8      | 1100 0110 | Z 0 H C |
| adc a, imm8      | 1100 1110 | Z 0 H C |
| sub a, imm8      | 1101 0110 | Z 1 H C |
| sbc a, imm8      | 1101 1110 | Z 1 H C |
| and a, imm8      | 1110 0110 | Z 0 1 0 |
| xor a, imm8      | 1110 1110 | Z 0 0 0 |
| or a, imm8       | 1111 0110 | Z 0 0 0 |
| cp a, imm8       | 1111 1110 | Z 1 H C |
| daa              | 0010 0111 | Z - 0 C |
| cpl              | 0010 1111 | - 1 1 - |
| scf              | 0011 0111 | - 0 0 1 |
| ccf              | 0011 1111 | - 0 0 C |
| add sp, ims8     | 1110 1000 | 0 0 H C |

#### Control Flow Instructions
| Instruction      | Opcode    | Flags   |
|------------------|-----------|---------|
| jr ims8          | 0001 1000 | - - - - |
| jr cond, ims8    | 001x x000 | - - - - |
| ret cond         | 110x x000 | - - - - |
| ret              | 1100 1001 | - - - - |
| reti             | 1101 1001 | - - - - |
| jp cond, imm16   | 110x x010 | - - - - |
| jp imm16         | 1100 0011 | - - - - |
| call cond, imm16 | 110x x100 | - - - - |
| call imm16       | 1100 1101 | - - - - |
| rst tgt3         | 11xx x111 | - - - - |

#### Miscellaneous Instructions
| Instruction      | Opcode    | Flags   |
|------------------|-----------|---------|
| nop              | 0000 0000 | - - - - |
| stop             | 0001 0000 | - - - - |
| halt             | 0111 0110 | - - - - |
| di               | 1111 0011 | - - - - |
| ei               | 1111 1011 | - - - - |
| <prefix> (2)     | 1100 1011 | - - - - |
| pop r16stk (3)   | 11xx 0001 | Z N H C |
| push r16stk      | 11xx 0101 | - - - - |

> 2- 0xCB is a prefix instruction; the next byte contains the actual instruction.

> 3- `r16stk` can also be AF - this causes the flags to be set to the data popped into it. The flags are NOT set otherwise.

#### Shift Instructions
| Instruction      | Opcode    | Flags   |
|------------------|-----------|---------|
| rlca             | 0000 0111 | 0 0 0 C |
| rrca             | 0000 1111 | 0 0 0 C |
| rla              | 0001 0111 | 0 0 0 C |
| rra              | 0001 1111 | 0 0 0 C |

The following instructions are prefixed by 0xCB.
| Instruction      | Opcode    | Flags   |
|------------------|-----------|---------|
| rlc r8           | 0000 0xxx | Z 0 0 C |
| rrc r8           | 0000 1xxx | Z 0 0 C |
| rl r8            | 0001 0xxx | Z 0 0 C |
| rr r8            | 0001 1xxx | Z 0 0 C |
| sla r8           | 0010 1xxx | Z 0 0 C |
| swap r8          | 0011 0xxx | Z 0 0 0 |
| srl r8           | 0011 1xxx | Z 0 0 C |
| bit b3, r8       | 01xx xyyy | Z 0 1 - |
| res b3, r8       | 10xx xyyy | - - - - |
| set b3, r8       | 11xx xyyy | - - - - |

## Interrupts

### IME (Interrupt Master Enable)
IME is a write-only flag internal to the CPU that controls whether _any_ interrupt handlers are called. IME can only be modified by `ei`, `di` and `reti`.

IME is cleared right before an interrupt handler is `call`ed, and when the game starts running.

### IE (Interrupt Enable) (0xFFFF)
IE is used to enable and disable particular interrupts. It is overridden by IME.

| Bit | Interrupt |
|-----|-----------|
| 4   | Joypad    |
| 3   | Serial    |
| 2   | Timer     |
| 1   | LCD       |
| 0   | VBlank    |

### IF (Interrupt Flag) (0xFF0F)
When an interrupt request signal changes from low to high, the corresponding bit in IF is set.   
Any set bits in IF are only __requesting__ an interrupt. The interrupt is handled only if IME and the corresponding bit in IE are set; otherwise, it waits.   
The CPU automatically sets and clears the bits in IF, so it is not necessary to write to it; however, writing is possible and has the same outcome as if it was set by an interrupt request signal.

| Bit | Purpose | Interrupt Handler | Executed When                |
|-----|---------|-------------------|------------------------------|
| 4   | Joypad  | 0x60 | Any of the bits in P1 change from 1 to 0. |
| 3   | Serial  | 0x58 | A serial data transfer completes.         |
| 2   | Timer   | 0x50 | The timer TIMA overflows.                 |
| 1   | LCD     | 0x48 | Defined in the STAT register.             |
| 0   | VBlank  | 0x40 | The PPU enters VBlank (LY = 144).         |

- Because the interrupt only triggers on a rising edge, consecutive interrupts cannot be triggered as the STAT line has no time to go to 0. This phenomenon is known as __STAT blocking__.  
- Due to a hardware quirk, the LCD interrupt triggers when writing to STAT during OAM scan, HBlank, VBlank or LC=LYC. It behaves as if 0xFF was written for one M-cycle, and then the written value was written the next M-cycle. Two games (Ocean’s Road Rash and Vic Tokai’s Xerd no Densetsu) depend on this quirk.

### Interrupt Handling 
Interrupts are handled in the following steps:  
- IME and the corresponding bit in IF are cleared.
- The corresponding interrupt handler is called by the CPU. This occurs exactly like the regular `call` instruction.

## Graphics
The Game Boy has a 160x144 monochrome LCD display. A dedicated PPU (Pixel Processing Unit) renders graphics onto the LCD, drawing pixels row by row on the screen from left to right, similar to CRT screens. Each row is called a __scanline__.   

The PPU cycles between 4 modes (listed in order per frame):
| Mode | Name           | Duration (dots) |
|------|----------------|-----------------|
| 2    | OAM Scan       | 80              |
| 3    | Pixel transfer | 172-289         |
| 0    | HBlank         | 87-204          |
| 1    | VBlank         | 4560            |

A frame takes a total of 70224 dots, which comes to about 59.7 fps.

The PPU blocks the CPU from accessing OAM during modes 2 and 3 and VRAM during mode 3. Writes to blocked regions are ignored, and reads return garbage values (usually 0xFF).  
For this purpose, the PPU pauses for a few cycles to allow the CPU to modify VRAM and OAM. The two types of such periods are HBlank and VBlank. HBlank occurs after each scanline, and VBlank occurs after the frame is rendered. For this purpose, there are 154 scanlines - the first 144 correspond to the 144 rows of pixels on the LCD screen, and the PPU remains in VBlank mode for the remaining 10 scanlines.

### Layers
The Game Boy has 3 rendering layers:
*   Background
*   Window (e.g. text boxes)
*   Objects (player and other sprites)

Windows are rendered above the background, and objects above windows.

### Tiles
The base unit of Game Boy graphics is the tile - an 8x8 grid of pixels (tiles are also called patterns or characters). A 2-bit color index is used to encode color for a single pixel. This is called 2BPP (2 Bits Per Pixel).

Each tile is encoded in 16 bytes, which can be split into 8 2-byte pairs where each pair encodes one row (lower byte pairs encode higher rows.) In each pair, the first byte has the LSBs of the color indices of all the 8 pixels of the row it encodes, and the second byte has their MSBs.

Objects are special tiles that are independent of the tile grid. They consist of 1 tile (8x8 objects) or 2 tiles stacked horizontally (8x16 objects).

#### Colors and Palettes
The Game Boy can display 4 colours. By default, they are mapped as 11 being black, 10 being dark grey, 01 being light grey and 00 being white.   
This mapping can be changed in __BGP (0xFF47)__ - any index can map to any color (e.g. 00 can map to black and 11 to white).   
The register description of BGP is as follows:
| Bit(s) | Purpose |
|--------|---------|
| 7-6    | ID3     |
| 5-4    | ID2     |
| 3-2    | ID1     |
| 1-0    | ID0     |

Objects have two separate object palettes __OBP0 (0xFF48)__ and __OBP1 (0xFF49)__. Any of these palettes can be used by a particular object. They function the same as BGP, except the lower two bits are ignored since index 0 is transparent for objects.

### Tile Maps
Tile maps define the actual arrangement of tiles on screen (for the background and window layers). 

A tile map is stored as a 32x32 byte matrix. Each byte in this matrix contains the ___index___ of the tile at that position (going left to right, top to bottom as addresses increase). Each index points to a particular tile in memory.

Since tiles are 8x8 pixel squares, the tile map defines a 256x256 pixel region, out of which only a 160x144 pixel region is displayed on screen at a time. The displayed region can be scrolled using the LCD registers.

The window is not scrollable; it is always displayed starting from the top left tile of its tile map. The length and height of the window can be set using the LCD registers.

### OAM (Object Attribute Memory) (0xFE00 - 0xFE9f)
The Game Boy PPU can display up to 40 objects (8x8 or 8x16). All displayed objects have 4-byte attributes each, with a total of 160 bytes - this is what the OAM contains.   

#### Byte 0: Y Position
This byte controls the vertical position of the object on the screen, offset upwards by 16. This value can range from 0 to 160.
*   Y = 0 displays the object starting 16 pixels above the visible screen, completely hiding 8x16 objects.
*   Y = 16 displays the object starting right at the top of the visible screen.
*   Y = 160 displays the object starting one pixel below the visible screen, hiding it completely.

![](/README_images/OAM_1.png)

#### Byte 1: X Position
This byte controls the horizontal position of the object on the screen, offset to the left by 8. This value ranges from 0 to 168.
*   X = 0 displays the object starting 8 pixels to the left of the visible screen, completely hiding the object.
*   X = 8 displays the object starting at the first pixel from the left of the screen.
*   X = 168 displays the object starting one pixel to the right of the visible screen, completely hiding the object.

#### Byte 2: Tile Index
This byte specifies the index of the tile used.
*   For 8x8 objects, this index specifies the only tile used.
*   For 8x16 objects, this index specifies the top tile; the bottom tile is right after it.

#### Byte 3: Attributes
| Bit(s) | Purpose  |
|--------|----------|
| 7      | Priority |
| 6      | Y Flip   |
| 5      | X Flip   |
| 4      | Palette  |

*   Priority: if set, background and window color indices are drawn over this object.
*   Y Flip: if set, vertically mirrors the object.
*   X Flip: if set, horizontally mirrors the object.
*   Palette: 0 for OBP0, 1 for OBP1.

The recommended method to write to OAM is to write the data into a buffer in normal RAM, and then copy it to OAM using DMA transfer. This is because OAM cannot be directly accessed except during the HBlank and VBlank periods.

#### DMA (0xFF46)
Writing to this register triggers a DMA transfer request from ROM or RAM to OAM. The written value specifies the transfer source address divided by 0x100 (only the upper 8 bits are taken by the DMA controller). For example, if the value 0x0542 is written, a DMA transfer is started from (0x0500 - 0x059F) to OAM (0xFF00 - 0xFF9F).  

The transfer takes 640 dots (1.4 lines). During the transfer, the CPU can only access HRAM (0xFF80 - 0xFFFE).  
If the transfer is active during OAM scan (mode 2), most PPU revisions read each object as being off-screen and thus hidden on that line.  
If the transfer is active during mode 3, the PPU reads newly written data, causing incorrect tile numbers and attributes for objects already determined to be in range.

### LCD Registers (0xFF40 - 0xFF4B)
These registers control the way objects are displayed.

#### LCDC (LCD Control) (0xFF40)
| Bit(s) | Purpose                              | 
|--------|--------------------------------------|
| 7      | Enable LCD and PPU                   |
| 6      | Window tile map area                 |
| 5      | Window enable                        |
| 4      | Background and window tile data area |
| 3      | Background tile map area             |
| 2      | Object size                          |
| 1      | Object enable                        |
| 0      | Background and window enable         |

*   7 - Setting this to 0 turns off the LCD and PPU and grants the CPU immediate and full access to VRAM and OAM.
*   6 - [0x9800 - 0x9BFF / 0x9C00 - 0x9FFF].
*   5 - Overridden by LCDC.0 if LCDC.0 is 0.
*   4 - [0x8000 - 0x8FFF / 0x8800 - 0x97FF].
*   3 - Same as 6, but for background.
*   2 - [8x8 / 8x16], Controls the size of all objects.
*   1 - Hides all objects if 0.
*   0 - If 0, both background and window become blank (white).

This register can be modified ay any time during the frame.

#### LY (LCD Y coordinate) (0xFF44) (Read-only)
Indicates the current horizontal line just drawn, being drawn or to be drawn.  
Ranges from 0 to 153, where values from 144-153 indicate the VBlank period.

#### LYC (LCD Y Compare) (0xFF45)
When LY = LYC, a flag is set in the STAT register.

#### STAT (LCD STATus) (0xFF41)
| Bit(s) | Purpose                                                      |
|--------|--------------------------------------------------------------|
| 6      | LYC interrupt select                                         |
| 5      | Mode 2 interrupt enable                                      |
| 4      | Mode 1 interrupt enable                                      |
| 3      | Mode 0 interrupt enable                                      |
| 2      | LYC == LY (Read-only)                                        |
| 1-0    | PPU Mode (Read-only)                                         |

*   6 - If 0, does not set STAT.2.
*   5-3 - Raise an interrupt when the PPU switches to the specified mode.
*   1-0 - Holds the PPU's current mode.

Bits 5-2 are logically ORed into a shared STAT interrupt line. A rising edge on this line will cause a STAT interrupt.


#### SCY and SCX (SCroll Y and SCroll X) (0xFF42 and 0xFF43)
These two registers specify the top left __pixel__ coordinates of the visible 160x144 pixel region within the 256x256 pixel background tile map. They take the full 8-bit range of values (0-255).  

The PPU calculates the bottom right corner as follows: 

> Given `(SCX, SCY)` is the top left corner,  
> `((SCX + 159) % 256, (SCY + 143) % 256)` is the bottom right corner.  

As the modulo operation indicates, the visible region can wrap around tile map boundaries.

These registers are reread on every tile fetch (except the lower 3 bits of SCX, which are only read once at the beginning of the scanline for the initial shifting of pixels).   
SCY is read once for each bitplane.

#### WY and WX (Window Y and Window X) (0xFF4A and 0xFF4B)
These two registers specify the top left pixel coordinates of the window.  

WX ranges from 0-166 and WY ranges from 0-143. WX is offset by 7 pixels to the left (similar to the OAM X and Y bytes), so (WX, WY) = (7, 0) puts the top left corner of the window at the top left corner of the screen.

The window is drawn infinitely horizontally and vertically from its top left corner. Thus, a value of (7, 0) would cause the window to cover the entire background.

### VRAM (0x8000 - 0x9FFF)
Tile data is stored in the region 0x8000 - 0x97FF. This region is split into 3 "blocks" of 128 tiles each. Block 0 occupies the region (0x8000 - 0x87FF), Block 1 occupies (0x8800 - 0x8FFF), Block 2 occupies (0x9000 - 0x97FF).  

Tile maps are stored in the region 0x9800 - 0x9FFF. This region is split into 2 32x32 tile maps, occupying regions (0x9800 - 0x9BFF) and (0x9C00 - 0x9FFF). 

Tiles are indexed according to this table:
| Layer               | Block 0 | Block 1   | Block 2 |
|---------------------|---------|-----------|---------|
| Objects             | 0 - 127 | 128 - 255 |         |
| BG/Win (LCDC.4 = 1) | 0 - 127 | 128 - 255 |         |
| BG/Win (LCDC.4 = 0) |         | 128 - 255 | 0 - 127 |

Two addressing methods exist for tiles:
*   The 0x8000 method: the base pointer is 0x8000 with unsigned addressing from 0 - 256.
*   The 0x8800 method: the base pointer is 0x9000 with signed addressing. Indices 0 to 127 are taken from block 2, while indices from -128 to -1 are taken from block 1.
    +   Since negative numbers are represented in 2's complement, the range -128 to -1 is represented in binary as 10000000 to 11111111, which also stands for unsigned 128 - 255.

Objects always use 0x8000 addressing, while the background and the window can use either mode, controlled by LCDC.4.

### Rendering
>  This section is incomplete.

The PPU renders pixels on the screen as they are popped off from the __pixel FIFOs__. There are two pixel FIFOs - one for background/window pixels and one for object pixels. These FIFOs operate independently, except when pixels are popped.   
Each FIFO holds up to 16 pixels. The FIFO does not pop pixels unless it has over 8.  
The FIFOs are only manipulated during mode 3.

> When this text refers to the FIFO being 'empty', it actually means the FIFO has 8 pixels or less, which means 
> - it cannot pop to the LCD 
> - it can receive new pixels from the pixel fetcher

Each pixel in the FIFOs is stored as a grouping of 3 values:
*   A color index
*   A palette (only for objects)
*   Background priority - the value of the OBJ-to-BG priority bit.

#### Pixel Fetcher
The pixel fetcher fetches the next row of pixels as the FIFOs pop. It has 5 steps:

*   __Get tile index__ - The fetcher determines which tile the pixels need to be fetched from.  
The tilemap used is decided by the following logic:
    +   If LCDC.3 is 1 and the window is not being rendered, the tilemap at 0x9C00 is used.
    +   If LCDC.6 is 1 and the window is being rendered, the tilemap at 0x9C00 is used.
    +   Else, the tilemap at 0x9800 is used.
    
    The fetcher keeps track of the coordinates of the tile it's on.
    +   If the current tile is a background tile, the coordinates are calculated as `(((SCX / 8) + LX) & 0x1F, (SCY + LY) & 0xFF)`. Due to this, the coordinates range between (0, 0) and (31, 255).
    +   If the current tile is a window tile, the coordinates are kept track of by internal counters.

    > For a window to be displayed on a scanline, the following conditions must be met:
    > *   LCDC.5 is 1
    > *   WY was equal to LY at some point in the current frame (only checked at the start of mode 2)
    > *   WX was equal to LX + 7
    >
    > If the second condition had already been triggered and LCDC.5 was set at the start of a scanline, resetting that bit before the third condition is triggered on that row causes a glitch pixel where the window would have been activated.
    > 
    > When selecting which line of the window's tile map to render, the Y position is selected by an internal counter, which is reset to 0 during VBlank and only incremented when the window starts being rendered on a scanline. This means that hiding the window mid-frame will also cause the Y position counter to not increment.

    The fetcher's tile coordinates can then be used to get the tile index from the tilemap. 
    
*   __Get Low Tile Data__ - 
    +   Check LCDC.4 to determine the tile data region to use.
    +   Retrieve the tile data using the tile index.
    +   Use the Y coordinate to get the first byte of the row that is currently being rendered.

*   __Get High Tile Data__ - Same as the previous step, except the next byte is used. This step also creates the data for the 8 pixels of this row. If the FIFO is empty, the fetcher pushes to the FIFO in the same dot and skips the next two steps.

*   __Sleep__ - Does nothing.

*   __Push__ - Pushes a row of background/window pixels to the background FIFO. This can only occur if the background FIFO has 8 pixels of empty space.

Steps 1-4 take 2 dots each, and step 5 is repeated every dot until it succeeds. Popping from the FIFO takes 1 dot, so the fetcher follows an 8-dot memory access pattern of '3 reads, 1 sleep' as in [this section](https://youtu.be/HyzD8pNlpwI?si=aGxci2ZS6TlHJ5xD&t=2956).

#### Pixel FIFO Operation
The pixel FIFOs only operate during mode 3. At the beginning of mode 3, both FIFOs are cleared.

When the window starts being rendered, the background FIFO is cleared and the fetcher is reset to step 1. When WX is 0 and SCX & 7 > 0, mode 3 is shortened by 1 dot.
-   If WX is changed after the window starts rendering and the new value of WX is reached again, a pixel with a color index of 0 and the lowest priority is pushed onto the FIFO.

When (LX, LY) has an object on it and LCDC.1 is enabled, the object starts being rendered. Object fetching can be canceled at multiple points in this process.
-   If the background FIFO is not empty, the fetcher is at step 4. The fetcher is advanced to step 5, pushing the fetched pixels. Advancing the fetcher here lengthens mode 3 by 1 dot. 
-   If there is an object at X = 0 of the current scanline, mode 3 is lengthened by `(SCX & 7)` dots.

#### VRAM Access
The PPU's access to VRAM can be blocked in the following circumstances: 
-   The LCD is off
-   When switching from mode 3 to mode 0

Access is restored in the following circumstances:
-   When reaching scanline 0
-   When searching OAM and reaching index 37
-   After switching from mode 2 to mode 3

These conditions are only checked when entering STOP mode, and the PPU's access to VRAM is always restored upon leaving STOP mode.

## Boot ROM
When the Game Boy is powered on, the CPU starts executing instructions at 0x0000 - this is the start of the __boot ROM__. This is burned into the CPU. and is mapped over the addresses 0x0000 - 0x00FF of the cartridge ROM at first.

## Cartridge

### Cartridge Header (0x0100 - 0x014F)
Each cartridge contains a header, which contains the following information:

| Address range (Hex) | Description             | 
|---------------------|-------------------------|
| 0100 - 0103         | Entry point             |
| 0104 - 0133         | Nintendo logo           |
| 0134 - 0143*        | Title                   |
| 013F - 0142         | Manufacturer code       |
| 0143                | CGB Flag                |
| 0144 - 0145         | New licensee code       |
| 0146                | SGB Flag                |
| 0147                | Cartridge type          |
| 0148                | ROM Size                |
| 0149                | RAM Size                |
| 014A                | Destination code        |
| 014B                | Old licensee code       |
| 014C                | Mask ROM version number |
| 014D                | Header checksum         |
| 014E                | Global checksum         |

##  References
*   https://gekkio.fi/files/gb-docs/gbctr.pdf
*   https://gbdev.io/
*   https://www.raylib.com/
*   https://www.youtube.com/watch?v=HyzD8pNlpwI