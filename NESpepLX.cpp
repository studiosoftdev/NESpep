#include <GL/glut.h>
#include <unistd.h>
#include <stdlib.h>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <time.h>
#include <map>

constexpr auto HASHES = "##############################################################################################################\n";

typedef unsigned short u16;
typedef unsigned char u8;
typedef __int16_t i16;
typedef signed char i8;

using namespace std;

char loadGame(const char* filename);
void emulateCPUcycle();
u16 getAddrFromAddrMode(int addrmode);
void dumpStateOutput();

u8 header[16];
u8 mapper;
u8 CMEM[0x10000];
u8 GMEM[0x4000];
u8 pcInc = 1;
u8 addrname = 0; //for debug
u8 SP = 0xFF; //SP is a byte wide value but the actual Stack in memory is 0x100 to 0x1FF - SP is an offset to 0x100. First value at 0x1FF.
string addrnames[13] = { "a    ", "abs  ", "abs,x", "abs,y", "imm  ", "imp  ", "xind ", "indy ", "rel  ", "zpg  ", "zpg,x", "zpg,y", "ind  " };
int REG[8] = { 0,0,0,0,0,1,0,0 }; /// C Z I D B - V N in order. Note bit 5 is ignored and is always 1.
u8 A = 0, X = 0, Y = 0, P = 0x34;
u16 PC = 0x0000;
bool errorstate = false;

int opcodes[0x100] =
{ //00  01  02  03  04  05  06  07  08  09  0A  0B  0C  0D  0E  0F
    11, 35, 0 , 0 , 0 , 35, 3 , 0 , 37, 35, 3 , 0 , 0 , 35, 3 , 0 ,//00
    10, 35, 0 , 0 , 0 , 35, 3 , 0 , 14, 35, 0 , 0 , 0 , 35, 3 , 0 ,//10
    29, 2 , 0 , 0 , 7 , 2 , 40, 0 , 39, 2 , 40, 0 , 7 , 2 , 40, 0 ,//20
    8 , 2 , 0 , 0 , 0 , 2 , 40, 0 , 45, 2 , 0 , 0 , 0 , 2 , 40, 0 ,//30
    42, 24, 0 , 0 , 0 , 25, 33, 0 , 36, 24, 33, 0 , 28, 24, 33, 0 ,//40
    12, 24, 0 , 0 , 0 , 25, 33, 0 , 16, 24, 0 , 0 , 0 , 24, 33, 0 ,//50
    43, 1 , 0 , 0 , 0 , 1 , 41, 0 , 38, 1 , 41, 0 , 28, 1 , 41, 0 ,//60
    13, 1 , 0 , 0 , 0 , 1 , 41, 0 , 47, 1 , 0 , 0 , 0 , 1 , 41, 0 ,//70
    0 , 48, 0 , 0 , 50, 48, 49, 0 , 23, 0 , 54, 0 , 50, 48, 49, 0 ,//80
    4 , 48, 0 , 0 , 50, 48, 49, 0 , 56, 48, 55, 0 , 0 , 48, 0 , 0 ,//90
    32, 30, 31, 0 , 32, 30, 31, 0 , 52, 30, 51, 0 , 32, 30, 31, 0 ,//A0
    5 , 30, 0 , 0 , 32, 30, 31, 0 , 17, 30, 53, 0 , 32, 30, 31, 0 ,//B0
    20, 18, 0 , 0 , 20, 18, 21, 0 , 27, 18, 22, 0 , 20, 18, 21, 0 ,//C0
    9 , 18, 0 , 0 , 0 , 18, 21, 0 , 15, 18, 0 , 0 , 0 , 18, 21, 0 ,//D0
    19, 44, 0 , 0 , 19, 44, 25, 0 , 26, 44, 34, 0 , 19, 44, 25, 0 ,//E0
    6 , 44, 0 , 0 , 0 , 44, 25, 0 , 46, 44, 0 , 0 , 0 , 44, 25, 0 ,//F0
};

