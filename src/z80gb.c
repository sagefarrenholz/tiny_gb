#include "z80gb.h"

/*

 [====================]
  EXECUTION ALGORITHIM
 [====================]

*/


inline int execute(){

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
	#define z ((*(RAM + PC) & 0x07))				//2-0 bits; 07 Mask 0000 0111
	#define p (y >> 1)								//y(5-4 bits)
	#define q (y % 2)								//y(3 bit)

	//temp d variable used to prevent bad access to high byte; also used by PREFIX OPCODE
	int16_t temp_d = (int16_t) *d;
	//rst define used to reference restarts for RST command
	#define rst ((uint16_t*) RAM + (y * 8))
	
	if(cpu->prefixed){
		cpu->prefixed = 0;
		switch(x){
			//Rotation, Shift, and Swap commands
			case 0:
				switch(y){
					case 0:
						//Rotate register left
						rlc(reg(z));
						break;
					case 1:
						//Rotate register right
						rrc(reg(z));
						break;
					case 2:
						//Rotate register left through carry
						rl(reg(z));
						break;
					case 3:
						//Rotate register right through carry
						rr(reg(z));
						break;
					case 4:
						//Shift carry left, low bit zeroed
						sl(reg(z));
						break;
					case 5:
						//Shift carry right, high bit zeroed
						sr(reg(z));
						break;
					case 6:
						//Swap high and low nibbles of a register
						swp(reg(z));
						break;
					case 7:
						//Shift carry right, high bit remains same
						srl(reg(z));
						break;

				}
				PC++;
				return 8; 
				break;
			//BIT TEST
			case 1:
				SUB_RESET;
				HALF_SET;
				ZERO_SET;
				if(*reg(z) & (1 << y)) ZERO_RESET;
				PC++;
				return 8;
				break;
			//BIT RESET
			case 2:
				*reg(z) &= ~(1 << y);
				PC++;
				return 8;
				break;
			//BIT SET
			case 4:	
				*reg(z) &= (1 << y);
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
						//printf("con(y-4) %i, y %x, d %i\n", con(y-4),y,d);
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
							//rrca; 0x0F; rotate a right
							rrc(A);
							PC+=1;
							return 4;
							break;
						case 2:
							//rla; 0x17; rotate a left through carry
							rl(A);
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


