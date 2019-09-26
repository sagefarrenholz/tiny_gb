#ifndef z80gb_h
#define z80gb_h
#include "gameboy.h"

extern CPU cpu;
#define c cpu

//Same as c but intended for pointer arithmetic in order to avoid warnings.
#define uc ((uint8_t*) c)
#define uc16 ((uint16_t*) c)

/*
ENDIANESS:
The z80-like processor of the Gameboy
uses little endian. The high byte of the array is
in the higher address; smallest in the lowest / first.

----------------------------------------

Register Example BC:
In memory B comes second because it is the highest byte:

ADDR 0		ADDR 1
C		B

----------------------------------------

Memory Example:
0x45BF in memory:

n Address		n Adress+1
0xBF			0x45

*/

//Register values and pointer to ram being used
#define RAM (c->ram)	
#define PC (c->pc)
#define SP (c->sp)
#define AF (c->af)
#define BC (c->bc)
#define DE (c->de)
#define HL (c->hl)
//Warning: these are pointers!
#define A (uc+9u)
#define F (uc+8u)
#define C uc

//Flag values, retrieve flag values
#define ZERO ((AF & 0x0080) >> 7)
#define SUB ((AF & 0x0040) >> 6)
#define HALF ((AF & 0x0020) >> 5)
#define CARRY ((AF & 0x0010) >> 4)

//Flag switches, these are all masks used for flipping flag bits in flag register
#define ZERO_SET (AF |= 0x0080)
#define ZERO_RESET (AF &= 0xFF7F) 

#define SUB_SET  (AF |= 0x0040)
#define SUB_RESET (AF &= 0xFFBF)

#define HALF_SET (AF |= 0x0020)
#define HALF_RESET (AF &= 0xFFDF)

#define CARRY_SET (AF |= 0x0010)
#define CARRY_RESET (AF &= 0xFFEF)

#define IME (c->ime)

//Important registers held in memory
//LCDC - 0xFF40 - Controlling rendering and the screen
#define LCDC (*(RAM+0xFF40))
#define LCDC_OBJ_SIZE ((LCDC & 0x04) >> 2) 


//LCDC STATUS  - 0xFF41 - Status of LCD and related interrupts
#define LCDC_STATUS (*(RAM+0xFF41))
//SET COMPARE BIT
#define LCDC_STATUS_COINCIDENCE(C) (LCDC_STATUS &= (0x0FB | (C << 2)))
//SET MODE BITS
#define LCDC_STATUS_MODE(M) (LCDC_STATUS &= (0xFC | M))
//GET MODE BITS
#define LCDC_STATUS_MODE_GET (LCDC_STATUS & 0x04)

//LY - 0xFF44 - Current line to be rendered - Read only
#define LINE (*(RAM+0xFF44))

//LYC - 0xFF45 - Used for comparing against LY
#define LYC (*(RAM+0xFF45))



//DMA - 0xFF56 - Transfer memory chunk from XX00 - XX9F to FE00-FE9F (OAM MEMORY)
#define DMA (*(RAM+0xFF56))

/*

Summary:
execute() simulates the executing cycles of
the gameboy's z80 processor. It manipulates the
input processor's registers and the input ram so 
as to perform the action of the next instruction.

Return value:
Number of cpu clock cycles. NOT machine cycles.

*/
int execute();

/*

 [==========]
  INTERRUPTS
 [==========]
 
*/

//Switch IME ON, ime is master interrupt switch. 
static inline void ei(){
	c->ime = 1;
}

/*

 [===================]
  GENERAL INSTRUCTION
 [===================]

>---------------------<
 These are not actual 
 instructions but
 generalized for 
 easier to write / 
 easier to understand
 code. A faster 
 approach may just to
 be a giant opcode to
 instruction map.
>---------------------<

*/

//load
static inline void ld(uint8_t* dest, uint8_t* src) {
	*dest = *src;
}

//load 2 bytes
static inline void ld16(uint16_t* dest, uint16_t* src){
	*dest = *src;
}

/*
NOTE:
Conflicting documentation on zero flag effect
from rotate operation. Some sources say
zero flag is always reset while others
say it can act as defined.
*/
//rotate left 
static inline void rlc(uint8_t* dest){
	//Normal bit rotation left. Bit 7 is copied into carry flag.
	uint8_t car = *dest & 0x80;
	if(car) CARRY_SET; else CARRY_RESET;
	SUB_RESET;
	HALF_RESET;
	ZERO_RESET;
	*dest <<= 1;
	*dest |= (car >> 7);
}

//rotate left through carry
static inline void rl(uint8_t* dest){
	//if carry bit is set it is rotated into bit 0. Bit 7 is rotated left into carry.
	uint8_t carry = CARRY;
	uint8_t car = *dest & 0x80;
	if(car) CARRY_SET; else CARRY_RESET;
	SUB_RESET;
	HALF_RESET;
	ZERO_RESET;
	*dest <<= 1;
	*dest |= carry;
}

