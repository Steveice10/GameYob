/* Known graphical issues:
 * DMG sprite order
 * Vertical window split behavior
 */

#include "gbgfx.h"
#include "gameboy.h"
#include "romfile.h"
#include <math.h>
#include <SDL/SDL.h>
#include <GL/gl.h>

// public variables

bool probingForBorder;

int interruptWaitMode;
int scaleMode;
int scaleFilter;
u8 gfxMask;
volatile int loadedBorderType;
bool customBorderExists;
bool sgbBorderLoaded;



// private variables

u32 gbColors[4];
Uint32 pixels[32*32*64];
u8 tileCache[2][0x1800][8][8];

int tileSize;
int tileSigned = 0;
int tileAddr = 0x8000;
int BGMapAddr = 0x800;
int winMapAddr = 0x800;
int BGOn = 1;
int winOn = 0;
int scale = 3;

u32 bgPalettes[8][4];
u32* bgPalettesRef[8][4];
u32 sprPalettes[8][4];
u32* sprPalettesRef[8][4];

int changedTileQueueLength;
u16 changedTileQueue[0x300];
bool changedTile[2][0x180];

int changedTileInFrameQueueLength;
u16 changedTileInFrameQueue[0x300];
bool changedTileInFrame[2][0x180];
int dmaLine;
bool lineModified;

SDL_PixelFormat* format;
    

// For drawScanline / drawSprite

u8 spritePixels[256];
Uint32 spritePixelsTrue[256]; // Holds the palettized colors

u8 spritePixelsLow[256];
Uint32 spritePixelsTrueLow[256];

u8 bgPixels[256];
Uint32 bgPixelsTrue[256];
u8 bgPixelsLow[256];
Uint32 bgPixelsTrueLow[256];


bool bgPalettesModified[8];
bool sprPalettesModified[8];


bool openglInitialized = false;

// Private functions
void drawSprite(int scanline, int spriteNum);

void updateBgPalette(int paletteid);
void updateBgPaletteDMG();
void updateSprPalette(int paletteid);
void updateSprPaletteDMG(int paletteid);


// Function definitions

void doAtVBlank(void (*func)(void)) {
    func();
}
void initGFX()
{
    if (!openglInitialized) {
        gbColors[0] = 255;
        gbColors[1] = 192;
        gbColors[2] = 94;
        gbColors[3] = 0;

        //Set Clear Color
        glClearColor(0, 0, 0, 0);

        glOrtho(0, 160, 144, 0, -1, 1); //Sets orthographic (2D) projection

        glRasterPos2f(0, 0);
        glPixelZoom(scale, -scale);

        SDL_Surface* gbScreen = SDL_CreateRGBSurface(SDL_SWSURFACE, 256*scale, 256*scale, 32, 0, 0, 0, 0);
        format = gbScreen->format;

        openglInitialized = true;
    }

    bgPalettes[0][0] = SDL_MapRGB(format, 255, 255, 255);
    bgPalettes[0][1] = SDL_MapRGB(format, 192, 192, 192);
    bgPalettes[0][2] = SDL_MapRGB(format, 94, 94, 94);
    bgPalettes[0][3] = SDL_MapRGB(format, 0, 0, 0);
    sprPalettes[0][0] = SDL_MapRGB(format, 255, 255, 255);
    sprPalettes[0][1] = SDL_MapRGB(format, 192, 192, 192);
    sprPalettes[0][2] = SDL_MapRGB(format, 94, 94, 94);
    sprPalettes[0][3] = SDL_MapRGB(format, 0, 0, 0);
    sprPalettes[1][0] = SDL_MapRGB(format, 255, 255, 255);
    sprPalettes[1][1] = SDL_MapRGB(format, 192, 192, 192);
    sprPalettes[1][2] = SDL_MapRGB(format, 94, 94, 94);
    sprPalettes[1][3] = SDL_MapRGB(format, 0, 0, 0);

	for (int i=0; i<8; i++)
	{

		sprPalettesRef[i][0] = &sprPalettes[i][0];
		sprPalettesRef[i][1] = &sprPalettes[i][1];
		sprPalettesRef[i][2] = &sprPalettes[i][2];
		sprPalettesRef[i][3] = &sprPalettes[i][3];

		bgPalettesRef[i][0] = &bgPalettes[i][0];
		bgPalettesRef[i][1] = &bgPalettes[i][1];
		bgPalettesRef[i][2] = &bgPalettes[i][2];
		bgPalettesRef[i][3] = &bgPalettes[i][3];
	}

    memset(bgPalettesModified, 0, sizeof(bgPalettesModified));
    memset(sprPalettesModified, 0, sizeof(sprPalettesModified));
}