int addrmode[0x100] = //make a function for the addressing modes to return the address used in the instruction depending on the address mode used
{ //00  01  02  03  04  05  06  07  08  09  0A  0B  0C  0D  0E  0F
    6 , 7 , 0 , 0 , 0 , 10, 10, 0 , 6 , 5 , 1 , 0 , 0 , 2 , 2 , 0 ,//00
    9 , 8 , 0 , 0 , 0 , 11, 11, 0 , 6 , 4 , 0 , 0 , 0 , 3 , 3 , 0 ,//10
    2 , 7 , 0 , 0 , 10, 10, 10, 0 , 6 , 5 , 1 , 0 , 2 , 2 , 2 , 0 ,//20
    9 , 8 , 0 , 0 , 0 , 11, 11, 0 , 6 , 4 , 0 , 0 , 0 , 3 , 3 , 0 ,//30
    6 , 7 , 0 , 0 , 0 , 10, 10, 0 , 6 , 5 , 1 , 0 , 2 , 2 , 2 , 0 ,//40
    9 , 8 , 0 , 0 , 0 , 11, 11, 0 , 6 , 4 , 0 , 0 , 0 , 3 , 3 , 0 ,//50
    6 , 7 , 0 , 0 , 0 , 10, 10, 0 , 6 , 5 , 1 , 0 , 13, 2 , 2 , 0 ,//60 - this row has a "13": indirect (at 6C)
    9 , 8 , 0 , 0 , 0 , 11, 11, 0 , 6 , 4 , 0 , 0 , 0 , 3 , 3 , 0 ,//70
    0 , 7 , 0 , 0 , 10, 10, 10, 0 , 6 , 0 , 6 , 0 , 2 , 2 , 2 , 0 ,//80
    9 , 8 , 0 , 0 , 11, 11, 12, 0 , 6 , 4 , 6 , 0 , 0 , 3 , 0 , 0 ,//90
    5 , 7 , 5 , 0 , 10, 10, 10, 0 , 6 , 5 , 6 , 0 , 2 , 2 , 2 , 0 ,//A0
    9 , 8 , 0 , 0 , 11, 11, 12, 0 , 6 , 4 , 6 , 0 , 3 , 3 , 3 , 0 ,//B0
    5 , 7 , 0 , 0 , 10, 10, 10, 0 , 6 , 5 , 6 , 0 , 2 , 2 , 2 , 0 ,//C0
    9 , 8 , 0 , 0 , 0 , 11, 11, 0 , 6 , 4 , 0 , 0 , 0 , 3 , 3 , 0 ,//D0
    5 , 7 , 0 , 0 , 10, 10, 10, 0 , 6 , 5 , 6 , 0 , 2 , 2 , 2 , 0 ,//E0
    9 , 8 , 0 , 0 , 0 , 11, 11, 0 , 6 , 4 , 0 , 0 , 0 , 3 , 3 , 0 ,//F0
};

/* Program entry point */

int main(int argc, char* argv[])
{
    //cout << "oreo" << endl;
    srand(time(0));
    if (argc < 2) { //has to load with game
        cout << "No file loaded." << endl;
        return 1;
    }
    if (loadGame(argv[1]) == 'f') {
        return 1;
    }
    cout << (int)CMEM[0x8001] << endl;
    cout << (int)GMEM[0x0001] << endl;
    signed char t = -((~(0xFB)) + 1);
    cout << int(0xFB) << " : " << int(t) << endl;
    cout << HASHES;
    while (!errorstate) {
        emulateCPUcycle();
    }
    //Sleep(30000);
}

void dumpStateOutput() {
    int inst = 0;
    cout << hex << addrnames[addrname];
    cout << setfill('0') << " | PC: " << setw(4) << int(PC) << " | Instruction: ";
    for (int i = 0; i < pcInc; i++) {
        inst += (CMEM[PC + i] << ((pcInc - i - 1) * 8));
    }
    cout << setfill('0') << setw(8) << int(inst);
    cout << setfill('0') << " | SP: " << setw(3) << int(SP);
    cout << setfill('0') << " | A: " << setw(2) << int(A) << " | X: " << setw(2) << int(X) << " | Y: " << setw(2) << int(Y) << " | CZIDBVN: " << REG[0] << REG[1] << REG[2] << REG[3] << REG[4] << REG[5] << REG[6] << REG[7] << endl;
}

