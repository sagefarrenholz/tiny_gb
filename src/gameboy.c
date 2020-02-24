#include "gameboy.h"
#include "z80gb.h"

CPU cpu;
#undef main

void tileRender(uint8_t* idx, unsigned x, unsigned y, uint32_t* buffer, uint32_t* palette, uint8_t scalar); 
void tileRenderLine(uint8_t* idx, unsigned x, unsigned y, unsigned line, uint32_t* buffer, uint32_t* palette, uint8_t scalar); 
void blankBuffer(uint32_t* buff, uint64_t size);

int main(int argc, char** argv) {

	//Debugging variables
	uint8_t dbg = 0, memdmp = 0, exelog = 0;
	char* exelog_loc = NULL, *memdmp_loc = NULL;

	//Rom File name
	char* romname = NULL;
	
	//Scaling factor for the game screen, 1 = 160*144 window
	int scalar = 3;

	//If the first argument equals -d open a debugging window
	if(argc > 1){
		for(int i = 1; i < argc; i++){
			//Options
			if(argv[i][0] == '-') {
				switch(argv[i][1]) {
					case 'd':
						// debugger
						dbg = 1;
						break;
					case 's':
						// sets the scalar from default of 3
						scalar = atoi(argv[i+1]);
						break;
					case 'h':
					case '-':
						// help
						printf("Sage's Tiny GB Emulator\nOptions:\n-h Prints this help blurb\n-s <num> Sets the graphics scaling \n");
						return 0;
						break; 
					case 'm':
						// memory dump
						memdmp = 1;
						memdmp_loc = argv[i+1];
						break;
					case 'l':
						// log / execution trace
						exelog = 1;
						exelog_loc = argv[i+1];
						break;
				}
			} else {
				//Rom name
				romname = argv[i];	
			}
		}
	}
	
	//if (!strcmp(memdmp_loc, romname)) memdmp_loc = "var/dump.hex";
	//if (!strcmp(exelog_loc, romname)) exelog_loc = "var/dump.hex";

	// No file was given
	if (!romname) {
		printf("You must enter a rom file name.\n");
		return -1;		
	}

	//The processor for this emulation instance
	Sharp_LR35902 processor;
	cpu = &processor;

	//Total RAM / mem buffer
	/*
	Memory map goes here
	*/
	uint8_t* ram = (uint8_t*) calloc(0xFFFF,1);
	RAM = ram;	
	PC = 0;

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
	//Pitch is the row width in bytes of the buffer sent to the texture used to render the gameboy screen
	uint64_t pitch = width * 4 * scalar;
	

	//ROM
	FILE* ROM = fopen(romname,"rb");
	//load rom, except lowest 256 bytes into memory
	fread(RAM, 0x7FFF, 1, ROM);
	fclose(ROM);

	//BOOTLOADER
	FILE* BOOT = fopen("resources/boot", "rb");
	//load bootloader into ram
	fread(RAM, 0xFF, 1, BOOT);
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
	//CURRENTLY NOT USED
	//uint8_t vram[0xFF] = {0};
	//uint8_t oam[0xFF] = {0};

	// Execution log / trace
	FILE* log;
	if(exelog)
		log = fopen(exelog_loc,"w+b");
	// Memory dump
	FILE* dump;
	if(memdmp)
		dump = fopen(memdmp_loc,"w+b");

	/*

	 [==========]	
	  INTERRUPTS
	 [==========]

	*/
	//Interrupts Enable Register, Interrupt checks, and each corresponding switch
	#define IN_EN_REG *(RAM+0xFFFF)

	//Interrupt checks
	#define VBLANK_IN (IN_EN_REG & 0x01)
	#define LCDC_IN (IN_EN_REG & 0x02)
	#define TIMER_IN (IN_EN_REG & 0x04)
	#define SERIAL_IN (IN_EN_REG & 0x08)
	#define BUTTON_IN (IN_EN_REG & 0x10)

	//Interrupt enable switches
	#define VBLANK_SWITCH (IN_EN_REG | 0x01)
	#define LCDC_SWITCH (IN_EN_REG | 0x02)
	#define TIMER_SWITCH (IN_EN_REG | 0x04)
	#define SERIAL_SWITCH (IN_EN_REG | 0x08)
	#define BUTTON_SWITCH (IN_EN_REG | 0x10)

	//TODO, figure out what interrupts are on by default
	//DISABLE all interrupts by default
	IN_EN_REG = 0x00;  	
	//DISABLE interrupt master switch
	cpu->ime = 0x00;
	
	SDL_Window* win;
	SDL_Renderer* ren;

	//init SDL
	assert(!SDL_Init(SDL_INIT_VIDEO));
	SDL_CreateWindowAndRenderer(width * scalar, height * scalar, 0, &win, &ren);	
	printf("%s", SDL_GetError());

	
	//Debug variables
	TTF_Font* debug_font;
	SDL_Window* win2;
	SDL_Renderer* ren2;
	SDL_Color white = { 0xFF, 0xFF, 0xFF, 0xFF };	
	SDL_Texture* text;
	SDL_Rect bound;
	char** strArr;
	
	//Debug window creation
	if(dbg) {
		//init TTF
		assert(!TTF_Init());

		//get font
		debug_font = TTF_OpenFont("../resources/fonts/font.ttf", 12); 

		//Create debug window
		SDL_CreateWindow("debug", 0, 0, 500, 500, 0);
		SDL_CreateWindowAndRenderer(500, 500, 0, &win2, &ren);	
	
		#ifndef DEBUG_VARIABLES 
		#define DEBUG_VARIABLES 12
		#endif
		strArr = (char**) malloc(sizeof(char*) * DEBUG_VARIABLES);
		for(int i = 0; i < DEBUG_VARIABLES; i++)
			strArr[i] = (char*) malloc(30);
	}
	//Sizing variables for rect
	int wide, tall;
	bound.x = 0;
	bound.y = 0;

	

	//Nanosecond time structures used for calculating high resolution time deltas
	/*struct timespec currtime, lasttime;
	clock_gettime(CLOCK_MONOTONIC_RAW, &lasttime);*/

	SDL_Event e;

	//delta time of instruction execution
	long dt_s = 0;
	//delta time since last frame draw
	long dv_s = 0;

	//buffer used for logging data
	char *buff = (char*) calloc(256,1);
	//used for correcting from slow instructions
	long time_stack = 0;
	long misscnt = 0;

	//RENDERING & EXECUTION VARIABLES
	//Execution loop cycle count, differs depending on whether vblanking or not
	int loop_count = 456;
	//Last address written to DMA Transfer register
	DMA = 0xFF;

	LCDC_STATUS_MODE(1);

	//Size of the framebuffer
	const size_t FRAMESIZE_BYTES = 160 * 144 * 4 * scalar * scalar;
	//Allocate a framebuffer, each pixel is 4 bytes (32 bit-depth w/ alpha)
	uint32_t* framebuffer = (uint32_t*) malloc(FRAMESIZE_BYTES);
	SDL_Texture* frame = SDL_CreateTexture(ren, SDL_PIXELFORMAT_BGRA32, SDL_TEXTUREACCESS_STREAMING, width * scalar, height * scalar);
	printf("%s", SDL_GetError());

	//Data used during OAM reading stage for determing which sprites to render
	uint8_t sprite_count = 0;
	uint8_t* sprite_array[10]; 

	//Rendering Color Palette, lightest to darkest!
	//ORIGINAL
	uint32_t palette[4] = {0xff9bbc0f, 0xff8bac0f, 0xff306230, 0xff0f380f};
	//MONOCHROME
	//uint32_t palette[4] = {0xffffffff, 0xffb6b6b6, 0xff676767, 0xff000000};
	//SpaceHaze
	//uint32_t palette[4] = {0xfff8e3c4, 0xffcc3495, 0xff6b1fb1, 0xff0b0630};

 	unsigned incer = 0;

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
				if(LCDC_IN){
					//if(/*LCDC compare to LY */0)
					PC = 0x48;
				}


				//TIMER OVERFLOW - PRIORITY 3
				if(TIMER_IN){
					//if(/*timer overflows*/0);
				}

				//SERIAL TRANSFER - PRIORITY 4

				//INPUT - PRIORITY 5
			}
			
			//If the DMA register is written to in RAM then perform a DMA memory transfer
			//DMA TRANSFER, 0xFF used to signify not written to
			if(DMA != 0xFF){
				printf("DMA");
				memcpy(RAM + 0xFE00, RAM + (DMA << 8), 0x9F);
				DMA = 0xFF;
			}	

			//Bypasses boot zeroing loop, memory and registers already zeroed
			//if(HL == 0x9fff) HL =  0x8001;
			
			//These routines run only once when the rendering mode changes
			//OAM SCANNING - MODE 2
			if(cyclecount < 80 && (LCDC_STATUS_MODE_GET == 1 || LCDC_STATUS_MODE_GET == 0) && loop_count == 456) {
				LCDC_STATUS_MODE(2);
				sprite_count = 0;
				/*
				 * This loop traverses the OAM table and adds sprites to the sprite_array which
				 * is used for rendering during MODE 3. It starts at address FE00 and moves to
				 * address FE9f. i increments by 4 because each entry is 4 bytes wide. The first
				 * two bytes are position y and x respsectively of each entry. 
				 */
				for(int i = 0; i <= 0x9f && sprite_count < 10; i += 4){
					//object at this address
					uint8_t* temp_obj = RAM + (0xFE00 | i);
					if(!temp_obj[2]) continue;
					uint8_t y = temp_obj[0];
					uint8_t x = temp_obj[1];
					uint8_t w = 8, h;
					//printf("Sprite collected: Address %x Position %u %u\n", temp_obj, x, y);
					if(LCDC_OBJ_SIZE) h = 16; else h = 8;
					//Not being drawn, out of bounds
					if(!x || x >= 168) continue;  
					if(y + h <= 16 || y >= 160) continue; 
					//If it intersects line then add it to draw list and increment sprite_count 
					if(LINE + 8 >= y && LINE + 8 <= y + (h - 1)){
						sprite_array[sprite_count++] = temp_obj;
					}
						
				}
			}
			//SCANLINE RENDERING - MODE 3
			else if(cyclecount >= 80 && LCDC_STATUS_MODE_GET == 2) {
				LCDC_STATUS_MODE(3);
				
				//BG RENDERING
				uint8_t* bg_map = LCDC_STATUS_BG_MAP ? RAM+0x9C00 : RAM+0x9800;
				uint8_t* bg_tile = LCDC_STATUS_TILE_DATA ? RAM+0x8000 : RAM+0x8800; 
				
				uint8_t pix, col;
				//Starting index in the background tile data
				uint8_t* idx = &bg_tile[bg_map[SCX % 8 + (SCY % 8) * 32]];				

				//Note that the parent loop is counting in bytes not pixels
				for(int u = 0; u < 160; u += 8){
					for(int z = 0; z < 2; z++){
						for(int i = 0; i < 4; i++){
							//Pixel value at this position
							pix = (*(RAM + LINE * 160 + u + z * 2) >> ((3 - i) * 2)) & 0x3;
							//printf("pix %x",pix);
							col = (BG_PAL >> (pix * 2)) & 0x3;
							//Copy corresponding color into framebuffer at current pixel x + i.
							framebuffer[LINE * 160 + u + z * 4 + i] = palette[col];
						}
					}
				}				
			
				//SPRITE RENDERING
				while(sprite_count > 0){
					
					//printf("Sprite 1: Address %x Position %u %u\n",sprite_array[0],sprite_array[0][0],sprite_array[0][1]);
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
					//FIX VARIABLE NAMES HERE
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
							else color = (pal >> (pixel * 2)) & 0x3;
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
			//clock_gettime(CLOCK_MONOTONIC_RAW, &lasttime);
			cycle = execute();	
			//clock_gettime(CLOCK_MONOTONIC_RAW, &currtime);
			//dt_s = currtime.tv_nsec - lasttime.tv_nsec;
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
			if(exelog){
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
			}
			cyclecount += cycle;
		}
		
		//LINE STEPPER
		//Increment the scanline that is being rendered
		if(LINE < 143) {
			//Increment LCDC register (LCD Y Coordinated), the line to be rendered
			//if statement ensures first line is rendered
			if(loop_count == 456) LINE++;
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
			if(IME && VBLANK_IN)
				PC = 0x40;
			
			//Blanking using memset
			memset(framebuffer, 0, FRAMESIZE_BYTES);

			//Just some testing of tileRender
			/*for(int r = 0; r < 18; r++){
				for(int col = 0; col < 20; col++){
					tileRender(RAM + (16 * (r * 20 + col + incer / 20)), col * 8 + 8, r * 8 + 16, framebuffer, palette, scalar);
					incer++;
				}
			}*/
			
			tileRenderLine(RAM + 0x0150, 8, 16, incer % 8, framebuffer, palette, scalar);
			incer+=2;
			
			//DISPLAY BUFFERED FRAME HERE
			SDL_RenderClear(ren);
			SDL_UpdateTexture(frame, NULL, (void*) framebuffer, pitch);
			SDL_RenderCopy(ren, frame, NULL, NULL);
			SDL_RenderPresent(ren);

			//Draw Debug window
			if (dbg) {
				//Create a texture of text
				
				sprintf(strArr[0], "BC 0x%x 0x%x", (BC & 0xFF00) >> 8, BC & 0xFF);
				sprintf(strArr[1], "DE 0x%x 0x%x", (DE & 0xFF00) >> 8, DE & 0xFF);
				sprintf(strArr[2], "HL 0x%x", HL);
				sprintf(strArr[3], "(HL) 0x%x", *(cpu->hl + RAM));
				sprintf(strArr[4], "A 0x%x", *A);
				sprintf(strArr[5], "SP 0x%x", SP);
				sprintf(strArr[6], "ZERO 0x%x", ZERO);
				sprintf(strArr[7], "SUB 0x%x", SUB);
				sprintf(strArr[8], "HALF 0x%x", HALF);
				sprintf(strArr[9], "CARRY 0x%x", CARRY);
				sprintf(strArr[10], "PC 0x%x", PC);
				sprintf(strArr[11], "IME 0x%x", IME);
				
				//ADD INTERRUPT SWITCH READOUT
				
				SDL_RenderClear(ren2);
				int lineDist = 0;
				for(int i = 0; i < DEBUG_VARIABLES; i++){
					TTF_SizeText(debug_font, strArr[i], &wide, &tall);
					bound.w = wide;
					bound.h = tall;
					bound.y = lineDist;
					text = SDL_CreateTextureFromSurface(ren2, TTF_RenderText_Solid(debug_font, strArr[i], white));
					SDL_RenderCopy(ren2, text, NULL, &bound);
					lineDist += tall + 5;	
				}
				bound.y = 0;
				SDL_RenderPresent(ren2);
			}
				
		}
		

		//Reset cycles used for this execution loop (either VBLANK or SCANLINE RENDER).
		cyclecount = 0;

		//dv_s = 0;
	}
	//mem dump
	if(memdmp){
		for(size_t i = 0; i < 0x10000; i++){
			fputc(RAM[i], dump); 
		}
	} 
	//CLEANUP
	if(memdmp)
		fclose(dump);
	if(exelog)
		fclose(log);
	free(framebuffer);
	free(buff);
        TTF_CloseFont(debug_font);
	SDL_DestroyTexture(frame);
	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(win);
	SDL_Quit();
	return 0;
}


