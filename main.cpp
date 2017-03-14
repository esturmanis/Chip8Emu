#include <stdlib.h>
#include <stdio.h>
#include <cstring>

#include <Windows.h>

#include "OpCodes.h"
#include "Font.h"


typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned u32;
typedef char c8;
typedef wchar_t c16;
typedef char s8;

#define KEY_COUNT 16
#define STACK_SIZE 16
#define RAM_SIZE (4*1024)	//4K
#define REGISTER_SIZE 16

#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 32

#define REFRESH_RATE 60	// Hz

#define SET_PIXEL '@'
#define CLEAR_PIXEL ' '

#define PROGRAM_NAME "c8games\\PONG"

#define PRINTF(format, ...) printf(format, __VA_ARGS__);
#define PRINTF_UPDATE(format, ...) printf(format, __VA_ARGS__);

#define PRINT_OPCODE_DESC 0
#define DEBUG_PRINT_ON 0

#define INPUT_UPDATE 0	// every 60 frames

struct Program {
	u16 mStack[STACK_SIZE];
	u16 mPC;	// program counter
	u16 mI;		// register address
	u8 mSP;		// stack pointer
	u8 mDelayTimer;
	u8 mSoundTimer;
	u8 mInputTimer;

	u8 mRam[RAM_SIZE];
	u8 mRegister[REGISTER_SIZE];
	u8 mGBuffer[SCREEN_WIDTH * SCREEN_HEIGHT];	// graphics buffer
	u8 mIBuffer[KEY_COUNT];	// input buffer

	Program() {
		Reset();
	}

	void Reset() {
		mPC = 0x2000;
		mSP = 0x00;
		mDelayTimer = 0;
		mSoundTimer = 0;
		mInputTimer = INPUT_UPDATE;

		memset(mStack, 0, sizeof(u16) * STACK_SIZE);
		memset(mRam, 0, sizeof(u8) * RAM_SIZE);
		memset(mRegister, 0, sizeof(u8) * REGISTER_SIZE);
		memset(mGBuffer, 0, sizeof(u8) * SCREEN_WIDTH * SCREEN_HEIGHT);
		memset(mIBuffer, 0, sizeof(u8) * KEY_COUNT);
	}
};

void DebugPrintFont(u8* font, u32 fontSize);
void LoadFont(u8* ram, u8* font, u32 fontSize);
void InitInput(u8* inputBuffer, u32 keyCount);
void InitGraphics(u8* gBuffer, u32 width, u32 height);
s8 LoadProgram(const c8* programName, u8* ram);
void RunProgram(Program& program);

static const u8 KEY_MAPPING[16] = 
{
	0x31,0x32,0x33,0x44,
	0x34,0x35,0x36,0x45,
	0x37,0x38,0x39,0x46,
	0x41,0x40,0x42,0x43
};

void PrintOpCodeDescription(u16 opCode);

void DrawSprite(u8 x, u8 y, u8 height);
void PresentBuffer();
void UpdateInput(u8* keys, u32 keyCount);

HANDLE outputHandle = GetStdHandle(STD_OUTPUT_HANDLE);

u8 RAM[RAM_SIZE] = {0};
u8 V[REGISTER_SIZE] = {0};
// gfx
u8 GFX_BUFFER[SCREEN_WIDTH * SCREEN_HEIGHT] = {0};
// address register
u16 I = 0;
// input
u8 inputBuffer[KEY_COUNT];
bool draw = false;
int main()
{
	//STARTUPINFO startUpInfo;
	//memset(&startUpInfo, 0, sizeof(STARTUPINFO));
	//startUpInfo.cb = sizeof(startUpInfo);
	//PROCESS_INFORMATION processInfo;
	//CreateProcess(NULL, NULL, NULL, NULL, true, GetPriorityClass(outputHandle), NULL, NULL, &startUpInfo, &processInfo);
	Program program;
	srand(0);
	//outputHandle = FindWindow(NULL, L"C:\\Windows\\system32\\cmd.exe");
	LoadFont(program.mRam, fontSet_01, fontSetSize);
	InitInput(program.mIBuffer, KEY_COUNT);
	InitGraphics(program.mGBuffer, SCREEN_WIDTH, SCREEN_HEIGHT);
	LoadProgram(PROGRAM_NAME, &program.mRam[program.mPC]);
	RunProgram(program);
	return 0;
}