u16 getAddrFromAddrMode(int addrmode) {
    signed char t = 0xFB; //used to help convert between signed and unsigned integers for relative addressing (9)
    switch (addrmode) {
    case 1: ///A - operand is from Accumulator.
        pcInc = 1; addrname = 0;
        return A;
    case 2: ///abs - operand is the value at given (next 2 bytes, an address is the value)
        pcInc = 3; addrname = 1;
        return (CMEM[PC + 2] << 8) | CMEM[PC + 1];
    case 3: ///abs,x - operand is the address given + X
        pcInc = 3; addrname = 2;
        return (CMEM[PC + 2] | CMEM[PC + 1]) + X;
    case 4: ///abs,y - operand is the address given + Y
        pcInc = 3; addrname = 3;
        return (CMEM[PC + 2] | CMEM[PC + 1]) + Y;
    case 5: ///imm - immediate, value is the operand
        pcInc = 2; addrname = 4;
        return CMEM[PC + 1];
    case 6: ///imp - implied, doesn't need an address/it is not given by an operand.
        pcInc = 1; addrname = 5;
        return 0xFFFF;
    case 7: ///ind,x - indexed indirect: value located at address given by operand+x on zpg
        pcInc = 2; addrname = 6;
        return CMEM[(CMEM[PC + 1] + X) % 0x100];
    case 8: ///ind,y - indirect indexed: value located at zpg[operand] + Y. zpg contains only LSByte
        pcInc = 2; addrname = 7;
        return CMEM[CMEM[PC + 1] + Y];
    case 9: ///rel - relative, operand is added to PC (operand is signed int)
        pcInc = 2; addrname = 8;
        return PC + (signed char)CMEM[PC+1];
    case 10: ///zpg - zero page, value is located at CMEM[operand] where operand is 1 byte
        pcInc = 2; addrname = 8;
        return PC+1;
    case 11: ///zpg,x - zero page x indexed, value is located at CMEM[operand+X]
        pcInc = 2; addrname = 10;
        return CMEM[(CMEM[PC + 1] + X) % 0x100];
    case 12: ///zpg,y - zero page y indexed, value is located at CMEM[operand+Y]
        pcInc = 2; addrname = 11;
        return CMEM[(CMEM[PC + 1] + Y) % 0x100];
    case 13: ///ind - indirect: used only for JMP, returns an address given by the 2Byte address in operand
        pcInc = 3; addrname = 12;
        return (CMEM[((CMEM[PC + 2] << 8) | CMEM[PC + 1]) + 1] << 8) | CMEM[(CMEM[PC + 2] << 8) | CMEM[PC + 1]];
    default: cout << "Unknown address mode: " << addrmode << endl;
        errorstate = true;
    }
}