// Rips an 8x8 tile starting at idx, converts it to BGRA32 using the given palette, places it in the buffer at (x, y), then scales it.
// Note 0, 0 is outside the view port. The origin (top-left) is actually (8, 16), this allows tiles to be off screen or drawn partially on screen. 
// Gameboy sprites can be 8x16 that is why the top bound is 16 rather than 8
// Assumes the buffer is 160 * 144 * 4 bytes at scale 1 
// WARNING! While tileRender scales tiles it does NOT scale the coordinate system, act as if the scaling is not happening (it is abstracted).
inline void tileRender(uint8_t* idx, unsigned x, unsigned y, uint32_t* buffer, uint32_t* palette, uint8_t scalar) {
	// Gameboy has a bitdepth of 4 so one byte contains 4 pixels.
	// A tile is 8x8 so 16 bytes [ie, 0x..00 -> 0x..10] contain one tile (	divide by 4 because of the above statement )
	
	// TODO implement a more complex way of doing this, that is more performant and less memory intensive by filling the buffer while reading out pixels
	// instead of using a temporary array.	

	uint32_t tile[64] = {0};
	// Conversion to BGRA32 array
	for (int i = 0; i < 16; i++){
		// SHIFTING LOOP
		for (int s = 0; s < 4; s++){
			/*
			The parent loop goes through each byte.
			This child SHIFTING LOOP shifts the byte getting every 2 bit pixel out. The 0x3 AND operation zeroes any higher bits leftover.
			*/
			tile[i * 4 + s] = palette[0x3 & (idx[i] >> (6 - 2 * s))];
		}
	}
	// Placing the tile onto the framebuffer
	// ROW LOOP
	for (int r = 0; r < 8; r++){
		// If the tile is past the bottom bound, we can end rendering, if y or x is 0 it is also out of bounds above or to the left of the viewport, so break
		// In other words, if any of these conditions are met the rest of the sprite is completely hidden
		if (y + r >= 16 + 144 || !y || !x) break;		
		// If beyond the top bound wait until in bounds, ie go down a few rows until it is visible
		if (y + r < 16) continue;
		// COLUMN LOOP
		for (int c = 0; c < 8; c++) {
			// if the tile is past the right bound, we can skip the rest of this row.
			if(x + c >= 8 + 160) return;
			// If beyond the left bound wait until in bound, too far left go some amount of columns to the right
			if(x + c < 8) continue;
			// Render the pixel to the buffer at position at position (x + c - 8, y + r - 16) 
			//buffer[(x + c - 8) + 160 * (y + r - 16)] = tile[c + 8 * r];	

			// SCALING FUNCTION
			// SCALING ROW LOOP
			for (int row = 0; row < scalar; row++){
				// SCALING COLUMN LOOP
				for (int col = 0; col < scalar; col++){
					// Sets each pixel in the scalar x scalar square to the tile color
					/*
					Essentially, what these nested loops do is go through the buffer and
					scaling each pixel to a series of pixel (square shape, maybe add rectangular
					scaling in the future) which has its own row and height dimensions defined
					by the scalar.
					*/
					buffer[scalar * (x + c - 8) + 160 * (y + r - 16) * scalar * scalar + col + row * scalar * 160] = tile[c + 8 * r];	
				}
			}
		}
	}

}