//rotate right 
static inline void rrc(uint8_t* dest){
	//Normal bit rotation right. Bit 0 is copied into carry flag.
	uint8_t car = *dest & 0x01;
	if(car) CARRY_SET; else CARRY_RESET;
	SUB_RESET;
	HALF_RESET;
	ZERO_RESET;
	*dest >>= 1;
	*dest |= (car << 7);
}

//rotate left through carry
static inline void rr(uint8_t* dest){
	//if carry bit is set it is rotated into bit 7. Bit 0 is rotated right into carry.
	uint8_t carry = CARRY;
	uint8_t car = *dest & 0x01;
	if(car) CARRY_SET; else CARRY_RESET;
	SUB_RESET;
	HALF_RESET;
	ZERO_RESET;
	*dest >>= 1;
	*dest |= (carry << 7);
}

//shift right into carry, highest bit remains same
static inline void sr(uint8_t* dest){
	HALF_RESET;
	SUB_RESET;
	ZERO_RESET;
	uint8_t msb = *dest & 0x80;
	if(*dest & 0x01) CARRY_SET; else CARRY_RESET;
	*dest >>= 1;
	*dest |= msb;
}

//shift right into carry, highest bit is zeroed
static inline void srl(uint8_t* dest){
	HALF_RESET;
	SUB_RESET;
	ZERO_RESET;
	if(*dest & 0x01) CARRY_SET; else CARRY_RESET;
	*dest >>= 1;
}

//shift left into carry, lowest bit is zeroed
static inline void sl(uint8_t* dest){
	HALF_RESET;
	SUB_RESET;
	ZERO_RESET;
	if(*dest & 0x80) CARRY_SET; else CARRY_RESET;
	*dest <<= 1;
}

//swap the high and low nibble of a byte
static inline void swp(uint8_t* dest){
	if(!*dest) ZERO_SET;
	HALF_RESET;
	SUB_RESET;
	CARRY_RESET;
	uint8_t hn = *dest & 0xF0;
	*dest = (*dest << 4) & hn;

}

//increment
static inline void inc(uint8_t* dest){
	SUB_RESET;
	//Check if half-carry
	if(((*dest) & 0x0F) == 0x0F) HALF_SET;
	//Check if result zero
	if(*dest == 0xFF) ZERO_SET;
	(*dest)++; 	
}

//increment 2 bytes
static inline void inc16(uint16_t* dest){
	(*dest)++;
}

//decrement
static inline void dec(uint8_t* dest){
	SUB_SET;
	//Check if half-carry
	if(*dest & 0x10) HALF_SET;
	//Check if result zero
	if(*dest == 0x01) ZERO_SET;
	(*dest)--; 	
}

//decrement 2 bytes
static inline void dec16(uint16_t* dest){
	(*dest)--;
}

//add
static inline void add(uint8_t* dest, uint8_t* src){
	uint16_t sum = *dest + *src;
	uint16_t carry = sum ^ (*dest ^ *src);
	SUB_RESET;
	//Check carry
	if(carry & 0x100) CARRY_SET; 
	//Check if half-carry
	if(carry & 0x10) HALF_SET;
	//Check if answer equals / overflows to 0
	if(!(*dest + *src)) ZERO_SET;
	*dest += *src;
}

//add 2 bytes
static inline void add16(uint16_t* dest, uint16_t* src){
	uint32_t sum = *dest + *src;
	uint32_t carry = sum ^ (*dest ^ *src);
	SUB_RESET;
	//Check carry
	if(carry & 0x10000) CARRY_SET;
	//Check half-carry, in a 16 bit add the half cary is based on a carry from bit 11, weirdly 
	if(carry & 0x1000) HALF_SET;
	*dest = (uint16_t) sum;
}

//add with carry
static inline void adc(uint8_t* dest, uint8_t* src){
	SUB_RESET;
	uint16_t temp = *dest + CARRY + *src;
	uint16_t carry = (*dest ^ CARRY ^ *src) ^ temp;
	if(!temp || temp > 0xFFFF) ZERO_SET;
	if(temp > 0xFFFF) CARRY_SET;
	if(carry & 0x10) HALF_SET;
	*dest = (uint8_t) temp; 
}


//subtract
static inline void sub(uint8_t* dest, uint8_t* src){
	SUB_SET;
	uint16_t temp = *dest - *src;
	uint16_t borrow = temp ^ (*dest ^ *src);
	if(!(*dest - *src)) ZERO_SET;
	if(borrow & 0x100) CARRY_SET;
	if(borrow & 0x10) HALF_SET;
	*dest = (uint8_t) temp;
}

//sub 2 bytes
//static inline void sub16(uint16_t* dest, uint16_t* src);