void DrawSprite(Program& program, u8 x, u8 y, u8 width, u8 height)
{
	PRINTF("Draw Sprite\n");

	bool cleared = false;
	program.mRegister[15] = 0;
	for(u8 h = 0; h < height;)
	{
		u8 sprite = program.mRam[program.mI+h];
		//sprite >>= 4;
		for(u8 i = 0; i < 8; ++i)
		{
			bool isSet = sprite & (1 << 7-i);
			if(isSet)
			{
				u8 pixelDrawn = program.mGBuffer[x + i + (y + h) * width];// GFX_BUFFER[x + i][y + h];
				if(pixelDrawn == 1)
				{
					program.mRegister[15] = 1; // VF is set to 1 if any screen pixels are flipped from set to unset when the sprite is drawn, and to 0 if that doesn’t happen.
				}
				program.mGBuffer[x + i + (y + h)* width] ^= 1; //GFX_BUFFER[x+i][y+h] ^= 1;
			}
		}
		++h;
	}

	draw = true;
}

void StartDraw() {}

void PresentBuffer()
{
#if DEBUG_PRINT_ON
	// draw border
	// top
	for(int i = 0; i < SCREEN_WIDTH+2; ++i)
	{
		printf("+");
	}
	printf("\n");
	for(int y = 0; y < SCREEN_HEIGHT; ++y)
	{
		printf("+");
		for(int x = 0; x < SCREEN_WIDTH; ++x)
		{
			printf("%c", GFX_BUFFER[x][y] == 0 ? CLEAR_PIXEL : SET_PIXEL);
		}
		printf("+\n");
	}
	// bottom
	for(int i = 0; i < SCREEN_WIDTH+2; ++i)
	{
		printf("+");
	}
	printf("\n");
#else
	//system("CLS");
	// draw border
	// top
	for(int i = 0; i < SCREEN_WIDTH+2; ++i)
	{
		printf("+");
	}
	printf("\n");
	for(int y = 0; y < SCREEN_HEIGHT; ++y)
	{
		printf("+");
		for(int x = 0; x < SCREEN_WIDTH; ++x)
		{
			printf("%c", GFX_BUFFER[x][y] == 0 ? CLEAR_PIXEL : SET_PIXEL);
		}
		printf("+\n");
	}
	// bottom
	for(int i = 0; i < SCREEN_WIDTH+2; ++i)
	{
		printf("+");
	}
	//printf("\n");
	COORD coord = {0};
	SetConsoleCursorPosition(outputHandle, coord);
#endif
	draw = false;
}

void UpdateInput(u8* iBuffer, u32 keyCount)
{
	for(int i = 0; i < keyCount; ++i)
	{
		bool pressed = GetAsyncKeyState(KEY_MAPPING[i]) & 0x8000;
		if(pressed)
		{
			//printf("\rPressed:%u\n", i);
		}
		iBuffer[i] = pressed ? 100 : 0;
	}
}

void PrintOpCodeDescription(u16 opCode)
{
	u8 instruction = (opCode & 0xF000) >> 12;
	printf("Instruction=%01X\n", instruction);
	switch(instruction)
	{
	case 2:
		{
			// Calls subroutine at NNN
			printf("Calls subroutine at 0x%03X\n", I); 
		}
		break;
	case 6:
		{
			printf("{6XNN	Sets VX to NN.}\n");
		}
		break;
	case 0xA:
		{
			printf("{ANNN	Sets I to the address NNN.}\n");
		}
		break;
	case 0xD:
		{
			printf("{DXYN	Sprites stored in memory at location in index register (I), maximum 8bits wide. Wraps around the screen. If when drawn, clears a pixel, register VF is set to 1 otherwise it is zero. All drawing is XOR drawing (e.g. it toggles the screen pixels)}\n");
		}
		break;
	case 0xF:
		switch(opCode & 0x00FF)
		{
		case 0x33:
			{
				printf("{Stores the Binary-coded decimal representation of VX, with the most significant of three digits at the address in I, the middle digit at I plus 1, and the least significant digit at I plus 2. (In other words, take the decimal representation of VX, place the hundreds digit in memory at location in I, the tens digit at location I+1, and the ones digit at location I+2.)}\n");
			}
			break;
		case 0x65:
			{
			}
			break;
		}
		break;
	}
}

void LoadFont(u8* ram, u8* font, u32 fontSize)
{
	// load font in memory
	printf("Loading font:\n");
	for (int i = 0, address = 0; i < 80; ++i)
	{
		ram[i] = font[i];
	}

	DebugPrintFont(font, fontSize);
}

void InitGraphics(u8* gBuffer, u32 width, u32 height)
{
	// init graphics
	for (int x = 0; x < width; ++x)
	{
		for (int y = 0; y < height; ++y)
		{
			gBuffer[x + y * width] = 0;
		}
	}
}

void InitInput(u8* inputBuffer, u32 keyCount)
{
	memset(inputBuffer, 0, sizeof(u8) * keyCount);
}