void refreshGFX() {

}

void clearGFX() {

}

void drawScanline(int scanline)
{
    for (int i=0; i<8; i++) {
        if (bgPalettesModified[i]) {
            if (gameboy->gbMode == GB)
                updateBgPaletteDMG();
            else
                updateBgPalette(i);
            bgPalettesModified[i] = false;
        }
        if (sprPalettesModified[i]) {
            if (gameboy->gbMode == GB)
                updateSprPaletteDMG(i);
            else
                updateSprPalette(i);
            sprPalettesModified[i] = false;
        }
    }

	if (gameboy->ioRam[0x40] & 0x10)	// Tile Data location
	{
		tileAddr = 0x8000;
		tileSigned = 0;
	}
	else
	{
		tileAddr = 0x8800;
		tileSigned = 1;
	}

	if (gameboy->ioRam[0x40] & 0x8)		// Tile Map location
	{
		BGMapAddr = 0x1C00;
	}
	else
	{
		BGMapAddr = 0x1800;
	}
	if (gameboy->ioRam[0x40] & 0x40)
		winMapAddr = 0x1C00;
	else
		winMapAddr = 0x1800;
	if (gameboy->ioRam[0x40] & 0x20)
		winOn = 1;
	else
		winOn = 0;

	for (int i=0; i<256; i++)
	{
	//	pixels[i+(scanline*256)] = 0;
		bgPixels[i] = 5;
		bgPixelsLow[i] = 5;
		spritePixels[i] = 0;
		spritePixelsLow[i] = 0;
	}
	if (gameboy->ioRam[0x40] & 0x2)
	{
		for (int i=39; i>=0; i--)
		{
			drawSprite(scanline, i);
		}
	}

	if (BGOn)
	{
		u8 scrollX = gameboy->ioRam[0x43];
		int scrollY = gameboy->ioRam[0x42];
		// The y position (measured in tiles)
		int tileY = ((scanline+scrollY)&0xFF)/8;
		for (int i=0; i<32; i++)
		{
			int mapAddr = BGMapAddr+i+(tileY*32);		// The address (from beginning of gameboy->vram) of the tile's mapping
			// This is the tile id.
			int tileNum = gameboy->vram[0][mapAddr];
			if (tileSigned)
				tileNum = ((s8)tileNum)+128+0x80;

			// This is the tile's Y position to be read (0-7)
			int pixelY = (scanline+scrollY)%8;
			int flipX = 0, flipY = 0;
			int bank = 0;
			int paletteid = 0;
			int priority = 0;

			if (gameboy->gbMode == CGB)
			{
				flipX = !!(gameboy->vram[1][mapAddr] & 0x20);
				flipY = !!(gameboy->vram[1][mapAddr] & 0x40);
				bank = !!(gameboy->vram[1][mapAddr] & 0x8);
				paletteid = gameboy->vram[1][mapAddr] & 0x7;
				priority = !!(gameboy->vram[1][mapAddr] & 0x80);
			}

			if (flipY)
			{
				pixelY = 7-pixelY;
			}

			for (int x=0; x<8; x++)
			{
				int colorid;
				u32 color;

				if (flipX)
				{
					colorid = !!(gameboy->vram[bank][(tileNum<<4)+(pixelY<<1)] & (0x80>>(7-x)));
					colorid |= !!(gameboy->vram[bank][(tileNum<<4)+(pixelY<<1)+1] & (0x80>>(7-x)))<<1;
				}
				else
				{
					colorid = !!(gameboy->vram[bank][(tileNum<<4)+(pixelY<<1)] & (0x80>>x));
					colorid |= !!(gameboy->vram[bank][(tileNum<<4)+(pixelY<<1)+1] & (0x80>>x))<<1;
				}
				// The x position to write to pixels[].
				u32 writeX = ((i*8)+x-scrollX)&0xFF;

				color = *bgPalettesRef[paletteid][colorid];
				if (priority)
				{
					bgPixels[writeX] = colorid;
					bgPixelsTrue[writeX] = color;
				}
				else
				{
					bgPixelsLow[writeX] = colorid;
					bgPixelsTrueLow[writeX] = color;
				}
				//spritePixels[writeX] = 0;
			}
		}
	}
	// Draw window
	int winX = gameboy->ioRam[0x4B]-7;
	int winY = gameboy->ioRam[0x4A];
	if (scanline >= winY && winOn)
	{
		int tileY = (scanline-winY)/8;
		for (int i=0; i<32; i++)
		{
			int mapAddr = winMapAddr+i+(tileY*32);
			// This is the tile id.
			int tileNum = gameboy->vram[0][mapAddr];
			if (tileSigned)
				tileNum = ((s8)tileNum)+128+0x80;

			int pixelY = (scanline-winY)%8;
			int flipX = 0, flipY = 0;
			int bank = 0;
			int paletteid = 0;
			int priority = 0;

			if (gameboy->gbMode == CGB)
			{
				flipX = !!(gameboy->vram[1][mapAddr] & 0x20);
				flipY = !!(gameboy->vram[1][mapAddr] & 0x40);
				bank = !!(gameboy->vram[1][mapAddr]&0x8);
				paletteid = gameboy->vram[1][mapAddr]&0x7;
				priority = !!(gameboy->vram[1][mapAddr] & 0x80);
			}

			if (flipY)
				pixelY = 7-pixelY;
			for (int x=0; x<8; x++)
			{
				int colorid;
				u32 color;

				if (flipX)
				{
					colorid = !!(gameboy->vram[bank][(tileNum<<4)+(pixelY<<1)] & (0x80>>(7-x)));
					colorid |= !!(gameboy->vram[bank][(tileNum<<4)+(pixelY<<1)+1] & (0x80>>(7-x)))<<1;
				}
				else
				{
					colorid = !!(gameboy->vram[bank][(tileNum<<4)+(pixelY<<1)] & (0x80>>x));
					colorid |= !!(gameboy->vram[bank][(tileNum<<4)+(pixelY<<1)+1] & (0x80>>x))<<1;
				}

				int writeX = (i*8)+x+winX;
				if (writeX >= 168)
					break;

				color = *bgPalettesRef[paletteid][colorid];
				if (priority)
				{
					bgPixels[writeX] = colorid;
					bgPixelsTrue[writeX] = color;
					bgPixelsLow[writeX] = 5;
				}
				else
				{
					bgPixelsLow[writeX] = colorid;
					bgPixelsTrueLow[writeX] = color;
					bgPixels[writeX] = 5;
				}
			}
		}
	}
	//if (gameboy->ioRam[0x40] & 0x2)
	{
		int dest = scanline*256;
		if (gameboy->ioRam[0x40] & 0x1)
		{
			for (int i=0; i<256; i++, dest++)
			{
				pixels[dest] = spritePixelsTrueLow[i];
				if ((bgPixelsLow[i] > 0 || spritePixelsLow[i] == 0) && bgPixelsLow[i] != 5)
					pixels[dest] = bgPixelsTrueLow[i];
				if (spritePixels[i] != 0)
					pixels[dest] = spritePixelsTrue[i];
				if ((bgPixels[i] != 0 || (spritePixels[i] == 0 && spritePixelsLow[i] == 0)) && bgPixels[i] != 5)
					pixels[dest] = bgPixelsTrue[i];
				/*if (spritePixels[i] != 0)
				  {
				  pixels[i+(scanline*256)] = spritePixelsTrue[i];
				  spritePixels[i] = 0;
				  }
				 */
			}
		}
		else
		{
			//printf("kewl");
			for (int i=0; i<256; i++, dest++)
			{
				if ((bgPixelsLow[i] > 0) && bgPixelsLow[i] != 5)
					pixels[dest] = bgPixelsTrueLow[i];
				if ((bgPixels[i] != 0) && bgPixels[i] != 5)
					pixels[dest] = bgPixelsTrue[i];

				if (spritePixelsLow[i] != 0)
					pixels[dest] = spritePixelsTrueLow[i];
				if (spritePixels[i] != 0)
					pixels[dest] = spritePixelsTrue[i];
				/*if (spritePixels[i] != 0)
				  {
				  pixels[i+(scanline*256)] = spritePixelsTrue[i];
				  spritePixels[i] = 0;
				  }
				 */
			}
		}
	}
}

