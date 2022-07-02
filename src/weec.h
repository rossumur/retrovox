
// opcodes
enum {
    NOP, SYS, LEA ,IMM, GLO, JMP ,JSR ,BZ  ,BNZ ,ENT ,ADJ ,
    LEV ,LI  ,LC  ,SI  ,SC  ,PSH ,
    OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,    // same as tokens

    OPEN,READ,CLOS,PRTF,MALC,MSET,MCMP,GETC,RAND,EXIT,

    LEA_ =      0x80,   // single byte
    IMM_ =      0x90,
    LEA_PSH =   0xA0,
    PSH_IMM =   0xB0,
    INC  =      0xC0,
    ADJ_ =      0xD0,
};

#define FIRST_SYS OPEN

int disasm(const uint8_t* d);
int run(uint8_t* _mem, int mem_size, int entry_pt, int code_size, int global_size);
