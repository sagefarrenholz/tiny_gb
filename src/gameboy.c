#include "gameboy.h"
#include "z80gb.h"

CPU cpu;

void render();

int main(void) {
		
	//The processor for this emulation instance
	Sharp_LR35902 processor;
	cpu = &processor;

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

	PC = 0x100;
	//Window size
	uint64_t width = 160, height = 144;		

	//ROM
	FILE* ROM = fopen("resources/mario.rom","rb");
	//load rom, except lowest 256 bytes into memory
	//fread(RAM, 0x7FFF, 1, ROM);

	//BOOTLOADER
	FILE* BOOT = fopen("resources/boot", "rb");
	//load bootloader into ram
	fread(RAM, 0x3FFF, 1, BOOT);
	fclose(BOOT);

	/*
	 * Upon powering up the gameboy runs a 256 byte bootloader program, this program is situated within the first
	 * 0xFF bytes of memory, before the program finishes it writes to a special register that allows the lower 
	 * 256 of bytes of the cartridge to replace it in memorty.
	 *
	 * This program plays the Nintendo Logo intro, as well as a few other miscallaneous functions like checking
	 * if the game is pirated.
	 */

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
	#define LCDC_SWITCH (IN_EN_REG & 0x02)
	#define TIMER_SWITCH (IN_EN_REG & 0x04)
	#define SERIAL_SWITCH (IN_EN_REG & 0x08)
	#define BUTTON_SWITCH (IN_EN_REG & 0x10)

	//TODO, figure out what interrupts are on by default
	//DISABLE all interrupts by default
	IN_EN_REG = 0x00;  	
	//DISABLE interrupt master switch
	cpu->ime = 0x00;
	
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
	*rec = (SDL_Rect){0,0,width,height};

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

	//RENDERING & EXECUTION VARIABLES
	//Execution loop cycle count, differs depending on whether vblanking or not
	int loop_count = 456;
	//Last address written to DMA Transfer register
	DMA = 0xFF;

	LCDC_STATUS_MODE(1);

	uint32_t* framebuffer = (uint32_t*) malloc(160*144);
	SDL_Texture* frame = SDL_CreateTexture(ren, SDL_PIXELFORMAT_BGRA32, SDL_TEXTUREACCESS_STREAMING, width, height);

	//Data used during OAM reading stage for determing which sprites to render
	uint8_t sprite_count = 0;
	uint8_t* sprite_array[10]; 

	//Rendering Color Palette, lightest to darkest
	uint32_t palette[4] = {0xff9bbc0f, 0xff8bac0f, 0xff306230, 0xff0f380f};
	//uint32_t palette[4] = {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};
	//for(int i = 0; i < 160*144; i++) framebuffer[i] = palette[0];

	while(1){
		//EXECUTION LOOP
		while(cyclecount < loop_count){

			//Poll for events, ie button presses.
			SDL_PollEvent(&e);
			
			//META INTERRUPTS
				//QUIT TINY_GB
				if(e.type == SDL_QUIT) {
					sprintf(buff, "MISS PERCENTAGE: %f%%\nMISS COUNT: %li\n",(double) misscnt/cyclecount * 100.0, misscnt);
					fputs(buff,log);
					return 0;
				}				


			//GAMEBOY INTERRUPT ROUTINES
			//Only run interrupt routines if IME is enabled via EI instruction. 
			if(IME){
				//LCDC STATUS - PRIORITY 2
				if(LCDC_SWITCH){
					if(/*LCDC compare to LY */0)
					PC = 0x48;
				}


				//TIMER OVERFLOW - PRIORITY 3
				if(TIMER_SWITCH){
					if(/*timer overflows*/0);
				}

				//SERIAL TRANSFER - PRIORITY 4

				//INPUT - PRIORITY 5
			}
			
			//If the DMA register is written to in RAM then perform a DMA memory transfer
			//DMA TRANSFER, 0xFF used to signify not written to
			if(DMA != 0xFF){
				memcpy(RAM + 0xFE00, RAM + (DMA << 8), 0x9F);
				DMA = 0xFF;
			}	

			//Bypasses boot zeroing loop, memory and registers already zeroed
			//if(HL == 0x9fff) HL =  0x8001;
			
			//These routines run only once when the rendering mode changes
			//OAM SCANNING - MODE 2
			if(cyclecount < 80 && LCDC_STATUS_MODE_GET == 1 && loop_count == 456) {
				LCDC_STATUS_MODE(2);
				sprite_count = 0;
				/*
				 * This loop traverses the OAM table and adds sprites to the sprite_array which
				 * is used for rendering during MODE 3. It starts at address FE00 and moves to
				 * address FE9f. i increments by 4 because each entry is 4 bytes wide. The first
				 * two bytes are position y and x respsectively of each entry. 
				 */
				for(int i = 0; i <= 0x9f && sprite_count < 10; i+=4){
					//object at this address
					uint8_t* temp_obj = RAM + (0xFE00 | i);
					if(!temp_obj[2]) continue;
					uint8_t y = temp_obj[0];
					uint8_t x = temp_obj[1];
					uint8_t w = 8, h;
					if(LCDC_OBJ_SIZE) h = 16; else h = 8;
					//Not being drawn, out of bounds
					if(!x || x >= 168) continue;  
					if(y + h <= 16 || y >= 160) continue; 
					//If it intersects line then add it to draw list and increment sprite_count 
					if(LINE + 8 >= y && LINE + 8 <= y + (h - 1))
						sprite_array[sprite_count++] = temp_obj;
						
				}
			}
			//SCANLINE RENDERING - MODE 3
			else if(cyclecount >= 80 && LCDC_STATUS_MODE_GET == 2) {
				LCDC_STATUS_MODE(3);
					
			
				//SPRITE RENDERING
				while(sprite_count > 0){
					/*
					This loop copies sprites from vram into the frame buffer for rendering.
					Each 2 bit pixel value is translated from its corresponding palette then
					read into a byte in the frame buffer. For example, take a pixel where the value
					is 0b01, first a lookup into the palette takes place. Say this value corresponds
					to white. Then white is placed into the corresponding byte of the frame buffer.
					*/
					uint8_t* curr_obj = sprite_array[--sprite_count];
					int y = curr_obj[0];
					int x = curr_obj[1];
					//Note, 4 pixels per byte. Sprites are 2 bytes across
					//Location of this sprite in memory
					uint8_t* loc = RAM + 0x8000 + curr_obj[2];
					//Which pallete is being used 0 - 0xFF48, 1 - 0xFF49
					uint8_t pal = *(RAM + 0xFF48 + ((curr_obj[3] & 0x80) >> 3));
					//Row byte  on the sprite
					uint8_t* byte = loc + ((LINE - y) * 2);
					//Pixel value and associated color value
					uint8_t pixel, color;
					//Each sprite is two bytes wide so the byte rendering loop occurs twice per row
					for(int i = 0; i < 2; i++){
						byte += i;
						//This loop renders over every in-the-bounds pixel
						for(int i = 0; i < 4; i++){
							//TODO, add x and y flip
							//Out of bounds (right bound), quit: all bits right of this are out of bounds
							if(x + i > 167) break;
							//Out of bounds (left bound)
							if(x + i < 8) continue;
							//Pixel value at this position
							pixel = (*byte >> ((3 - i) * 2)) & 0x3;
							//Color value lookup, takes value at pixel and translates it using its palette
							//Color of 0 valued pixels is always 0: transparent, go to next pixel.
							if(!pixel) continue; 	
							else color = (pal >> (i * 2)) & 0x3;
							//Copy corresponding color into framebuffer at current pixel x + i.
							framebuffer[x + i + LINE * 160] = palette[color];
						}
					}
				}			
			}

			//HBLANK - MODE 0
			else if(cyclecount >= 168 && LCDC_STATUS_MODE_GET == 3) {LCDC_STATUS_MODE(0);}
			
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
							BC,
							DE,
							HL,
							*(cpu->hl + RAM),
							*A,
							SP,
							ZERO,
							SUB,
							HALF,
							CARRY);
			//time_stack += dt_s - cycle * period; 
			sprintf(buff + strlen(buff), "ACTUAL dt_s %li TARGET %f DIFFERENCE %f PREV_INSTRUCTION 0x%x INSTRUCTION 0x%x ADDRESS 0x%x TIME_STACK %li CYCLE_COUNT %li\n**\n\n",dt_s, period*cycle, dt_s - cycle * period, preop, op, addr, time_stack, cyclecount);
				fputs(buff,log);			
			//}
			preop = op;
			cyclecount += cycle;
		}
		
		//LINE STEPPER
		//Increment the scanline that is being rendered
		if(LINE < 143) {
			//Increment LCDC register (LCD Y Coordinated), the line to be rendered
			LINE++;
			loop_count = 456;

			//Set coincidence bit for the scanline to be rendered. 
			if(LYC == LINE) 
				LCDC_STATUS_COINCIDENCE(1);
			else 
				LCDC_STATUS_COINCIDENCE(0);

		//VBLANK, display buffered frame, and reset to top line
		} else {
			LINE = 0;
			//VBLANK: set mode to 1
			LCDC_STATUS_MODE(1);
			//Loop count is set to length of vblank period in cycles
			loop_count = 4560;
			//If VBLANK Interrupt is enabled, activate vblank interrupt routine
			if(IME && VBLANK_SWITCH)
				PC = 0x40;

			//DISPLAY BUFFERED FRAME HERE
			//Lock frame texture, allows framebuffer to be copied to texture.
			//SDL_RenderClear(ren);
			SDL_UpdateTexture(frame, NULL, framebuffer, width);
			SDL_RenderCopy(ren, frame, NULL, NULL);
			SDL_RenderPresent(ren);
		}

		//Reset cycles used for this execution loop (either VBLANK or SCANLINE RENDER).
		cyclecount = 0;

		//dv_s = 0;
	}
	/*for(size_t i = 0; i < 0x10000; i++){
		fputc(cpu->ram[i], dump); 
	} */
	//CLEANUP
	fclose(dump);
	fclose(log);
	free(rec);	
	free(framebuffer);
	SDL_DestroyTexture(bg);
	SDL_DestroyTexture(frame);
	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(win);
	return 0;
}

void render(){

	


}
