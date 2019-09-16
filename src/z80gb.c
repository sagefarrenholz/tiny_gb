#include "z80gb.h"
/*
Instruction function name code:
r=register
n=immediate one byte
nn=immediate two byte
an=address at any immediate
ar=address at any register
a*= address at * register (please research register characters online)

examples:
ldrr load any register into any register
ldaHLr load any register into address at HL register
addAr
*/

//CPU struct holds all registers and pointer to ram
//static CPU c;

//Same as c but intended for pointer arithmetic in order to avoid warnings.
//#define uc (uint8_t*) c
//#define uc16 (uint16_t*) c

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
/*#define RAM (c->ram)	
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
#define ZERO (AF & 0x0080 >> 7)
#define SUB (AF & 0x0040 >> 6)
#define HALF (AF & 0x0020 >> 5)
#define CARRY (AF & 0x0010 >> 4)

//Flag switches, these are all masks used for flipping flag bits in flag register
#define ZERO_SET AF |= 0x0080
#define ZERO_RESET AF &= 0xFF7F 

#define SUB_SET  AF |= 0x0040
#define SUB_RESET AF &= 0xFFBF

#define HALF_SET AF |= 0x0020
#define HALF_RESET AF &= 0xFFDF

#define CARRY_SET AF |= 0x0010
#define CARRY_RESET AF &= 0xFFEF
*/
/*

 [==========]
  INTERRUPTS
 [==========]
 
*/

//Switch IME ON
/*void ei(){
	c->ime = 1;
}
*/
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
/*
//load
void ld(uint8_t* dest, uint8_t* src) {
	*dest = *src;
}

//load 2 bytes
void ld16(uint16_t* dest, uint16_t* src){
	*dest = *src;
}
*/
/*
NOTE:
Conflicting documentation on zero flag effect
from rotate operation. Some sources say
zero flag is always reset while others
say it can act as defined.
*/
//rotate left 
/*void rlc(uint8_t* dest){
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
void rl(uint8_t* dest){
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
void rrc(uint8_t* dest){
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
void rr(uint8_t* dest){
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

//increment
void inc(uint8_t* dest){
	SUB_RESET;
	//Check if half-carry
	if(((*dest) & 0x0F) == 0x0F) HALF_SET;
	//Check if result zero
	if(*dest == 0xFF) ZERO_SET;
	(*dest)++; 	
}

//increment 2 bytes
void inc16(uint16_t* dest){
	(*dest)++;
}

//decrement
void dec(uint8_t* dest){
	SUB_SET;
	//Check if half-carry
	if(*dest & 0x10) HALF_SET;
	//Check if result zero
	if(*dest == 0x01) ZERO_SET;
	(*dest)--; 	
}

//decrement 2 bytes
void dec16(uint16_t* dest){
	(*dest)--;
}

//add
void add(uint8_t* dest, uint8_t* src){
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
void add16(uint16_t* dest, uint16_t* src){
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
void adc(uint8_t* dest, uint8_t* src){
	SUB_RESET;
	uint16_t temp = *dest + CARRY + *src;
	uint16_t carry = (*dest ^ CARRY ^ *src) ^ temp;
	if(!temp || temp > 0xFFFF) ZERO_SET;
	if(temp > 0xFFFF) CARRY_SET;
	if(carry & 0x10) HALF_SET;
	*dest = (uint8_t) temp; 
}


//subtract
void sub(uint8_t* dest, uint8_t* src){
	SUB_SET;
	uint16_t temp = *dest - *src;
	uint16_t borrow = temp ^ (*dest ^ *src);
	if(!(*dest - *src)) ZERO_SET;
	if(borrow & 0x100) CARRY_SET;
	if(borrow & 0x10) HALF_SET;
	*dest = (uint8_t) temp;
}

//sub 2 bytes
void sub16(uint16_t* dest, uint16_t* src);

void sdc(uint8_t* dest, uint8_t* src){
	SUB_SET;
	uint16_t diff = *dest - (*src + CARRY);
	uint16_t borrow = diff ^ (*dest ^ (*src + CARRY));
	if(!diff) ZERO_SET;
	if(borrow & 0x100) CARRY_SET;
	if(borrow & 0x10) HALF_SET;
	*dest = (uint8_t) diff;
}

//jump
void jp(uint16_t* dest) {
	PC = *dest;
}

//relative jump
void jr(int8_t* offset){
	PC += *offset;
}
*/
/*

 [====]
  MAPS
 [====]

*/
/*
uint8_t* reg(uint8_t reg_val){
	
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
	/*if(reg_val == 6) return RAM + HL;
	//view gameboy.h for why pointer is advanced certain amounts
	uint8_t map[8] = {1, 0, 3, 2, 5, 4, 0, 9};
	return uc + map[reg_val];
}
*/
//register pair map 1
//uint16_t* rp(uint8_t reg_val){
	/*
	Register pair map with stack pointer:
	0: BC
	1: DE
	2: HL
	3: SP
	*/
	//return uc16+reg_val*2u;	
