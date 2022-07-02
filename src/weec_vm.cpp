
// c4.c plus VM
// Written by Robert Swierczek
// Mangled for small systems by rossum

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <string>
#include "weec.h"

int getchar_s();

namespace weec_vm {

// VM begins
uint8_t* mem_;
uint16_t pc,sp,bp,heap;

int8_t loadc(uint16_t a)
{
    return (int8_t)mem_[a];
}

void storec(uint16_t a, int8_t v)
{
    mem_[a] = v;
}

const char* loads(uint16_t a)
{
    return (char*)mem_ + a;
}

int16_t loadi(uint16_t a)
{
    return mem_[a] | (mem_[a+1] << 8);
}

//
void* loadp(uint16_t s)
{
    return (void*)loads(loadi(s));
}

void storei(uint16_t a, uint16_t v)
{
    mem_[a] = v;
    mem_[a+1] = v >> 8;
}

int16_t pop()
{
    int i = loadi(sp);
    sp += 2;
    return i;
}

void push(uint16_t v)
{
    sp -= 2;
    storei(sp,v);
}


#if 0
#include <vector>
#include <map>
using namespace std;
int lasti = 0;
map<int,int> indv;
map<int,int> pairs;

int small = 0;
int total = 0;
void stats(int i, int v)
{
    if (i & 0x80)
        i &= 0xF0;
    indv[i]++;
    pairs[i | (lasti << 8)]++;
    lasti = i;

    switch (i) {
        case LEA:
        case IMM:
            total++;
            if (v >= -8 && v <= 7)
                small++;
            break;
    }
}

void dump()
{
    printf("INDV =======\n");
    for (auto kv : indv) {
        pn(kv.first);printf("%d\n",kv.second);
    }
    printf("PAIRS =======\n");
    for (auto kv : pairs) {
        pn(kv.first >> 8); printf(" "); pn(kv.first & 0xFF);
        printf("%d\n",kv.second);
    }
    /*
    printf("TRIPS =======\n");
    for (auto kv : trips) {
        pn(kv.first >> 16); printf(" "); pn((kv.first >> 8)&0xFF); printf(" "); pn(kv.first & 0xFF);
        printf("%d\n",kv.second);
    }
     */
    printf("saved %d of %d\n",small,total);
}

void scan()
{
    int bytes = (int)(e-code_org);
    int p = 0;
    while (p < bytes) {
        uint8_t i = loadc(p++);
        int16_t v = 0;
        if (i <= ADJ) {
            v = loadi(p);
            p += 2;
        }
        stats(i,v);

        if (i == LEV) { // skip strings
            while (p < bytes) {
                if (loadc(p) == ENT)
                    break;
                p++;
            }
        }
    }
    dump();
}
#endif

// determinw which of the params are strings by scanning format
// fussier than it needs to be
int printf_(const char* s, int t)
{
    while (*s) {
        char c = *s++;
        if (c == '%') {
            if (*s == '%')
                s++;
            else {
                char buf[32];
                int i = 0;
                buf[i++] = c;
                while (*s) {
                    c = buf[i++] = *s++;
                    if (strchr("iduoxXcsp",c) || i == 31)
                        break;
                }
                buf[i] = 0;
                int n = loadi(t);
                if (c == 's')
                    printf(buf,loads(n));
                else
                    printf(buf,n);
                t -= 2;
                continue;
            }
        }
        putchar(c);
    }
    return 0;
}

int sys(int index)
{
    int i,t;
    switch (index + FIRST_SYS)
    {
        case PRTF:
            i = loadc(pc) == ADJ ? loadc(pc+1) : loadc(pc) & 0xF;   // i = number of params passed to call
            t = sp + (i-1)*2;                                       // peek at adjust to understand how many params
            return printf_((char*)loadp(t),t-2);                       // list of deep in the stack .. pain in the ass

        case OPEN: return open((char*)loadp(sp+2), loadi(sp));
        case READ: return (int)read(loadi(sp+4), (char*)loadp(sp+2), loadi(sp));
        case CLOS: return close(loadi(sp));

        case MALC: i = loadi(sp); t = heap; heap += i; return t;       // poor malloc

        case MSET: memset(loadp(sp+4), loadi(sp+2), loadi(sp)); return 0;
        case MCMP: return memcmp(loadp(sp+4), loadp(sp+2), loadi(sp));
        case GETC: return getchar_s();
        case RAND: return rand() & 0x7FFF; // keep it positive

        case EXIT: pc = 0; return 0;
        default:
            printf("bad syscall?\n"); return -1;
    }
}

// this is a 16 bit target
int run(uint8_t* _mem, int mem_size, int entry_pt, int code_size, int global_size)
{
    mem_ = _mem;
    pc = entry_pt;
    sp = mem_size;
    bp = 0;

    int16_t a = 0;  // accumulator
    uint16_t t;
    int cycle;      // vm registers

    printf("code_size %d, bss: %d\n\n\n",code_size,global_size);
    int g_offset = code_size;           // start of globals
    heap = g_offset + global_size;  // start of 'heap'
    //scan();

    int argc = 0;
    char* argv = 0;         // these would need to be pushed onto the stack...
    push(argc);             //
    push(0);                // (int)argv
    push(0);                // jump to zero ezits
    mem_[0] = EXIT;

    // run...
    int debug = 0;
    cycle = 0;
    while (1) {
        if (debug) {
            printf("a:%5d s:%5d bp:%5d ",(uint16_t)a,sp,bp);
            printf("%4d> %04X ",pc, cycle);
            disasm((uint8_t*)mem_ + pc);
        }
        if (!pc) {
            //printf("done at (%d) cycle = %d\n", sp, cycle);
            return 0;
        }

        uint8_t i = loadc(pc++);
        int16_t v = 0;
        if (i <= ADJ) {
            v = loadi(pc);
            pc += 2;
        } else if (i & 0x80) {
            v = ((int8_t)(i << 4)) >> 4;
            i &= 0xF0;
        }
        ++cycle;

        switch (i) {
            case LEA_:
            case LEA: a = bp + v*2;                         break;  // load local address
            case IMM_:
            case IMM: a = v;                                break;  // load address or immediate
            case GLO: a = v + g_offset;                     break;  // load global address - could patch to IMM
            case JMP: pc = v;                               break;  // jump
            case JSR: push(pc); pc = v;                     break;  // jump to subroutine
            case BZ:  pc = a ? pc : v;                      break;  // branch if zero
            case BNZ: pc = a ? v : pc;                      break;  // branch if not zero
            case ENT: push(bp); bp = sp; sp -= v*2;         break;  // enter subroutine
            case ADJ_:
            case ADJ: sp += v*2;                            break;  // stack adjust

            case LEA_PSH: a = bp + v*2; push(a);            break;
            case PSH_IMM: push(a); a = v;                   break;
            case INC:                                               // increment gets its own instruction
                t = v & 7;
                i = t > 2 ? 2:1;            // PTR
                a += (v < 0) ? -i : i;
                if (t)
                    storei(pop(),a);
                else
                    storec(pop(),a);
                break;

            case LEV: sp = bp; bp = pop(); pc = pop();      break;  // leave subroutine
            case LI:  a = loadi(a);         break;                  // load int
            case LC:  a = loadc(a);         break;                  // load char
            case SI:  storei(pop(),a);      break;                  // store int to addr on stack
            case SC:  storec(pop(),a);      break;                  // store char to addr on stack
            case PSH: push(a);              break;                  // push (always 16)

            case OR:  a = pop() |  a; break;  // binops
            case XOR: a = pop() ^  a; break;
            case AND: a = pop() &  a; break;
            case EQ:  a = pop() == a; break;
            case NE:  a = pop() != a; break;
            case LT:  a = pop() <  a; break;
            case GT:  a = pop() >  a; break;
            case LE:  a = pop() <= a; break;
            case GE:  a = pop() >= a; break;
            case SHL: a = pop() << a; break;
            case SHR: a = pop() >> a; break;
            case ADD: a = pop() +  a; break;
            case SUB: a = pop() -  a; break;
            case MUL: a = pop() *  a; break;
            case DIV: a = pop() /  a; break;
            case MOD: a = pop() %  a; break;

            case SYS: a = sys(v);     break;
            default:
                printf("unknown instruction = %d! cycle = %d\n", i, cycle);
                return -1;
        }
    }
}
}

int run(uint8_t* _mem, int mem_size, int entry_pt, int code_size, int global_size)
{
    return weec_vm::run(_mem,mem_size,entry_pt,code_size,global_size);
}
