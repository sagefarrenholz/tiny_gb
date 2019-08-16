#include "gameboy.h"
#include "z80gb.h"

int main(void) {

	CPU cpu = (CPU) calloc(sizeof(Sharp_LR35902),1);
	
	//Stack pointer starts at 0xFFFE
	cpu->sp = 0xFFFE;
	//Program counter starts at 0x0100
	cpu->pc = 0x0100;

	//Total RAM / mem buffer
	/*
	Memory map goes here
	*/
	#define RAM cpu->ram
	RAM = (uint8_t*) calloc(0xFFFF,1);	

	//1 instruction period in nanoseconds; essentially equal to the time it takes for one NOP instruction; default i-period is 
	double period = ((1.0f / CLOCK_FREQ ) * 1000.0f);
	printf("period %f\n",period);
	//cycle count
	long cyclecount = 0; 
	//cycle till next execute
	int cycle = 0;

	//instruction buffer
	uint8_t op;	

	//Window size
	uint64_t width = 160, height = 144;		
	
	//rom file
	FILE* rom = fopen("rom", "rb");
	//load ROM in mem buffer
	fread(RAM, 0x3FFF, 1, rom );

	//log
	FILE* log = fopen("log","w+b");

	/*

	 [==========]	
	  INTERRUPTS
	 [==========]

	*/
	//Interrupts Enable Register and each switch
	#define IN_EN_REG *(RAM+0xFFFF)
	#define VBLANK_SWITCH (IN_EN_REG & 0x01)
	#define LCD_SWITCH (IN_EN_REG & 0x02)
	#define TIMER_SWITCH (IN_EN_REG & 0x04)
	#define SERIAL_SWITCH (IN_EN_REG & 0x08)
	#define BUTTON_SWITCH (IN_EN_REG & 0x10)


	//Enable all interrupts by default
	IN_EN_REG = 0xFF;  	
	//Enable interrupt master switch
	cpu->ime = 0xFF;
	
	SDL_Window* win;
	SDL_Renderer* ren;

	SDL_Init(SDL_INIT_VIDEO);
	SDL_CreateWindowAndRenderer(width, height, 0, &win, &ren);	

	uint32_t rmask, gmask, bmask, amask;

	#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    		rmask = 0xff000000;
    		gmask = 0x00ff0000;
    		bmask = 0x0000ff00;
    		amask = 0x000000ff;
	#else
    		rmask = 0x000000ff;
    		gmask = 0x0000ff00;
    		bmask = 0x00ff0000;
    		amask = 0xff000000;
	#endif
    	SDL_Surface* sur = SDL_CreateRGBSurface(0, width, height, 32,
                                   rmask, gmask, bmask, amask);	
	
	SDL_Rect* rec = (SDL_Rect*) calloc(sizeof(SDL_Rect),1);
	*rec = (SDL_Rect){0,0,8,8};

	SDL_FillRect(sur, NULL, SDL_MapRGB(sur->format, 255,0,0));	
	SDL_Texture* bg = SDL_CreateTextureFromSurface(ren, sur);
	printf("%s", SDL_GetError());
	
	int debug = 0;	
	
	struct timespec currtime, lasttime;
	clock_gettime(CLOCK_MONOTONIC, &lasttime);

	SDL_Event e;
	//delta time since last instruction execution
	long dt_s = 0;
	//delta time since last frame draw
	long dv_s = 0;
	//calibration delta time
	long dc_s = 0;
	long cnt = 0;
	

	char buff[256];
	
	while(1){
		//INPUT EVENTS
		if(cpu->ime && BUTTON_SWITCH){ 
			SDL_PollEvent(&e);
			if(e.type == SDL_QUIT) break;
		}		

		//DELTA TIME AND CLOCK CALCULATIONS
		clock_gettime(CLOCK_MONOTONIC, &currtime);
		dt_s += currtime.tv_nsec - lasttime.tv_nsec;
		dv_s += currtime.tv_nsec - lasttime.tv_nsec;
		lasttime.tv_nsec = currtime.tv_nsec;
		
		if(dv_s < (70224 * period)) {
			//EXECUTING
			//fetch instruction
			/*op = RAM[cpu->pc];
			char in;
			execute:
			if(debug){
				printf("\nCycle counter: %zu\n", cyclecount);	
				printf("Time since last instruction in us: %i\n", dt_s);
				printf("Max distance from perfect time: %i\n", dt_s - (period * (cycle/4)));
				printf("Instruction address: 0x%x\n", cpu->pc);
				printf("The last in address, of stack: 0x%x\n\n", cpu->sp);
				printf("Opcode to be executed: 0x%x\n", op);
					printf("y for next instruction, i for cpu readout, s to end debugging, or q to quit\n");
					scanf(" %c",&in);
					if(in == 'i') { 
						printf(" \nRegisters:\nBC 0x%x\nDE 0x%x\nHL 0x%x\n(HL) 0x%x\nA 0x%x\n\nFlags:\nZero %u\nSubtract %u\nHalf-Carry %u\nCarry %u\n",
							cpu->bc,
							cpu->de,
							cpu->hl,
							*(cpu->hl + RAM),
							(cpu->af & 0xFF00) >> 8,
							(cpu->af & 0x0080) >> 7,
							(cpu->af & 0x0040) >> 6,
							(cpu->af & 0x0020) >> 5,
							(cpu->af & 0x0010) >> 4);
						goto execute;
					}	
					if(in == 's') debug = 0; 
					if(in == 'q') return 1;
					if(in != 'y') goto execute;
			}*/
		
			cycle = execute(cpu, 0);	
			cyclecount += cycle;
			dt_s = 0;
		
		} else {
			sprintf(buff, "ACTUAL dv_s %li. TARGET %f. DIFFERENCE %f\n",dv_s, period*70224, dv_s - 70224 * period);//dt_s);
			fputs(buff,log);			
			//DRAWING		
			dv_s = 0;
			SDL_RenderCopy(ren, bg, NULL, rec);
			//CALIBRATION			
			//clock_gettime(CLOCK_MONOTONIC, &currtime);
			//dc_s = (currtime.tv_nsec - lasttime.tv_nsec) / (++cnt);	
		}		
		
		//draw
		SDL_RenderPresent(ren);
	
	}
	free(rec);	
	SDL_DestroyTexture(bg);
	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(win);
	return 0;
}