void drawScanline_P2(int scanline) {

}

void drawScreen()
{
    glDrawPixels(256, 144, GL_BGRA, GL_UNSIGNED_BYTE, pixels);

	SDL_GL_SwapBuffers();

	/*
	Uint32* screenPixels = gbScreen->pixels;
	int t,x,y;
	int var;
	for (t=0; t<256*144; t++)
	{
		screenPixels[t] = pixels[t];
	}
	SDL_Rect rect;
	rect.x = 0;
	rect.y = 0;
	rect.w = 160*scale;
	rect.h = 144*scale;
	SDL_BlitSurface(gbScreen, &rect, screen, NULL);
	SDL_Flip(screen);*/
}


void displayIcon(int iconid) {

}


void selectBorder() {

}

int loadBorder(const char* filename) {

}

void checkBorder() {

}

void refreshScaleMode() {

}


// SGB stub functions
void refreshSgbPalette() {

}
void setSgbMask(int mask) {

}
void setSgbTiles(u8* src, u8 flags) {

}
void setSgbMap(u8* src) {

}


void writeVram(u16 addr, u8 val) {
    gameboy->vram[gameboy->vramBank][addr] = val;
}

void writeVram16(u16 addr, u16 src) {
    for (int i=0; i<16; i++) {
        gameboy->vram[gameboy->vramBank][addr++] = gameboy->readMemory(src++);
    }
}