void emulateCPUcycle() {
    unsigned int instruction; //max 4 bytes per instruction
    u8 opcode = CMEM[PC];
    u16 operand = getAddrFromAddrMode(addrmode[opcode]); //need to account for how many bytes to skip with PC somehow

    switch (opcodes[opcode]) {
    case 1: cout << "ADC (preop)  | "; dumpStateOutput(); ///ADC. Add operand to A, set carry flag if necessary
        A += operand;
        if (A + operand > 0xFF) { REG[0] = 1; A = (A + operand) % 0x100; } //set carry flag
        else { A += operand; REG[0] = 0; } //else clear flag
        if (A > 0x7F) { REG[6] = 1; } //set overflow if addition has resulted in a 2's comp overflow to neg
        else { REG[6] = 0; } //else clear flag
        if (A == 0) { REG[1] = 1; } //set zero flag if necessary
        else { REG[1] = 0; } //else clear flag
        REG[7] = (A & 0x80) >> 7; //set N to MSB as that reflects sign
        PC += pcInc;
        cout << "ADC (postop) | ";
        break;
    case 2: 
        {
            cout << "AND (preop)  | "; dumpStateOutput(); //AND. ANDs operand with accumulator
            u8 res = (operand & 0x00FF) & A;
            REG[7] = res > 127 ? 1 : 0; //should definitely check if u8 is above 127, that is negative signed int
            REG[6] = res == 0 ? 1 : 0;
            A = res;
            cout << "AND (postop)  | ";
        }
        break;
    case 3: cout << "ASL (preop)  | "; dumpStateOutput(); //ASL. Shifts operand left by 1 bit. Affects C,Z,N.
        REG[0] = (operand & 0x80) >> 7;
        if (A == 0) { REG[1] = 1; }
        else { REG[1] = 0; }
        REG[7] = (A & 0x80) >> 7;
        PC += pcInc;
        cout << "ASL (postop) | ";
        break;
    case 4: cout << "BCC (preop)  | "; dumpStateOutput(); //BCC. Branch if C = 0. Relative address.
        PC = REG[0] == 0 ? operand : PC + pcInc;
        cout << "BCC (postop)  | ";
        break;
    case 5: cout << "BCS (preop)  | "; dumpStateOutput(); //BCS. Branch if C = 1. Relative address.
        PC = REG[0] == 1 ? operand : PC + pcInc;
        cout << "BCS (postop)  | ";
        break;
    case 6: cout << "BEQ (preop)  | "; dumpStateOutput(); //BEQ. Branch if Z = 1. Relative address.
        PC = REG[1] == 1 ? operand : PC + pcInc;
        cout << "BEQ (postop)  | ";
        break;
    case 7: cout << "BIT (preop)  | "; dumpStateOutput(); //BIT. BIt Test with accumulator. bits 7, 6 of operand moved to N,V. Z set to operand AND accumulator.
        REG[6] = (CMEM[operand] & 0x40) >> 6;
        REG[7] = (CMEM[operand] & 0x80) >> 7;
        REG[1] = (CMEM[operand] & A) == 0 ? 1 : 0;
        cout << "BIT (postop)  | "; dumpStateOutput();
        break;
    case 8: cout << "BMI (preop)  | "; dumpStateOutput(); //BMI. Branch on MInus. Branch if N = 1.
         PC = (REG[7] == 1) ? operand : PC + pcInc;
        cout << "BMI (postop) | ";
        break;
    case 9: cout << "BNE (preop)  | "; dumpStateOutput(); ///BNE. Branch if Not Equal (Z = 0) to relative address.
        PC = (REG[1] == 0) ? operand : PC + pcInc; // is (~operand + 1) correct ?
        cout << "BNE (postop) | ";
        break;
    case 10: cout << "BPL (preop)  | "; dumpStateOutput(); ///BPL. Branch if N = 0 to relative address.
        PC = (REG[7] == 0) ? operand : PC + pcInc;
        cout << "BPL (postop) | ";
        break;
    case 12: cout << "BVC (preop)  | "; dumpStateOutput(); ///BPL. Branch if V = 0 to relative address.
        PC = (REG[6] == 0) ? operand : PC + pcInc;
        cout << "BVC (postop) | ";
        break;
    case 15: cout << "CLD (preop)  | "; dumpStateOutput(); ///CLD. Clear Decimal. Sets D = 0 (REG[3])
        REG[3] = 0;
        PC += pcInc;
        cout << "CLD (postop) | ";
        break;
    case 23: cout << "DEY (preop)  | "; dumpStateOutput(); ///DEY. Y--; Affects Z,N.
        Y--;
        if (Y == 0) { REG[1] = 1; }
        else { REG[1] = 0; }
        REG[7] == (Y & 0x80) >> 7;
        PC += pcInc;
        cout << "DEY (postop) | "; 
        break;
    case 29: cout << "JSR (preop)  | "; dumpStateOutput(); ///JSR. pushes the address -1 of the current addr on to the stack and then sets the program counter to operand.
        CMEM[SP+0x100] = PC - 1;
        SP--;
        PC = operand;
        cout << "JSR (postop) | ";
        break;
    case 30: cout << "LDA (preop)  | "; dumpStateOutput(); ///LDA. Load into A.
        A = operand;
        REG[1] = (A == 0) ? 1 : 0;
        REG[7] = (A & 0x80) >> 7;
        PC += pcInc;
        cout << "LDA (postop) | ";
        break;
    case 31: cout << "LDX (preop)  | "; dumpStateOutput(); ///LDX. Load from mem into X. Modifies N, Z.
        X = operand;
        if (X == 0) { REG[1] = 0; }
        REG[7] = (X & 0x80) >> 7;
        PC += pcInc;
        cout << "LDX (postop) | ";
        break;
    case 32: cout << "LDY (preop)  | "; dumpStateOutput(); //LDY. Load CMEM[operand] -> Y.
        Y = operand;
        if (Y == 0) { REG[1] = 0; }
        REG[7] = (Y & 0x80) >> 7;
        PC += pcInc;
        cout << "LDY (postop) | ";
        break;
    case 47: cout << "SEI (preop)  | "; dumpStateOutput(); ///SEI. Set interrupt disable, sets I = 1 (REG[2])
        REG[2] = 1;
        PC++;
        cout << "SEI (postop) | ";
        break;
    case 48: cout << "STA (preop)  | "; dumpStateOutput(); ///STA. Stores A -> M[operand].
        CMEM[operand] = A;
        PC += pcInc;
        cout << "STA (postop) | ";
        break;
    case 50: cout << "STY (preop)  | "; dumpStateOutput(); ///STY. Stores Y -> M[operand].
        CMEM[operand] = Y;
        PC += pcInc;
        cout << "STY (postop) | "; 
        break;
    case 55: cout << "TXS (preop)  | "; dumpStateOutput(); ///TXS. Transfer X to SP.
        SP = X;
        PC += pcInc;
        cout << "TXS (postop) | ";
        break;
    default: cout << hex << "Unrecognized opcode. Bytes: " << PC << " - " << int(CMEM[PC]) << int(CMEM[PC + 1]) << int(CMEM[PC + 2]) << int(CMEM[PC + 3]) << endl;
        errorstate = true;
    }
    dumpStateOutput();
    cout << HASHES;
}

