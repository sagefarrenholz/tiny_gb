#ifndef gameboy_h
#define gameboy_h
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>

#if defined(__APPLE__) || (__gnu_linux__)
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <time.h>
#include <sys/time.h>
#elif defined(WIN32)
#define _CRT_SECURE_NO_WARNINGS_
#include <SDL2\SDL.h>
#include <SDL22\SDL_ttf.h>
#endif

#define CLOCK_FREQ 4.194304
#define CPU Sharp_LR35902*

//Gameboy Processor:
typedef struct Sharp_LR35902  {
uint16_t
bc,             //b + c registers
de,             //d + e registers
hl,             //h + l registers
sp,             //stack pointer
af,             //accumulator + flags
pc;             //program counter
uint8_t* ram;	//pointer to ram 
uint8_t ime;	//interupt master enable flag, if != 0 then all interrupt bits enabled in 0xFFFF are enabled.
uint8_t lcdc;	//lcd control register
uint8_t prefixed;	//previously executed opcode
} Sharp_LR35902;

#endif