void writeHram(u16 addr, u8 val) {
}

void handleVideoRegister(u8 ioReg, u8 val) {
    switch(ioReg) {
        case 0x47:
            if (gameboy->gbMode == GB)
                bgPalettesModified[0] = true;
            break;
        case 0x48:
            if (gameboy->gbMode == GB)
                sprPalettesModified[0] = true;
            break;
        case 0x49:
            if (gameboy->gbMode == GB)
                sprPalettesModified[1] = true;
            break;
        case 0x69:
            if (gameboy->gbMode == CGB)
                bgPalettesModified[(gameboy->ioRam[0x68]/8)&7] = true;
            break;
        case 0x6B:
            if (gameboy->gbMode == CGB)
                sprPalettesModified[(gameboy->ioRam[0x6A]/8)&7] = true;
            break;
        default:
            break;
    }
}

void updateBgPalette(int paletteid)
{
    double multiplier = 8;
    int i;
    for (i=0; i<4; i++)
    {
        int red = (gameboy->bgPaletteData[(paletteid*8)+(i*2)]&0x1F)*multiplier;
        int green = (((gameboy->bgPaletteData[(paletteid*8)+(i*2)]&0xE0) >> 5) |
                ((gameboy->bgPaletteData[(paletteid*8)+(i*2)+1]) & 0x3) << 3)*multiplier;
        int blue = ((gameboy->bgPaletteData[(paletteid*8)+(i*2)+1] >> 2) & 0x1F)*multiplier;
        bgPalettes[paletteid][i] = SDL_MapRGB(format, red, green, blue);
    }
}

void updateBgPaletteDMG()
{
    u8 val = gameboy->ioRam[0x47];
    u8 palette[] = {val&3, (val>>2)&3, (val>>4)&3, (val>>6)};

    int paletteid = 0;
    int multiplier = 8;
    for (int i=0; i<4; i++) {
        u8 col = palette[i];
        int red = (gameboy->bgPaletteData[(paletteid*8)+(col*2)]&0x1F)*multiplier;
        int green = (((gameboy->bgPaletteData[(paletteid*8)+(col*2)]&0xE0) >> 5) |
                ((gameboy->bgPaletteData[(paletteid*8)+(col*2)+1]) & 0x3) << 3)*multiplier;
        int blue = ((gameboy->bgPaletteData[(paletteid*8)+(col*2)+1] >> 2) & 0x1F)*multiplier;

        bgPalettes[0][i] = SDL_MapRGB(format, red, green, blue);
    }
}