char loadGame(const char* filename) {
    FILE* pFile = fopen(filename, "rb");

    fseek(pFile, 0, SEEK_END);
    unsigned long lSize = ftell(pFile);
    rewind(pFile);
    printf("Filesize: %d\n", (int)lSize);
    // Allocate memory to contain the whole file
    char* buffer = (char*)malloc(sizeof(char) * lSize);
    if (buffer == NULL)
    {
        fputs("Memory error", stderr);
        //Sleep(30000);
        return 'f';
    }

    // Copy the file into the buffer
    size_t result = fread(buffer, 1, lSize, pFile);
    if (result != lSize)
    {
        fputs("Reading error", stderr);
        //Sleep(30000);
        return 'f';
    }
    //copy first 16 bytes of buffer into header, we will use this to identify space allocations
    for (int i = 0; i < 16; i++) {
        header[i] = buffer[i];
    }
    //Determine file format - iNES or NES2.0 respectively
    if (buffer[0] == 'N' && buffer[1] == 'E' && buffer[2] == 'S' && buffer[3] == 0x1A) {
        //cout << "ooga" << endl;

        //Determine what mapper we're using
        int prglen = header[4] * 16384;
        switch (header[6] >> 4) {
        case 0x0: mapper = '0'; cout << "Mapper = 0" << endl;
            //copy prgrom into mem starting at 0x8000. mapper 0 is just a straight copy paste
            if (header[5] == 2) { //if NROM 256
                for (int i = 0; i < prglen; i++) {
                    CMEM[i + 0x8000] = buffer[i + 0x10];
                }
            }
            else if (header[5] == 1) { //if NROM 128
                for (int i = 0; i < prglen; i++) {
                    CMEM[i + 0x8000] = buffer[i + 0x10]; //need to offset header. Note this is in case of no trainer
                    CMEM[i + 0xC000] = buffer[i + 0x10]; //mirror
                }
            }
            for (int i = 0; i < header[5] * 8192; i++) {
                GMEM[i] = buffer[i + prglen + 0x10];
            }
            PC = CMEM[0xFFFD] << 8 | CMEM[0xFFFC];
            return true;
        case 0x1: mapper = '1'; cout << "Mapper = 1" << endl; break;
        default: cout << "Unknown mapper type" << endl;
            sleep(30000); return 1; //if we dunno the mapper, it an error
        }
    }
    else if (buffer[0] == 'N' && buffer[1] == 'E' && buffer[2] == 'S' && buffer[3] == 0x1A && buffer[7] == 0x08) {
        cout << "booga" << endl;
        return true;
    }

    // Close file, free buffer
    fclose(pFile);
    free(buffer);

    //Sleep(30000);
    return false;
}