static inline void sdc(uint8_t* dest, uint8_t* src){
	SUB_SET;
	uint16_t diff = *dest - (*src + CARRY);
	uint16_t borrow = diff ^ (*dest ^ (*src + CARRY));
	if(!diff) ZERO_SET;
	if(borrow & 0x100) CARRY_SET;
	if(borrow & 0x10) HALF_SET;
	*dest = (uint8_t) diff;
}

//jump
static inline void jp(uint16_t* dest) {
	PC = *dest;
}

//relative jump
static inline void jr(int8_t* offset){
	//Address must be advanced by two to represent the relative jump from the beginning of the next instructions address.
	PC += 2;
	PC += *offset;
}

/*

 [====]
  MAPS
 [====]

*/

static inline uint8_t* reg(uint8_t reg_val){
	/*
	Register maps:
	0: B 
	1: C
	2: D
	3: E
	4: H
	5: L
	6: (HL) / address of hl
	7: A
	*/
	//if reg_val == 6 return the addr of value at HL
	if(reg_val == 6) return RAM + HL;
	//view gameboy.h for why pointer is advanced certain amounts
	uint8_t map[8] = {1, 0, 3, 2, 5, 4, 0, 9};
	return uc + map[reg_val];
}

//register pair map 1
static inline uint16_t* rp(uint8_t reg_val){
	/*
	Register pair map with stack pointer:
	0: BC
	1: DE
	2: HL
	3: SP
	*/
	return uc16 + reg_val;	
}

//register pair map 2
static inline uint16_t* rp2(uint8_t reg_val){
	/*
	Register pair map with accumulator and flag pair:
	0: BC
	1: DE
	2: HL
	3: AF
	*/
	return reg_val==3 ? &AF : uc16+reg_val;	
}

/*

 [==========]
  ARITHMETIC
 [==========]

*/
static inline void alu(uint8_t operation, uint8_t* src){
	/*
	Arithmetic map
	0: ADD
	1: ADC
	2: SUB
	3: SBC
	4: AND
	5: XOR
	6: OR
	7: CP
	*/
	switch(operation){
		case 0:
			add(A, src);
			break;
		case 1:
			adc(A, src);
			break;
		case 2:
			sub(A, src);
			break;
		case 3:
			sdc(A, src);
			break;
		case 4:
			//AND
			*A &= *src;
			SUB_RESET;
			HALF_SET;
			CARRY_RESET;
			if(!*A) ZERO_SET; 
			break;
		case 5:
			//OR
			*A |= *src;
			SUB_RESET;
			HALF_RESET;
			CARRY_RESET;
			if(!*A) ZERO_SET;
			break;
		case 6:
			//XOR
			*A ^= *src;
			SUB_RESET;
			HALF_RESET;
			CARRY_RESET;
			if(!*A) ZERO_SET;
			break;
		case 7:
			//Compare; same as subtract but result is not stored in A
			SUB_SET;
			uint16_t temp = *A - *src;
			uint16_t borrow = temp ^ (*A ^ *src);
			if(!(*A - *src)) ZERO_SET;
			if(borrow & 0x100) CARRY_SET;
			if(borrow & 0x10) HALF_SET;
			break;
	}
} 

/*

 [==================]
  CONDITION CHECKING
 [==================]

*/

//check condition held in y
static inline uint8_t con(uint8_t condition){
	/*
	Condition map
	0: Zero flag 0 / Disabled
	1: Zero Flag 1 / Enabled
	2: Carry flag 0 / Disabled
	3: Carry flag 1 / Enabled
	*/
	uint8_t ret = 0;
	switch(condition){
		//Not Zero?
		case 0:
		 	if(!(ZERO)) return 1;
			break;
		//Zero?
		case 1:
			if(ZERO) return 1;
			break;
		//Not Carry?
		case 2:
			if(!(CARRY)) return 1;
			break;
		//Carry?
		case 3:
			if(CARRY) return 1;
			break;
	}
	return 0;
}

/*

 [==================]
  STACK INSTRUCTIONS
 [==================]

*/

/*
	Note:
	The stack grows down on the gameboy. Whenever a push occurs,
       	the stack pointer decrements rather than incrementing.
*/

//return
static inline void ret(){
	//Goto address at last in of stack then increment the stack by 2 bytes.
	PC=*(uint16_t*)(RAM + SP);	
	SP+=2;
}
//pop word / 2bytes off stack into register pair
static inline void pop(uint16_t* rp){
	*rp = *(uint16_t*)(RAM + SP);
	SP+=2;
}

//decrement stack pointer by 2 bytes and set the 2 bytes equal to register pair
static inline void push(uint16_t* rp){
	SP-=2;
	*(uint16_t*)(RAM+SP) = *rp;
}

//push address of next instruction onto stack then jump to instruction
static inline void call(uint16_t* dest){
	uint16_t temp_addr = PC+3;
	push(&temp_addr);
	jp(dest);
}

#endif
