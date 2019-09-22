#include "gameboy.h"
#include "z80gb.h"

CPU cpu;

int main(void) {
		
	//The processor for this emulation instance
	Sharp_LR35902 processor;
	cpu = &processor;
	//Stack pointer starts at 0xFFFE
	cpu->sp = 0xFFFE;
	//Program counter starts at 0x0100
	//cpu->pc = 0x0100;

	//Total RAM / mem buffer
	/*
	Memory map goes here
	*/
	uint8_t ram[0xFFFF] = {0};
	RAM = ram;	

	//One clock cycle period, 4 of them is one NOP Instruction.
	double period = ((1.0f / CLOCK_FREQ ) * 1000.0f);
	//cycle count
	long cyclecount = 0; 
	//cycle till next execute
	int cycle = 0;

	//instruction buffer for debugging and mode for scanline modes
	uint8_t preop = 0, op, mode;
	uint16_t addr;	

	//Window size
	uint64_t width = 160, height = 144;		
	
	//BOOTLOADER
	FILE* BOOT = fopen("resources/boot", "rb");
	//load bootloader into ram
	fread(RAM, 0x3FFF, 1, BOOT);
	fclose(BOOT);

	//ROM 

	//ram used by ppu
	//note that these rams also exist within the cpu's ram buffer but are actually shadow copies of the actual ppu ram
	uint8_t vram[0xFF] = {0};
	uint8_t oam[0xFF] = {0};

	//log
	FILE* log = fopen("log/log","w+b");
	//memory dump
	FILE* dump = fopen("log/dump","w+b");

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
	
	//Nanosecond time structures used for calculating high resolution time deltas
	struct timespec currtime, lasttime;
	clock_gettime(CLOCK_MONOTONIC_RAW, &lasttime);

	SDL_Event e;

	//delta time of instruction execution
	long dt_s = 0;
	//delta time since last frame draw
	long dv_s = 0;

	//buffer used for logging data
	char buff[256];
	//used for correcting from slow instructions
	long time_stack = 0;
	long misscnt;
	while(1){
		SDL_PollEvent(&e);
		//User has quit.
		if(e.type == SDL_QUIT) {
			sprintf(buff, "MISS PERCENTAGE: %f%%\nMISS COUNT: %li\n",(double) misscnt/cyclecount * 100.0, misscnt);
			fputs(buff,log);
			break;
		}
		//Button has been pressed. Input interrupt is enabled.
		if(cpu->ime && BUTTON_SWITCH){ 
				
		}
		
		//SCANLINE RENDER
		while(cyclecount < 456){
			SDL_PollEvent(&e);

			//Scanning OAM for sprites that share pixels at this coordinate
			if(cyclecount < 80) mode = 2;
			
			//Picture Generation Step, TODO mode 1 is variable based on sprites in vram, fix eventually
			else if(cyclecount >= 80 && cyclecount < 168) mode = 3;
			
			//Horizontal Blanking Period
			else if(cyclecount >= 168 ) mode = 0; 
			
			addr = cpu->pc;
			op = *(RAM + addr);
				
			//DELTA TIME AND CLOCK CALCULATIONS
			clock_gettime(CLOCK_MONOTONIC_RAW, &lasttime);
			cycle = execute();	
			clock_gettime(CLOCK_MONOTONIC_RAW, &currtime);
			dt_s = currtime.tv_nsec - lasttime.tv_nsec;
			//struct timespec t = {0, period * cycle - dt_s};
			//nanosleep(&t ,NULL);
			/*if(dt_s <= (long) period * cycle){
				if(time_stack <= 0){
					struct timespec t = {0, period * cycle - dt_s};
					assert(!nanosleep(&t ,NULL));
				} else if(time_stack - (period * cycle - dt_s) < 0){ 
					struct timespec t = {0, period * cycle - dt_s - time_stack};
					time_stack = 0;
					assert(!nanosleep(&t ,NULL));
				} else 
					time_stack -= period * cycle - dt_s;
			} else {*/
				//misscnt++;
				sprintf(buff, " \n**\nRegisters:\nBC 0x%x\nDE 0x%x\nHL 0x%x\n(HL) 0x%x\nA 0x%x\nSP 0x%x\n\nFlags:\nZero %u\nSubtract %u\nHalf-Carry %u\nCarry %u\n",
							cpu->bc,
							cpu->de,
							cpu->hl,
							*(cpu->hl + RAM),
							(cpu->af & 0xFF00) >> 8,
							cpu->sp,
							(cpu->af & 0x0080) >> 7,
							(cpu->af & 0x0040) >> 6,
							(cpu->af & 0x0020) >> 5,
							(cpu->af & 0x0010) >> 4);
			//time_stack += dt_s - cycle * period; 
			sprintf(buff + strlen(buff), "ACTUAL dt_s %li TARGET %f DIFFERENCE %f PREV_INSTRUCTION 0x%x INSTRUCTION 0x%x ADDRESS 0x%x TIME_STACK %li CYCLE_COUNT %li\n**\n\n",dt_s, period*cycle, dt_s - cycle * period, preop, op, addr, time_stack, cyclecount);
				fputs(buff,log);			
			//}
			preop = op;
			cyclecount += cycle;
		}
		cyclecount = 0;

		//DRAWING	
			
		//OAM SCANNING 		
		
				
		//dv_s = 0;
		//SDL_RenderCopy(ren, bg, NULL, rec);
		
		//draw
		SDL_RenderPresent(ren);
	
	}
	/*for(size_t i = 0; i < 0x10000; i++){
		fputc(cpu->ram[i], dump); 
	} */
	fclose(dump);
	fclose(log);
	free(rec);	
	SDL_DestroyTexture(bg);
	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(win);
	return 0;
}