//}

//register pair map 2
//uint16_t* rp2(uint8_t reg_val){
	/*
	Register pair map with accumulator and flag pair:
	0: BC
	1: DE
	2: HL
	3: AF
	*/
	//return reg_val==3 ? &AF : uc16+reg_val*2u;	
//}

/*

 [==========]
  ARITHMETIC
 [==========]

*/
//void alu(uint8_t operation, uint8_t* src){
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
/*	switch(operation){
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
*/
/*

 [==================]
  CONDITION CHECKING
 [==================]

*/

//check condition held in y
//uint8_t con(uint8_t condition){
	/*
	Condition map
	0: Zero flag 0 / Disabled
	1: Zero Flag 1 / Enabled
	2: Carry flag 0 / Disabled
	3: Carry flag 1 / Enabled
	*/
/*	uint8_t result = 0;
	switch(condition){
		//Not Zero?
		case 0:
		 	if(!(AF & 0x0080)) result = 1;
			break;
		//Zero?
		case 1:
			if(AF & 0x0080) result = 1;
			break;
		//Not Carry?
		case 2:
			if(!(AF & 0x0010)) result = 1;
			break;
		//Carry?
		case 3:
			if(AF & 0x0010) result = 1;
			break;
		default: 
			return 0;
		
	}
	return result;
*/
////////////////}

/*

 [==================]
  STACK INSTRUCTIONS
 [==================]

*/
//return
//void ret(){
	//Goto address at last in of stack then increment the stack by 2 bytes.
//	PC=*(uint16_t*)(RAM + SP);	
//	SP+=2;
//}
//pop word / 2bytes off stack into register pair
//void pop(uint16_t* rp){
//	*rp = *(uint16_t*)(RAM + SP);
//	SP+=2;
//}

//decrement stack pointer by 2 bytes and set the 2 bytes equal to register pair
//void push(uint16_t* rp){
//	SP-=2;
//	*(uint16_t*)(RAM+SP) = *rp;
//}

//push address of next instruction onto stack then jump to instruction
//void call(uint16_t* dest){
//	uint16_t temp_addr = PC+1;
//	push(&temp_addr);
//	jp(dest);
//}

/*

 [====================]
  EXECUTION ALGORITHIM
 [====================]

*/