void updateSprPalette(int paletteid)
{
    double multiplier = 8;
    int i;
    for (i=0; i<4; i++)
    {
        int red = (gameboy->sprPaletteData[(paletteid*8)+(i*2)]&0x1F)*multiplier;
        int green = (((gameboy->sprPaletteData[(paletteid*8)+(i*2)]&0xE0) >> 5) |
                ((gameboy->sprPaletteData[(paletteid*8)+(i*2)+1]) & 0x3) << 3)*multiplier;
        int blue = ((gameboy->sprPaletteData[(paletteid*8)+(i*2)+1] >> 2) & 0x1F)*multiplier;
        sprPalettes[paletteid][i] = SDL_MapRGB(format, red, green, blue);
    }
}

void updateSprPaletteDMG(int paletteid)
{
    u8 val = gameboy->ioRam[0x48+paletteid];
    u8 palette[] = {val&3, (val>>2)&3, (val>>4)&3, (val>>6)};

    int multiplier = 8;
    for (int i=0; i<4; i++) {
        u8 col = palette[i];
        int red = (gameboy->sprPaletteData[(paletteid*8)+(col*2)]&0x1F)*multiplier;
        int green = (((gameboy->sprPaletteData[(paletteid*8)+(col*2)]&0xE0) >> 5) |
                ((gameboy->sprPaletteData[(paletteid*8)+(col*2)+1]) & 0x3) << 3)*multiplier;
        int blue = ((gameboy->sprPaletteData[(paletteid*8)+(col*2)+1] >> 2) & 0x1F)*multiplier;
        sprPalettes[paletteid][i] = SDL_MapRGB(format, red, green, blue);
    }
}

void drawSprite(int scanline, int spriteNum)
{
	// The sprite's number, times 4 (each uses 4 bytes)
	spriteNum *= 4;
	int tileNum = gameboy->hram[spriteNum+2];
	int x = (gameboy->hram[spriteNum+1]-8);
	int y = (gameboy->hram[spriteNum]-16);
	int height;
	if (gameboy->ioRam[0x40] & 0x4)
		height = 16;
	else
		height = 8;
	int bank = 0;
	int flipX = (gameboy->hram[spriteNum+3] & 0x20);
	int flipY = (gameboy->hram[spriteNum+3] & 0x40);
	int priority = !(gameboy->hram[spriteNum+3] & 0x80);
	int paletteid;

	if (gameboy->gbMode == CGB)
	{
		bank = !!(gameboy->hram[spriteNum+3]&0x8);
		paletteid = gameboy->hram[spriteNum+3] & 0x7;
	}
	else
	{
		//paletteid = gameboy->hram[spriteNum+3] & 0x7;
		paletteid = !!(gameboy->hram[spriteNum+3] & 0x10);
	}

	if (height == 16)
		tileNum &= ~1;
	if (scanline >= y && scanline < y+height)
	{
		if (scanline-y >= 8)
			tileNum++;

		//u8* tile = &memory[0]+((tileNum)*16)+0x8000;//tileAddr;
		int pixelY = (scanline-y)%8;
		int j;

		if (flipY)
		{
			pixelY = 7-pixelY;
			if (height == 16)
				tileNum = tileNum^1;
		}
		for (j=0; j<8; j++)
		{
			int color;
			int trueColor;

			color = !!(gameboy->vram[bank][(tileNum<<4)+(pixelY<<1)] & (0x80>>j));
			color |= !!(gameboy->vram[bank][(tileNum<<4)+(pixelY<<1)+1] & (0x80>>j))<<1;
			if (color != 0)
			{
				trueColor = *sprPalettesRef[paletteid][color];
				u32* trueDest = (priority ? spritePixelsTrue : spritePixelsTrueLow);
				u8* idDest = (priority ? spritePixels : spritePixelsLow);

				if (flipX)
				{
					idDest[(x+(7-j))&0xFF] = color;
					trueDest[(x+(7-j))&0xFF] = trueColor;
				}
				else
				{
					idDest[(x+j)&0xFF] = color;
					trueDest[(x+j)&0xFF] = trueColor;
				}
			}
		}
	}
}

