#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include <stdlib.h>
#include <windows.h>
#include <iostream>
#include <fstream>
#include <time.h>

using namespace std;

char loadGame(const char *filename);

unsigned char header [16];
unsigned char mapper;
unsigned char CMEM [0xFFFF];
unsigned char GMEM [0x3FFF];
int REG [7] = {0,0,0,0,0,0,0}; // C Z I D B V N in order
unsigned char A = 0, X = 0, Y = 0;
unsigned short PC = 0x8000, SP = 0x100;

/* Program entry point */

int main(int argc, char *argv[])
{
    //cout << "oreo" << endl;
    srand(time(0));
    if(argc < 2){ //has to load with game
        return 1;
    }
    if(loadGame(argv[1]) == 'f'){
        return 1;
    }
    cout << (int)CMEM[0x8001] << endl;
    cout << (int)GMEM[0x0001] << endl;
    bool errorstate = false;
    while(!errorstate){
        unsigned int instruction; //max 4 bytes per instruction
        unsigned char opcode = CMEM[PC];
        //cout << hex << int(opcode) << endl;
        switch(opcode){
            case 0x20: //JSR - Jump to New location saving return address
                {instruction = CMEM[PC] << 16 | CMEM[PC+1] << 8 | CMEM[PC+2]; //plcaing the 3 bytes necessary into the instruction, opcode + operand
                SP++;
                CMEM[SP] = ((PC+2) & 0xFF00) >> 8; //need +2 to push the address of the instruction AFTER this JSR to the stack
                SP++;
                CMEM[SP] = (PC+2) & 0x00FF; //see above comment
                cout << hex << "PC: " << PC << " -> " << 0x8000+(instruction & 0xFFFF) << "\t | " << ((instruction & 0xFF0000) >> 16) << " " << (instruction & 0x00FFFF) << "\t\t | JSR (abs)" << endl;
                PC = 0x8000 + (instruction & 0xFFFF);
                break;}
            case 0x30: //BMI - branch if minus (N = 1)
                {instruction = CMEM[PC] << 8 | CMEM[PC+1] ;
                if(REG[7] == 1){
                    cout << hex << "PC: " << PC << " -> " << CMEM[PC]+(instruction&0x00FF) << "\t | " << ((instruction & 0xFF00) >> 8) << " " << (instruction & 0x00FF) << "\t\t | BMI (rel)" << endl;
                    PC += instruction & 0x00FF;
                }
                else{
                    cout << hex << "PC: " << PC << " -> " << PC + 2 << "\t | " << ((instruction & 0xFF00) >> 8) << " " << (instruction & 0x00FF) << "\t | N: " << REG[7] << "\t\t | BMI (rel)" << endl;
                    PC+=2;
                }
                break;}
            case 0x36: // ###UNTESTED### ROL - ROtate Left (set N if , set Z if result is 0, set C as the bit that is shifted out)
                {instruction = CMEM[PC] << 8 | CMEM[PC+1];
                long data = CMEM[(X + (instruction & 0x00FF)) & 0x0FF] * 2;  //consequence of zero paged index (in the index), multiply by 2 for shift
                data += REG[0]; //adding in the carry as the lowest bit
                REG[0] = data & 0x0F00 >> 16; //moving the shifted out bit to the carry
                cout << hex << "PC: " << PC << " -> " << PC + 2 << "\t | " << ((instruction & 0xFF00) >> 8) << " " << (instruction & 0x00FF) << "\t | temp: C: N: Z: " << "\t | ROL (zpg)" << endl;
                break;}
            case 0x60: //RTS - Return To Subroutine. Pulls 2 bytes off the stack to be the PC.
                {instruction = CMEM[PC];
                short oldPC = PC;
                PC = (CMEM[SP-1] << 8) | CMEM[SP];
                SP-=2;
                cout << hex << "PC: " << oldPC << " -> " << PC << "\t | " << instruction << "\t\t\t | RTS (abs)" << endl;
                break;}
            case 0xEA: //NOP - No OP. Do nothing for 2 cycles.
                {instruction = CMEM[PC];
                cout << hex << "PC: " << PC << " -> " << PC+1 << "\t | " << instruction << "\t\t\t | NOP (imp)" << endl;
                PC++;
                break;}

            default: cout << "Unknown opcode" << endl;
                     instruction = CMEM[PC] << 24 | CMEM[PC+1] << 16 | CMEM[PC+2] << 8 | CMEM[PC+3];
                     cout << hex << "Bytes: " << instruction <<endl;
                     errorstate = true; break;
        }
    }
    Sleep(30000);
}

char loadGame(const char *filename){
    FILE * pFile = fopen(filename, "rb");

    fseek(pFile , 0 , SEEK_END);
	unsigned long lSize = ftell(pFile);
	rewind(pFile);
	printf("Filesize: %d\n", (int)lSize);
    // Allocate memory to contain the whole file
	char * buffer = (char*)malloc(sizeof(char) * lSize);
	if (buffer == NULL)
	{
		fputs ("Memory error", stderr);
		Sleep(30000);
		return 'f';
	}

	// Copy the file into the buffer
	size_t result = fread (buffer, 1, lSize, pFile);
	if (result != lSize)
	{
		fputs("Reading error",stderr);
		Sleep(30000);
		return 'f';
	}
	//copy first 16 bytes of buffer into header, we will use this to identify space allocations
	for(int i = 0; i < 16; i++){
        header[i] = buffer[i];
	}
	//Determine file format - iNES or NES2.0 respectively
    if(buffer[0] == 'N' && buffer[1] == 'E' && buffer[2] == 'S' && buffer[3] == 0x1A){
        //cout << "ooga" << endl;

        //Determine what mapper we're using
        int prglen = header[4]*16384;
        switch(header[6] >> 4){
            case 0x0: mapper = '0'; cout << "Mapper = 0" << endl;
                //copy prgrom into mem starting at 0x8000. mapper 0 is just a straight copy paste
                for(int i = 0; i < prglen; i++){
                    CMEM[i + 0x8000] = buffer[i + 0x10];
                }
                for(int i = 0; i < header[5]*8192; i++){
                    GMEM[i] = buffer[i + prglen + 0x10];
                }
                return true;
            case 0x1: mapper = '1'; cout << "Mapper = 1" << endl; break;
            default: cout << "Unknown mapper type" << endl;
                     Sleep(30000); return 1; //if we dunno the mapper, it an error
        }
	}
	else if(buffer[0] == 'N' && buffer[1] == 'E' && buffer[2] == 'S' && buffer[3] == 0x1A && buffer[7] == 0x08){
        cout << "booga" << endl;
        return true;
	}

    // Close file, free buffer
	fclose(pFile);
	free(buffer);

    Sleep(30000);
	return false;
}
