#ifndef z80gb_h
#define z80gb_h
#include "gameboy.h"

/*

Summary:
execute() simulates the executing cycles of
the gameboy's z80 processor. It manipulates the
input processor's registers and the input ram so 
as to perform the action of the instruction parameter.

Paramaters:
cpu: pointer to z80 struct containing registers.
ram: pointer to ram structure.
instruction: byte instruction to be executed.

Return value:
Number of cpu clock cycles. NOT machine cycles.

Notes:
Program counter will likely change values.
*/
int execute(CPU cpu, uint8_t instruction);

#endif