s8 LoadProgram(const c8* programName, u8* ram)
{
	// load a game in memory
	PRINTF("Loading:%s\n", PROGRAM_NAME);
	FILE* fp = fopen(PROGRAM_NAME, "rb");
	if (!fp) return -1;

	u16 fileSize = 0;
	fseek(fp, 0, SEEK_END);
	fileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	u16 readSize = fread(ram, sizeof(u8), fileSize, fp);

	PRINTF("ReadSize: %u bytes\n\n", readSize);

	fclose(fp);
	return 0;
}

void RunProgram(Program& program)
{
	u16 cycle = 1;
	u8* RAM = program.mRam;
	u16& pc = program.mPC;
	u16* stack = program.mStack;
	u8& sp = program.mSP;
	u8& delayTimer = program.mDelayTimer;
	u8& soundTimer = program.mSoundTimer;
	u8& inputTimer = program.mInputTimer;
	u8* keys = program.mIBuffer;

	for (;;)	// faster than while() loop - does not have an if check
	{
		//if(cycle-- <= 0)
		{
			u8 l = RAM[pc];
			u8 h = RAM[pc + 1];
			
			PRINTF_UPDATE("RAM[%u]=0x%02X, RAM[%u]=0x%02X\n", pc, l, pc + 1, h);

			u16 opCode = RAM[pc] << 8 | RAM[pc + 1];
#if PRINT_OPCODE_DESC
			PrintOpCodeDescription(opCode);
#endif
			PRINTF_UPDATE("OpCode=%Xh\n", opCode);
			u8 instruction = (opCode & 0xF000) >> 12;
			PRINTF_UPDATE("Instruction=%01X\n", instruction);
			switch (instruction)
			{
			case 0x0:
				switch (opCode & 0x000F)
				{
				case 0xE:
				{
					pc = stack[--sp];
					pc += 2;
				}
				break;
				default:
					//printf("NOT IMPLEMENTED");
					pc += 2;
				}
				break;
			case 0x1:
			{
				u16 address = (opCode & 0x0FFF);
				pc = address;
			}
			break;
			case 2:
			{
				// Calls subroutine at NNN
				PRINTF_UPDATE("Calls subroutine at 0x%03X\n", I);
				stack[sp] = pc;
				sp++;
				pc = opCode & 0x0FFF;
			}
			break;
			case 3:
			{
				u8 VX = (opCode & 0x0F00) >> 8;
				u16 val = (opCode & 0x00FF);
				pc += (V[VX] == val) ? 4 : 2;
			}
			break;
			case 4:
			{
				u8 VX = (opCode & 0x0F00) >> 8;
				u16 val = (opCode & 0x00FF);
				pc += (V[VX] != val) ? 4 : 2;
			}
			break;
			case 0x5:
			{
				u8 VX = (opCode & 0x0F00) >> 8;
				u8 VY = (opCode & 0x00F0) >> 4;
				pc += (V[VX] == V[VY]) ? 4 : 2;
			}
			break;
			case 6:
			{
				u8 VX = (opCode & 0x0F00) >> 8;
				u8 val = (opCode & 0x00FF);
				V[VX] = val;
				PRINTF_UPDATE("V[%u]=%02Xh(%u)\n", VX, val, val);
				pc += 2;
			}
			break;
			case 7:
			{
				u8 VX = (opCode & 0x0F00) >> 8;
				u16 val = (opCode & 0x00FF);
				V[VX] += val;
				pc += 2;
			}
			break;
			case 8:
			{
				switch (opCode & 0x000F)
				{
				case 0x0:
				{
					u8 VX = (opCode & 0x0F00) >> 8;
					u8 VY = (opCode & 0x00F0) >> 4;
					V[VX] = V[VY];
					pc += 2;
				}
				break;
				case 0x1:
				{
					u8 VX = (opCode & 0x0F00) >> 8;
					u8 VY = (opCode & 0x00F0) >> 4;
					V[VX] = V[VX] | V[VY];
					pc += 2;
				}
				break;
				case 0x2:
				{
					u8 VX = (opCode & 0x0F00) >> 8;
					u8 VY = (opCode & 0x00F0) >> 4;
					V[VX] = V[VX] & V[VY];
					pc += 2;
				}
				break;
				case 0x3:
				{
					u8 VX = (opCode & 0x0F00) >> 8;
					u8 VY = (opCode & 0x00F0) >> 4;
					V[VX] = V[VX] ^ V[VY];
					pc += 2;
				}
				break;
				case 0x4:
				{
					u8 VX = (opCode & 0x0F00) >> 8;
					u8 VY = (opCode & 0x00F0) >> 4;
					u16 valX = V[VX];
					u16 valY = V[VY];

					u16 val = valX + valY;

					if (val > 0xFF)
					{
						V[0xF] = 1;
					}
					else
					{
						V[0xF] = 0;
					}

					V[VX] = val;

					pc += 2;
				}
				break;
				case 0x5:
				{
					u8 VX = (opCode & 0x0F00) >> 8;
					u8 VY = (opCode & 0x00F0) >> 4;
					u16 valX = V[VX];
					u16 valY = V[VY];

					u16 val = valX - valY;

					if (valY > valX)
					{
						V[0xF] = 0;
					}
					else
					{
						V[0xF] = 1;
					}

					V[VX] = val;

					pc += 2;
				}
				break;
				default:
					//printf("NOT IMPLEMENTED");
					pc += 2;
				}
			}
			break;
			case 0xA:
			{
				I = (opCode & 0x0FFF);
				PRINTF_UPDATE("I=0x%03X\n", I);
				pc += 2;
			}
			break;
			case 0xC:
			{
				u8 VX = (opCode & 0x0F00) >> 8;
				u16 val = (opCode & 0x00FF);
				V[VX] = /*(rand() % 0xFF)*/0 & val;
				pc += 2;
			}
			break;
			case 0xD:
			{
				u8 VX = (opCode & 0x0F00) >> 8;
				u8 VY = (opCode & 0x00F0) >> 4;
				u8 height = (opCode & 0x000F);
				//V[VX] = 
				DrawSprite(V[VX], V[VY], height);
				pc += 2;
			}
			break;
			case 0xE:
			{
				switch (opCode & 0x00FF)
				{
				case 0xA1:
				{
					// keys
					u8 VX = (opCode & 0x0F00) >> 8;
					if (keys[V[VX]] == 0)
					{
						pc += 4;
					}
					else
					{
						pc += 2;
					}
					//pc += (key[V[VX]] == 0) ? 4 : 2;
				}
				break;
				default:
					//printf("NOT IMPLEMENTED");
					pc += 2;
				}
			}
			break;
			case 0xF:
				switch (opCode & 0x00FF)
				{
				case 0x07:
				{
					u8 VX = (opCode & 0x0F00) >> 8;
					V[VX] = delayTimer;
					pc += 2;
				}
				break;
				/*case 0x0A:
				{
				u8 VX = (opCode & 0x0F00) >> 8;
				V[VX] =
				}
				break;*/
				case 0x15:
				{
					u8 VX = (opCode & 0x0F00) >> 8;
					delayTimer = V[VX];
					pc += 2;
				}
				break;
				case 0x18:
				{
					u8 VX = (opCode & 0x0F00) >> 8;
					soundTimer = V[VX];
					pc += 2;
				}
				break;
				case 0x1E:
				{
					u8 VX = (opCode & 0x0F00) >> 8;
					I += V[VX];
					pc += 2;
				}
				break;
				case 0x29:
				{
					u8 VX = (opCode & 0x0F00) >> 8;
					I = V[VX] * 5;
					pc += 2;
				}
				break;
				case 0x33:
				{
					u8 VX = (opCode & 0x0F00) >> 8;
					u16 val = V[VX];
					RAM[I] = val / 100;
					RAM[I + 1] = (val % 100) / 10;
					RAM[I + 2] = (val % 100) % 10;
					pc += 2;
				}
				break;
				case 0x65:
				{
					u8 VX = (opCode & 0x0F00) >> 8;
					for (int i = 0; i < VX; ++i)
					{
						V[i] = RAM[I + i];
					}
					pc += 2;
				}
				break;
				default:
					//printf("NOT IMPLEMENTED");
					pc += 2;
				}
				break;
			default:
				//printf("NOT IMPLEMENTED");
				pc += 2;
			}

			if (delayTimer > 0) delayTimer--;
			if (soundTimer > 0)
			{
				if (soundTimer == 1) printf("\a");

				soundTimer--;
			}
			cycle = 1;
		}
		if (draw)
		{
			PresentBuffer();
		}
		//if(inputTimer-- <= 0)
		{
			UpdateInput(program.mIBuffer, KEY_COUNT);
			inputTimer = INPUT_UPDATE;
		}
		//Sleep(1);
		PRINTF_UPDATE("\n");
	}
}

// debug
void DebugPrintFont(u8* font, u32 fontSize)
{
	for (int k = 0; k < 16; ++k)
	{
		for (int i = 0; i < 5; ++i)
		{
			u8 val = font[k * 5 + i];
			val >>= 4;
			printf("DEC:%03u | HEX:0x%X | ", val, val);
			for (int j = 0; j < 4; ++j)
			{
				bool set = val & (1 << 3 - j);
				printf("%c", set ? '@' : ' ');
			}
			printf("\n");
		}
		printf("\n");
	}
}