inline int execute(){
	//These declarations are at the top of the page.
	//c = cpu;

	//OPERANDS
	//n is a pointer to the byte following the opcode, nn a pointer to the two bytes following the opcode LSB, d is a pointer to the signed byte following the opcode
	#define n (RAM + PC + 1)
	#define nn (uint16_t*)n
	#define d (int8_t*)n	

	/*
	All of these values are encoded
	within the opcode and are used
	for deducing the function performed
	by the opcode. 
	
	References used:
	https://gb-archive.github.io/salvage/decoding_gbz80_opcodes/Decoding%20Gamboy%20Z80%20Opcodes.html
	*/

	#define x ((*(RAM + PC) & 0xC0) >> 6) 			//7-6 bits; C0 Mask 1100 0000
	#define y ((*(RAM + PC) & 0x38) >> 3)			//5-3 bits; 38 Mask 0011 1000 
	#define z ((*(RAM + PC) & 0x07))			//2-0 bits; 07 Mask 0000 0111
	#define p (y >> 1)					//y(5-4 bits)
	#define q (y % 2)					//y(3 bit)

	//temp d variable used to prevent bad access to high byte; also used by PREFIX OPCODE
	int16_t temp_d = (int16_t) *d;
	//rst define used to reference restarts for RST command
	#define rst ((uint16_t*) RAM + (y * 8))
	
	if(cpu->prefixed){
		cpu->prefixed = 0;
		switch(x){
			//ROT
			case 0:
				PC++;
				return 8; 
				break;
			//BIT TEST
			case 1:
				SUB_RESET;
				HALF_SET;
				if()
				PC++;
				return 8;
				break;
			//BIT RESET
			case 2:
				PC++;
				return 8;
				break;
			//BIT SET
			case 4:	
				PC++;
				return 8;
				break;
		}
	}
	//Set this instruction as the previously executed opcode
	switch (x) {
		//x is 0
		case 0:
			switch(z) {
				case 0:
					switch(y) {
						//NOP 0x00; Do nothing.
						case 0:
						PC++;
						return 4;
						break;
						
						//LD (nn), SP; 0x08; load value at address nn into stack pointer
						case 1:
						ld16(&SP,nn);		
						PC+=3;
						return 20;
						break;
						
						//STOP; 0x10;
						case 2: 	
						return 10;
						break;
						
						//JR d; 0x18; relative jump
						case 3:
						jr(d);
						return 12;
						break;
						
						//JR cc, n; 0x20 NZ, 0x28 Z, 0x30 NC, 0x38 C; relative jump based on condition
						default:
						if(con(y-4)) jr(d);
						else {PC+=2; return 8;}
						return 12;
						break;
					}
				case 1:
					if(!q){
						//LD rp(p),nn; 0x01, 0x11, 0x21, 0x31; load 2 byte immediate into register pair
						ld16(rp(p),nn);
						PC+=3;
						return 12;
						break;
					} else {
						//ADD HL, rp(p); 0x09, 0x19, 0x29, 0x39; add register pair to register HL
						add16(&HL, rp(p));
						PC++;
						return 8;	
						break;
					}
				case 2:
					if(!q){
						switch(p) {
							case 0:
								//ld (BC),A ;0x02; load register A into value at address BC
								ld(RAM + BC, A);
								PC++;
								return 8;
								break;
							case 1:
								//ld (DE),A ;0x12; load register A into value at address DE
								ld(RAM + DE, A);
								PC++;
								return 8;
								break;
							case 2:
								//ld (HL+),A; 0x22; load A into into value at HL and increment HL after
								ld(RAM + HL, A);
								inc16(&HL);
								PC++;
								return 8;
								break;
							case 3:
								//ld (HL-),A; 0x32; load A into into value at HL and decrement HL after
								ld(RAM + HL, A);
								dec16(&HL);
								PC++;
								return 8;
								break;
						}
					} else {		
						switch(p) {
							case 0:
								//ld A, (BC); 0x0A; load value at BC into register A
								ld(A, RAM + BC);
								PC++;
								return 8;
								break;
							case 1:
								//ld A, (DE); 0x1A; load value at DE into register A
								ld(A, RAM + DE);
								PC++;
								return 8;
								break;
							case 2:
								//ld A,(HL+); 0x2A; load value at HL into register A and increment HL after
								ld(A, RAM + HL);
								inc16(&HL);
								PC++;
								return 8;
								break;
							case 3:
								//ld (HL-),A; 0x3A; load value at HL into register A and decrement HL after
								ld(A, RAM + HL);
								dec16(&HL);
								PC++;
								return 8;
								break;
						}
					}
				case 3: 
					if(!q){
						//inc rp(p); 0x03, 0x13, 0x23, 0x33; increment 16bit register pair
						inc16(rp(p));
						PC++;
						return 8;
					} else {
						//dec rp(p); 0x0B, 0x1B, 0x2B, 0x3B; decrement 16bit register pair
						dec16(rp(p));
						PC++;
						return 8;
					}
					break;
				case 4:
					//inc r(y); 0x04, 0x14, 0x24, 0x34, 0x0C, 0x1C, 0x2C, 0x3C; increment 8bit register
					inc(reg(y));
					PC++;
					if(y==6) return 12;
					return 4;
					break;
				case 5:
					//dec r(y); 0x05, 0x15, 0x25, 0x35, 0x0D, 0x1D, 0x2D, 0x3D; decrement 8bit register
					dec(reg(y));
					PC++;
					if(y==6) return 12;
					return 4;
					break;
				case 6:
					//ld r(y),n; 0x06,0x16,0x26,0x36,0x0E,0x1E,0x2E,0x3E;load immeadiate into 8bit register	
					ld(reg(y), n);
					PC+=2;
					if(y==6) return 12;
					return 8;
					break;
				case 7:
					switch(y){
						case 0:
							//rlca; 0x07; rotate a left
							rlc(A);
							PC+=1;
							return 4;
							break;
						case 1:
							//rla; 0x17; rotate a left through carry
							rl(A);
							PC+=1;
							return 4;
							break;
						case 2:
							//rrca; 0x0F; rotate a right
							rrc(A);
							PC+=1;
							return 4;
							break;
						case 3:
							//rra; 0x1F; rotate a right through carry
							rr(A);
							PC+=1;
							return 4;
							break;
						case 4:
							//daa; 0x27; pack a into bcd
							if(*A == 99) ZERO_SET;
							uint16_t temp = *A;
							if((*A & 0x0F) > 0x09) temp += 0x06;
							if((*A & 0xF0 >> 8) > 0x09) {
							temp += 0x60;
							CARRY_SET;
							}
							(*A) = (uint8_t) temp;
							SUB_RESET;
							PC++;
							return 4;
							break;
						case 5:
							//cpl; 0x2F; compliment / negate a
							*A = ~(*A);
							SUB_SET;
							HALF_SET;
							PC++;
							return 4;
							break;
						case 6:
							//scf; 0x37; set carry flag
							CARRY_SET;
							SUB_RESET;
							HALF_RESET;
							PC++;
							return 4;
							break;
						case 7:
							//ccf; 0x3F; compliment carry flag
							if(CARRY) CARRY_RESET;
							else CARRY_SET;
							SUB_RESET;
							HALF_RESET;
							PC++;
							return 4;
							break;
					}
					break;
			}	
			break;
		case 1:
			if(z==6 && y==6) {
				//HALT; 0x76; Stop until interrupt
				PC++;
				return 76;
			} else {
				//ld r(y), r(z); 0x40-0x75, 0x77-0x74; load 8 bit register into another.
				ld(reg(y),reg(z));
				PC++;
				if(y == 6 || z == 6) return 8;
				return 4;
			}
			break;
		case 2:
			//0x80-0xBF; alu operations on register
			alu(y,reg(z));
			PC++;
			if(z==6) return 8;
			return 4;
			break;
		case 3:
			switch(z){
				case 0:
					switch(y){
						case 0:
						case 1:
						case 2:
						case 3:
							if(con(y)) ret();
							return 8;
							break;
						case 4:
							ld(RAM+*n+0xFF00,A);
							PC+=2;
							return 12;
							break;
						case 5:
							//temporary prevents access to neighboring byte in memory
							add16(&SP,(uint16_t*)&temp_d);
							PC+=2;	
							return 16;
							break;
											
						case 6:
							ld(A,RAM+*n+0xFF00);
							PC+=2;
							return 12;
							break;
						case 7:
							ld16(&HL, &SP + *d);
							PC+=2;
							return 12;
							break;
							
					}	
					break;
				case 1:
					if(!q){
						pop(rp2(p));
						PC+=1;
						return 12;
					}
					else {
						switch(p){
							case 0:
								ret();
								return 8;
								break;
							case 1:
								// ; ;RETI
								//ENABLE INTERUPTS
								ret();
								c->ime = 0xFF;
								return 16;
								break;
							case 2:
								jp(&HL);
								return 4;
								break;
							case 3:
								ld16(&SP, &HL);
								PC++;
								return 8;
								break;
						}
					}
					break;
				case 2:
					switch(y){
						case 0:
						case 1:
						case 2:
						case 3:
							if(con(y)) jp(nn);
							else {PC+=3; return 12;}
							return 16;
							break;
						case 4:
							ld(RAM+*C+0xFF00,A);
							PC++;
							return 8;
							break;
						case 5:
							ld(RAM+*(nn),A);
							PC+=3;
							return 16;
							break;
						case 6:	
							ld(A, 0xFF00+*C+RAM);
							PC++;
							return 8;
							break;
						case 7:
							ld(A, RAM+*(nn));
							PC+=3;
							return 16;
							break;
					}
					break;
				case 3:
					switch(y){
						case 0:
							jp((nn));
							return 16;
							break;
						case 1:
							//PREFIX
							PC++;
							cpu->prefixed = 1;
							return 4;
							break;
						case 6: 
							//DISABLE INTERUPTS
							c->ime = 0;
							PC++;
							return 4;
							break;
						case 7:
							//ENABLE INTERRUPTS
							c->ime = 0xFF;
							PC++;
							return 4;
							break;
						default:
							//REMOVED INSTRUCTIONS	
							return 0;
							break;
					}
					break;
				case 4:
					switch(y){
						case 0:
						case 1:
						case 2:
						case 3:
							if(con(y)) {call(nn); return 24;}
							PC++;
							return 12;
							break;
						default:
							//REMOVED INSTRUCTIONS
							return 0;
							break;
					}
					break;
				case 5:
					if(!q){
						push(rp2(p));
						PC++;
						return 16; 
					} else {
						if(!p){
							call(nn);
							return 24;
						} 
						//ELSE, REMOVED INSTRUCTION
						return 0;
					}
					break;
				case 6:	
					//ALU IMMEDIATE 
					alu(y, n);
					PC+=2;
					return 8; 
					break;
				case 7:
					{
						//RESTART
						uint16_t temp = y*8;
						call(&temp);
						return 16;
						break;
					}
			}	
		break;	//case 3 x break
	}	
	return 0;
} 