// tileRenderLine renders similarly to tileRender except it only renders onto one line of the buffer
// Rips an 8x8 tile starting at idx, converts it to BGRA32 using the given palette, places it in the buffer at (x, y), then scales it.
// Note 0, 0 is outside the view port. The origin (top-left) is actually (8, 16), this allows tiles to be off screen or drawn partially on screen. 
// Gameboy sprites can be 8x16 that is why the top bound is 16 rather than 8
// Assumes the buffer is 160 * 144 * 4 bytes at scale 1 
// WARNING! While tileRender scales tiles it does NOT scale the coordinate system, act as if the scaling is not happening (it is abstracted).
inline void tileRenderLine(uint8_t* idx, unsigned x, unsigned y, unsigned line,  uint32_t* buffer, uint32_t* palette, uint8_t scalar) {
	// Gameboy has a bitdepth of 4 so one byte contains 4 pixels.
	// A tile is 8x8 so 16 bytes [ie, 0x..00 -> 0x..10] contain one tile (	divide by 4 because of the above statement )
	
	// TODO implement a more complex way of doing this, that is more performant and less memory intensive by filling the buffer while reading out pixels
	// instead of using a temporary array.	
	// The line renderer can definitely be optimized with some semi-complex algebra

	uint32_t tile[64] = {0};
	// Conversion to BGRA32 array
	for (int i = 0; i < 16; i++){
		// SHIFTING LOOP
		for (int s = 0; s < 4; s++){
			/*
			The parent loop goes through each byte.
			This child SHIFTING LOOP shifts the byte getting every 2 bit pixel out. The 0x3 AND operation zeroes any higher bits leftover.
			*/
			tile[i * 4 + s] = palette[0x3 & (idx[i] >> (6 - 2 * s))];
		}
	}

	// If the tile is past the bottom bound, we can end rendering, if y or x is 0 it is also out of bounds above or to the left of the viewport, so break
	// In other words, if any of these conditions are met the rest of the sprite is completely hidden
	// If y + 8 - 16 is less than or equal to line then line is past the tile. If line is less than y - 16 than line is before the tile.
	if (y >= 16 + 144 || !y || !x || line >= (y + 8) - 16 || line < (y - 16)) return;		
	// COLUMN LOOP
	for (int c = 0; c < 8; c++) {
		// if the tile is past the right bound, we can skip the rest of this row.
		if(x + c >= 8 + 160) break;
		// If beyond the left bound wait until in bound, too far left go some amount of columns to the right
		if(x + c < 8) continue;
		// Render the pixel to the buffer at position at position (x + c - 8, y + r - 16) 
		// buffer[(x + c - 8) + 160 * (y + r - 16)] = tile[c + 8 * r];	

		// SCALING FUNCTION
		// SCALING ROW LOOP
		for (int row = 0; row < scalar; row++){
			// SCALING COLUMN LOOP
			for (int col = 0; col < scalar; col++){
				// Sets each pixel in the scalar x scalar square to the tile color
				/*
				Essentially, what these nested loops do is go through the buffer and
				scaling each pixel to a series of pixel (square shape, maybe add rectangular
				scaling in the future) which has its own row and height dimensions defined
				by the scalar.
				*/
				// If the tile contains line (it meets the checks with the return statement), then the row to render is line - (y - 16)
				buffer[scalar * (x + c - 8) + 160 * (line - (y - 16)) * scalar * scalar + col + row * scalar * 160] = tile[c + 8 * (line - (y - 16))];	
				// TODO MOVE SHIFTING LOOP / CONVERSION LOOP HERE
			}
		}
	}

}  

// TODO implement this function
// Instruction unit tester, allows manual execution of instructions, prints the result of the registers
// Instruction is a pointer to the instruction(s) to be executed, num is the amount of instruction executions to occur.
void instTest(uint8_t* instruction, uint64_t num){


}


