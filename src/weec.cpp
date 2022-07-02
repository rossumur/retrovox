// c5.c - C in five functions

// c4.c plus
//   abstract syntax tree creation
//   back-end code generator
//   parameters passed in correct order
//   various optimizations
// c4.c - C in four functions

// char, int, and pointer types
// if, while, return, switch, and expression statements
// just enough features to allow self-compilation and a bit more

// Written by Robert Swierczek
//
// Mangled into weec for small systems by rossum

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include "weec.h"

// simple inline peeps worth ~20%
#define PEEP

#define SIZEOF_INT 2
typedef struct {
    uint8_t token;
    unsigned int cls : 4;
    unsigned int typ : 4;
    int16_t val : 16;
} Sym;

Sym *id;                            // currently parsed identifier
char *p, *lp;                       // current position in source code
uint8_t *e, *le;                    // current position in emitted code
uint16_t default_ptr,case_ptr;      // case statement patch-up pointer
int16_t bss = 0;                    // offset from start of bss for globals
uint8_t src,dbg;

int
tk,       // current token
ival,     // current token value
ty,       // current expression type
loc,      // local variable offset
line;     // current line number

//  everything is done in place
uint8_t* mem_pool;
uint8_t* mem_end;
char* sym_ptr;
char* min_sim_ptr;

// tokens and classes (operators last and in precedence order)
typedef enum {
    None = 128, Num, Fun, Sys, Glo, Loc, Id,
    Break, Case, Char, Default, Else, Enum, If, Int, Return, Sizeof, Switch, For, Do, While,
    Assign, Cond, Lor, Lan,
    Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Brak, GString
} Toks;

typedef enum {
    Nul_, Num_, Fun_, Sys_, Glo_, Loc_, Id_,
} Classes;

enum { CHAR, INT, PTR };

#define ID_TOKEN(_id)   (_id->token)
#define ID_CLASS(_id)   (_id->cls)
#define ID_TYPE(_id)    (_id->typ)
#define ID_VAL(_id)     (_id->val)
#define ID_NAME(_id)    ((char*)(id+1))

void initmem(uint8_t* pool, int len)
{
    mem_pool = pool;
    mem_end = pool + len;
    sym_ptr = min_sim_ptr = (char*)mem_end;
}

uint16_t byte_ref(uint8_t* i)
{
    return (uint16_t)(i - mem_pool);
}

uint16_t current_addr()
{
    return (uint16_t)(e+1 - mem_pool);
}

int16_t swap16(uint16_t a, int16_t v)
{
    uint8_t* c = mem_pool + a;
    int16_t i = c[0] | (c[1] << 8);
    c[0] = v;
    c[1] = v >> 8;
    return i;
}

uint16_t make_current(uint16_t a)
{
    return swap16(a,current_addr());
}

// create a linked list of rodata string refs, breaks, defaults etc
// _brk_link can be nested

uint16_t _rlink;
uint16_t _brk_link;
uint16_t push_break()
{
    uint16_t b = _brk_link;
    _brk_link = 0;
    return b;
}

void pop_break(uint16_t br)
{
    uint16_t addr = current_addr();     // now patch all the breaks
    while (_brk_link)
        _brk_link = swap16(_brk_link,addr);
    _brk_link = br;
}

int sym_end()
{
    return id >= (Sym*)mem_end;
}

void sym_next()
{
    char *s = (char*)(id+1);
    s += (strlen(s)+2) & ~1; // align
    id = (Sym*)s;
}

int find_sym(const char* s, const char* e)
{
    id = (Sym*)sym_ptr;
    while (!sym_end()) {
        char* d = (char*)(id + 1);
        if (d[e-s] == 0 && strncmp(s,d,e-s) == 0)
            return 1;
        sym_next();
    }
    return 0;
}

void add_sym(const char* s, const char* e)
{
    int len = (e-s + 2) & ~1;           // aligned size of string
    sym_ptr -= len + sizeof(Sym);       // allocate space, may collide with code!
    if (sym_ptr < min_sim_ptr)
        min_sim_ptr = sym_ptr;
    id = (Sym*)sym_ptr;                 // push
    char *dst = sym_ptr + sizeof(Sym);
    while (s < e)
        *dst++ = *s++;
    *dst = 0;                           // copy the name
    ID_TOKEN(id) = Id;
    ID_CLASS(id) = 0;
    ID_TYPE(id) = 0;
    ID_VAL(id) = 0;
}

void put_str(const char* s)
{
    while (*s) {
        if (*s == '\n')
            printf("\\n");
        else
            putchar(*s);
        s++;
    }
}

void dump_sym(const Sym* s)
{
    if (ID_TOKEN(s)== 1) {
        printf("STR: '");
        put_str((char*)(s+1));
        printf("'\n");
    } else {
        printf("T:%02X ",ID_TOKEN(s));
        printf("%8.4s", &"Nul Num Fun Sys Glo Loc Id  "[ID_CLASS(s)*4]);
        printf("%s ",(char*)(s+1));
        printf(" %d\n",ID_VAL(s));
    }
}

void dump_syms()
{
    id = (Sym*)sym_ptr;
    while (!sym_end()) {
        dump_sym(id);
        sym_next();
    }
}

void pn(int i)
{
    if (i & 0x80) {
        printf("%.7s",&"LEA_   ,IMM_   ,LEA_PSH,PSH_IMM,INC    ,ADJ_    "[((i>>4)&7)*8]);
        return;
    }
    printf("%.4s",
    &"NOP ,SYS, LEA ,IMM ,GLO ,JMP ,JSR ,BZ  ,BNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PSH ,"
    "OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,"
    "OPEN,READ,CLOS,PRTF,MALC,MSET,MCMP,EXIT,"[i * 5]);
}

int disasm(const uint8_t* d)
{
    int i = *d++;
    pn(i);
    if (i <= ADJ) {
        int16_t v = *d++;
        v |= (*d++) << 8;
        printf(" %d\n",v);
        return 3;
    }
    if (i & 0x80) {
        int8_t v = i << 4;
        printf(" %d",v >> 4);
        if ((i & 0xF0) == INC)
            printf((i & 0x08) ? " --" : " ++");
    }
    printf("\n");
    return 1;
}

void disasm_()
{
    printf(";%.*s", (int)(p - lp), lp); // wont work in get mode
    lp = p;
    while (le < e) {
        printf("%4d: ", (int)(le+1-mem_pool));
        le += disasm(le+1);
    }
}

// next token please
char get()
{
    return *p++;
}

char peek()
{
    return *p;
}

void next()
{
    char *pp;
    while ((tk = *p)) {
        ++p;

        // get an identifier, set id to point to it
        if ((tk >= 'a' && tk <= 'z') || (tk >= 'A' && tk <= 'Z') || tk == '_')
        {
            pp = p - 1;   // pointer to current token
            while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_')
                p++;                // token buffer!
            if (!find_sym(pp,p))    // sets id
                add_sym(pp,p);      // not found, add it to sym_pool
            tk = ID_TOKEN(id);
            return;
        }

        else if (tk >= '0' && tk <= '9') {          // decimal!
            if ((ival = tk - '0')) {
                while (*p >= '0' && *p <= '9')
                    ival = ival * 10 + *p++ - '0';
            }
            else if (*p == 'x' || *p == 'X') {      // hex!
                while ((tk = *++p) && ((tk >= '0' && tk <= '9') || (tk >= 'a' && tk <= 'f') || (tk >= 'A' && tk <= 'F')))
                    ival = ival * 16 + (tk & 15) + (tk >= 'A' ? 9 : 0);
            }
            else {
                while (*p >= '0' && *p <= '7')
                    ival = ival * 8 + *p++ - '0';   // octal!
            }
            tk = Num;
            return;
        }

        switch (tk) {
            case '\n':
                if (src) disasm_();
                ++line;
            case ' ': case '\t': case '\v': case '\f': case '\r':
                break;
            case '#':
                while (*p != 0 && *p != '\n') ++p;      // ignore includes
                break;
            case '/':
                if (*p == '/') {
                    ++p;
                    while (*p != 0 && *p != '\n') ++p;  // ignore // comments
                }
                else {
                    tk = Div;
                    return;
                }
                break;
            case '\'':
            case '"':
                if (tk == '"')
                    *--sym_ptr = 0;
                while (*p != 0 && *p != tk) {
                    if ((ival = *p++) == '\\') {
                        switch (ival = *p++) {
                            case 'n': ival = '\n'; break;
                            case 't': ival = '\t'; break;
                            case 'v': ival = '\v'; break;
                            case 'f': ival = '\f'; break;
                            case 'r': ival = '\r';
                        }
                    }
                    if (tk == '"')
                        *--sym_ptr = ival;         // backwards
                }
                ++p;
                if (tk == '"')  {                  // null terminated string
                    if ((int)((uint8_t*)sym_ptr-mem_end) & 1) {   // align it
                        char *s = sym_ptr--;            // cpy to align (TODO: do we want/need alignemnt?)
                        while (*s) {
                            s[-1] = s[0];
                            s++;
                        }
                        s[-1] = 0;
                    }
                    sym_ptr -= sizeof(Sym);         // rodata strings go in the symbol table during fucntion defs
                    ID_TOKEN(((Sym*)sym_ptr)) = 1;  // add a header
                } else
                    tk = Num;                       // char literal
                return;

            case '=': if (*p == '=') { ++p; tk = Eq; } else tk = Assign; return;
            case '+': if (*p == '+') { ++p; tk = Inc; } else tk = Add; return;
            case '-': if (*p == '-') { ++p; tk = Dec; } else tk = Sub; return;
            case '!': if (*p == '=') { ++p; tk = Ne; } return;
            case '<': if (*p == '=') { ++p; tk = Le; } else if (*p == '<') { ++p; tk = Shl; } else tk = Lt; return;
            case '>': if (*p == '=') { ++p; tk = Ge; } else if (*p == '>') { ++p; tk = Shr; } else tk = Gt; return;
            case '|': if (*p == '|') { ++p; tk = Lor; } else tk = Or; return;
            case '&': if (*p == '&') { ++p; tk = Lan; } else tk = And; return;
            case '^': tk = Xor; return;
            case '%': tk = Mod; return;
            case '*': tk = Mul; return;
            case '[': tk = Brak; return;
            case '?': tk = Cond; return;
            default: return;
        }
    }
}

void error(const char* bad)
{
    printf("%d: %s\n", line, bad);
    exit(-1);
}

void expect(int t)
{
    if (tk == t)
        next();
    else {
        char s[16] = {'\'',(char)t,0};
        if (t == While)
            strcpy(s,"'while' expected");
        else
            strcpy(s+2,"' expected");
        error(s);
    }
}

uint8_t _last_op;
void op1(uint8_t op)
{
    uint8_t i = _last_op;
#ifdef PEEP
    if ((i & 0xF0) == LEA_) {
        i &= 0x0F;
        if (op == PSH) {
            _last_op = *e = LEA_PSH | (i & 0xF);
            return;
        }
    }
#endif
    _last_op = *++e = op;
}

uint16_t op2(uint8_t op, int16_t v)
{
    #ifdef PEEP
    if (v >= -8 && v <= 7) {
        if (op == LEA) {
             _last_op = *++e = LEA_ | (v & 0xF);
            return 0xFFFF;   // plz don't point to this v anywhere
        }
        if (op == ADJ) {
            _last_op = *++e = ADJ_ | (v & 0xF);
            return 0xFFFF;   // plz don't point to this v anywhere
        }
    }
    #endif
    _last_op = *++e = op;
    *++e = v;
    *++e = v >> 8;
    return byte_ref(e-1);    // points to v
}

void expr(int lev);
const uint8_t lvs[] = {Xor,And,Eq,Lt,Lt,Shl,Shl,Shl,Shl,Add,Add};
void binop(uint8_t lev, uint8_t bc)
{
    next();
    op1(PSH);
    expr(lev);
    op1(bc);
    ty = INT;
}

void pushimm(int v)
{
#ifdef PEEP
    if (v >= -8 && v <= 7)
        op1(PSH_IMM | (v & 0xF));
    else {
        op1(PSH);
        op2(IMM,v);
    }
#else
    op1(PSH);
    op2(IMM,v);
#endif
}

//  increment(t);
void increment(int t)
{
#ifdef PEEP
    op1(INC | ty | (t == Inc ? 0 : 8));  //-ve if sub
#else
    pushimm((ty > PTR) ? 2 : 1);
    op1((t == Inc) ? ADD : SUB);
    op1((ty == CHAR) ? SC : SI);
#endif
}

void mul2()
{
    pushimm(2);
    op1(MUL);
}

void duptop()
{
    uint8_t c = *e--;
    op1(PSH);
    op1(c);
}

void expr(int lev)
{
    Sym* d;
    int16_t t;
    uint16_t c,b;

    switch (tk) {
        case 0:
            error("unexpected eof in expression");

        case Num:
            op2(IMM,ival);
            next();
            ty = INT;
            break;

        case '"':
            _rlink = op2(IMM,_rlink);   // save ref chain
            next();
            while (tk == '"') next();   // multiple strings
            ty = PTR;
            break;

        case Sizeof:
            next();
            expect('(');
            ty = INT; if (tk == Int) next(); else if (tk == Char) { next(); ty = CHAR; }
            while (tk == Mul) { next(); ty = ty + PTR; }
            expect(')');
            op2(IMM,ty == CHAR ? 1 : 2);
            ty = INT;
            break;

        case Id:
            d = id;
            next();
            if (tk == '(') {        // function call
                next();
                t = 0;
                while (tk != ')') { // push params
                    expr(Assign);
                    op1(PSH);       //
                    ++t;
                    if (tk == ',') next();
                }
                next();
                if (ID_CLASS(d) == Sys_)
                    op2(SYS,ID_VAL(d)-FIRST_SYS);   // index into sys proc table
                else if (ID_CLASS(d) == Fun_)
                    op2(JSR,ID_VAL(d));             // function call
                else
                    error("function call");
                if (t)
                    op2(ADJ,t);     // pop params if req
                ty = ID_TYPE(d);
            }
            else if (ID_CLASS(d) == Num_) {
                op2(IMM,ID_VAL(d));
                ty = INT;
            } else {
                if (ID_CLASS(d) == Loc_)
                    op2(LEA,loc - ID_VAL(d));       // locals
                else if (ID_CLASS(d) == Glo_) {     // globals
                    op2(GLO,ID_VAL(d));             // patch these up at some point? TODO
                } else
                    error("undefined variable");
                op1(((ty = ID_TYPE(d)) == CHAR) ? LC : LI);
            }
            break;

        case '(':
            next();
            if (tk == Int || tk == Char) {
                t = (tk == Int) ? INT : CHAR; next();
                while (tk == Mul) { next(); t = t + PTR; }
                expect(')');
                expr(Inc);
                ty = t;
            }
            else {
                expr(Assign);
                expect(')');
            }
            break;

        case Mul:
            next();
            expr(Inc);
            if (ty > INT) ty = ty - PTR; else error("dereference");
            op1((ty == CHAR) ? LC : LI);
            break;

        case And:
            next();
            expr(Inc);
            if (*e == LC || *e == LI) --e; else error("address-of");
            ty = ty + PTR;
            break;

        case '!': next(); expr(Inc); pushimm(0); op1(EQ); ty = INT; break;
        case '~': next(); expr(Inc); pushimm(-1); op1(XOR); ty = INT; break;
        case Add: next(); expr(Inc); ty = INT; break;

        case Sub:
            next();
            if (tk == Num) {
                op2(IMM,-ival);
                next();
            } else {
                op2(IMM,0);     // 0-val
                op1(PSH);
                expr(Inc);
                op1(SUB);
            }
            ty = INT;
            break;

        case Inc:
        case Dec:
            t = tk;
            next();
            expr(Inc);
            if (*e == LC || *e == LI)       // TODO: merge pre and post
                duptop();
            else
                error("lvalue in pre-increment");
            increment(t);
            break;

        default:
            error("expression");
    }

    while (tk >= lev) { // "precedence climbing" or "Top Down Operator Precedence" method
        t = ty;
        switch (tk) {
            case Assign:
                next();
                if (*e == LC || *e == LI) {
                    --e;
                    op1(PSH);
                }
                else
                    error("lvalue in assignment");
                expr(Assign);
                op1(((ty = t) == CHAR) ? SC : SI);
                break;

            case Cond:
                next();
                c = op2(BZ,0);
                expr(Assign);
                expect(':');
                b = op2(JMP,0);
                make_current(c);
                expr(Cond);
                make_current(b);
                break;

            case Lor: next(); c = op2(BNZ,0); expr(Lan); make_current(c); ty = INT; break;
            case Lan: next(); c = op2(BZ,0);  expr(Or);  make_current(c); ty = INT; break;

            case Or: case Xor: case And: case Eq:  case Ne:  case Lt:
            case Gt: case Le:  case Ge:  case Shl: case Shr:
                binop(lvs[tk-Or],OR+tk-Or); // will recurse
                break;

            case Add:
                next();
                op1(PSH);
                expr(Mul);
                if ((ty = t) > PTR)
                    mul2();
                op1(ADD);
                break;

            case Sub:
                next();
                op1(PSH);
                expr(Mul);
                if (t > PTR && t == ty) {
                    op1(SUB);
                    pushimm(2);
                    op1(DIV);
                    ty = INT;
                } else if ((ty = t) > PTR) {
                    mul2();
                    op1(SUB);
                } else
                    op1(SUB);
                break;

            case Mul: binop(Inc,MUL); break;
            case Div: binop(Inc,DIV); break;
            case Mod: binop(Inc,MOD); break;

            case Inc:
            case Dec:
                if (*e == LC || *e == LI)
                    duptop();
                else
                    error("lvalue in post-increment");
                increment(tk);
                pushimm((ty > PTR) ? 2 : 1);
                op1((tk == Inc) ? SUB : ADD);
                next();
                break;

            case Brak:
                next();
                op1(PSH);
                expr(Assign);
                expect(']');
                if (t > PTR)
                    mul2();
                else if (t < PTR)
                    error("pointer type expected");
                op1(ADD);
                op1(((ty = t - PTR) == CHAR) ? LC : LI);
                break;

            default:
                error("compiler error");
        }
    }
}

void paren_expr()
{
    expect('(');
    expr(Assign);
    expect(')');
}

void stmt()
{
    int16_t i, c, d;
    uint16_t b, b2;
    uint16_t br;
    switch (tk) {
        case If:        // if ( <expression> ) <statement> else <statement>
            next();
            paren_expr();
            b = op2(BZ,0);
            stmt();
            if (tk == Else) {
                b2 = op2(JMP,0);
                make_current(b);
                b = b2;
                next();
                stmt();
            }
            make_current(b);
            return;

        case While:     // while ( <expression> ) <statement>
            next();
            i = current_addr();
            paren_expr();
            b = op2(BZ,0);
            br = push_break();
            stmt();
            op2(JMP,i);
            make_current(b);
            pop_break(br);
            return;

        case Do:        // do <statement> while ( <expression> )
            next();
            i = current_addr();
            br = push_break();
            stmt();
            expect(While);
            paren_expr();
            op2(BNZ,i);
            pop_break(br);
            return;

        case For:       // for ( {<expression>}? ; {<expression>}? ; {<expression>}? ) <statement>
            next();
            expect('(');
            if (tk != ';')
                expr(Assign);       // init
            expect(';');

            b2 = 0;
            c = current_addr();     // check
            if (tk != ';') {
                expr(Assign);       // check for conditions
                b2 = op2(BZ,0);     // bail if complete, fill this address with done addr
            }
            b = op2(JMP,0);         // otherwise jump to body
            expect(';');

            d = current_addr();     // iterate
            if (tk != ')')
                expr(Assign);       // iterate, if any
            op2(JMP,c);             // back to check
            expect(')');

            make_current(b);
            br = push_break();
            stmt();
            op2(JMP,d);             // back to iterate

            pop_break(br);
            if (b2)
                make_current(b2);
            return;

        case Switch:
            next();
            paren_expr();

            // save case and default marks
            b = case_ptr;
            d = default_ptr;
            default_ptr = 0;
            br = push_break();

            // gen the case statements
            case_ptr = op2(JMP,0);
            stmt();

            swap16(case_ptr, default_ptr ? default_ptr : current_addr());       // patch default
            default_ptr = d;

            case_ptr = b;
            pop_break(br);
            return;

        case Case:
            next();
            b = op2(JMP,0);

            op1(PSH);
            i = swap16(case_ptr,byte_ref(e));   // BNZ target, i = last case index, patch case jumps
            expr(Or);                           // get the IMM case constant
            if (e[-2] != IMM) error("case immediate");
            {
                int16_t* i16 = (int16_t*)(e-1); // pointer to immediate value
                *i16 -= i;                      // subtract case constant
                int v = *i16;
                if (v >= -8 && v <= 7) {
                    e -= 2;
                    *e = IMM_ | (v & 0xF);      // short form saves case switch size
                }
                i += v;
                op1(SUB);
                case_ptr = op2(BNZ,i+*i16);     // store the delta switch value, will fixup branch later
            }
            make_current(b);                    // fixup jmp above

            expect(':');
            stmt();
            return;

        case Break:
            next();
            expect(';');
            _brk_link = op2(JMP,_brk_link);
            return;

        case Default:
            next();
            expect(':');
            default_ptr = current_addr();
            stmt();
            return;

        case Return:
            next();
            if (tk != ';')
                expr(Assign);
            op1(LEV);
            expect(';');
            return;

        case '{':
            next();
            while (tk != '}')
                stmt();
            next();
            return;

        case ';':
            next();
            return;

        default:
            expr(Assign);
            expect(';');
    }
}

// its a parameter or a local
void make_local(int ty, int i)
{
    ID_CLASS(id) = Loc_;
    ID_TYPE(id) = ty;
    ID_VAL(id) = i;
    next();
    if (tk == ',')
        next();
}

// write all the strings for a function
// pop local syms created during the function including strings
void flush_syms(char* mark)
{
    char* c = (char*)++e;
    while (sym_ptr < mark) {
        char *s = sym_ptr + sizeof(Sym);   // string
        int len = (int)strlen(s);
        Sym* sy = (Sym*)sym_ptr;
        sym_ptr = s + ((len+2) & ~1);       // align

        if (ID_TOKEN(sy) == 1) {
            int v = (int)(c - (char*)mem_pool);  // byte offset
            while (len--)
                *c++ = s[len];  // reverse
            *c++ = 0;
            printf("STR '");put_str((char*)mem_pool + v);printf("'\n");
            _rlink = swap16(_rlink,v);
        }
    }
    e = (uint8_t*)c;        // now inbetween functions
    le = e;                 // don't disassemble strings
}

// this can be const, removed from mem_pool TODO
Sym* init_syms()
{
    p = "break case char default else enum if int return sizeof switch for do while "
    "open read close printf malloc memset memcmp getchar rand exit void main";
    int i = Break;
    while (i <= While) {      // add keywords to symbol table
        next();
        ID_TOKEN(id) = i++;   // keywords
    }
    i = FIRST_SYS;            // add library to symbol table
    while (i <= EXIT) {
        next();
        ID_CLASS(id) = Sys_;
        ID_TYPE(id) = INT;
        ID_VAL(id) = i++;   //
    }
    next();
    printf("rom sym depth: %d\n",(int)((char*)mem_end-min_sim_ptr));

    // main needs to be last and mutable
    ID_TOKEN(id) = Char;       // handle void type
    next();
    return id;              // last sym is main!
}

// parse declarations
int parse()
{
    int base_type, ty, i;
    char* smark;
    line = 1;           // start parser
    next();
    while (tk) {
        base_type = INT; // basetype
        if (tk == Int)
            next();
        else if (tk == Char) {
            next();
            base_type = CHAR;
        } else if (tk == Enum) {
            next();
            if (tk != '{')
                next();
            if (tk == '{') {
                next();
                i = 0;
                while (tk != '}') {
                    if (tk != Id) error("bad enum identifier");
                    next();
                    if (tk == Assign) {
                        next();
                        if (tk != Num) error("bad enum initializer");
                        i = ival;
                        next();
                    }
                    ID_CLASS(id) = Num_;
                    ID_TYPE(id) = INT;
                    ID_VAL(id) = i++;
                    if (tk == ',')
                        next();
                }
                next();
            }
        }

        while (tk != ';' && tk != '}') {
            ty = base_type;
            while (tk == Mul) { next(); ty = ty + PTR; }
            if (tk != Id) error("bad global declaration");
            if (ID_CLASS(id)) error("duplicate global definition");
            next();
            ID_TYPE(id) = ty;

            if (tk == '(') {                // function declaration
                ID_CLASS(id) = Fun_;
                ID_VAL(id) = current_addr();
                next();
                smark = sym_ptr;            // don't keep syms from locals etc, delete them after definition
                i = 0;
                while (tk != ')') {
                    ty = INT;
                    if (tk == Int)
                        next();
                    else if (tk == Char) {
                        next();
                        ty = CHAR;
                    }
                    while (tk == Mul) {
                        next();
                        ty = ty + PTR;
                    }
                    if (tk != Id) error("bad parameter declaration");
                    if (ID_CLASS(id) == Loc_) error("duplicate parameter definition");
                    make_local(ty,i++);
                }
                next();

                expect('{');

                // local params
                loc = ++i;
                while (tk == Int || tk == Char) {
                    base_type = (tk == Int) ? INT : CHAR;
                    next();
                    while (tk != ';') {
                        ty = base_type;
                        while (tk == Mul)
                        {
                            next();
                            ty = ty + PTR;  // derefs
                        }
                        if (tk != Id) error("bad local declaration");
                        if (ID_CLASS(id) == Loc_) error("duplicate local definition");
                        make_local(ty,++i);
                    }
                    next();
                }

                _rlink = 0;                 // init chain of rodata refs
                op2(ENT,i - loc);
                while (tk != '}')
                    stmt();                 // body of function
                op1(LEV);
                flush_syms(smark);          // finalize rodata, drop local symbols
            } else {
                ID_CLASS(id) = Glo_;        // add a global to bss
                ID_VAL(id) = bss;           // from origin of bss
                bss += ty == CHAR ? 1 : 2;
            }
            if (tk == ',') next();
        }
        next();
    }
    return 0;
}

const char* FS_PATH(const char* path);

char* _src_mem = 0;
int open_src(const char* path)
{
    path = FS_PATH(path);

    int fd, i;
    struct stat st;
    if (stat(path, &st) != 0) {
        printf("could not open '%s'\n", path);
        return -1;
    }
    int len = (int)st.st_size;
    _src_mem = lp = p = (char*)malloc(len+1);
    p[len] = 0;
    if (!(fd = open(path, 0)))
        return -1;
    if ((i = (int)read(fd,p,st.st_size)) <= 0)
        return -1;
    close(fd);
    if (p[0] == 'W' && p[1] == 'C')   // it is a VM file
        return 1;
    printf("compiling '%s' (%d bytes)\n",path,len);
    return 0;
}

const char* swap_ext(char* p, int maxlen, const char* path, const char* ext)
{
    --maxlen;
    strncpy(p,path,maxlen-1);
    p[maxlen] = 0;
    char *s = p + strlen(p);
    while (*--s) {
        if (*s == '.') {
            strcpy(s,".wc");
            break;;
        }
    }
    return p;
}

int save(const char* path, uint8_t* _mem, int mem_size, int entry_pt, int code_size, int global_size)
{
    uint16_t hdr[4] = {
        'W' | ('C'<<8),
        (uint16_t)entry_pt,
        (uint16_t)code_size,
        (uint16_t)global_size
    };

    // swap suffix
    char p[256];
    swap_ext(p,sizeof(p)-1,path,".wc");

    // write it back
    FILE* f = fopen(FS_PATH(p),"wb");
    if (!f)
        return -1;
    fwrite(hdr,1,sizeof(hdr),f);
    fwrite(_mem,1,code_size,f);
    fclose(f);
    return 0;
}

int run_vm()
{
    uint16_t* hdr = (uint16_t*)&_src_mem[0];
    int entry_pt = hdr[1];
    int code_size = hdr[2];
    int global_size = hdr[3];
    memcpy(mem_pool,hdr+4,code_size);
    return run(mem_pool,(int)(mem_end-mem_pool),entry_pt,code_size,global_size);
}

int weec_main(int argc, const char **argv)
{
    while (--argc) {
        ++argv;
        if (**argv != '-')
            break;
        switch ((*argv)[1]) {
            case 's': src = 1;  break;
            case 'd': dbg = 1;  break;
            default: argc = 0;  break;
        }
    }
    if (argc < 1) {
        printf("usage: weec [-s] [-d] file ...\n");
        return -1;
    }
    src = dbg = 1;

    // read our source code
    const char* path = *argv;
    int vm;
    if ((vm = open_src(path)) < 0)
        return -2;

    // everything happens in place
    int mem_len = 32*1024;
    uint8_t* memp = (uint8_t*)malloc(mem_len);
    initmem(memp,mem_len);

    if (vm) {
        run_vm();
    } else {
        // setup keywords, keep track of main
        le = e = mem_pool;
        Sym* idmain = init_syms();
        p = lp;
        parse();

        //dump_syms();
        //printf("max sym depth: %d\n",(int)((char*)mem_end-min_sim_ptr));

        int entry = ID_VAL(idmain);
        if (!entry) {
            printf("main() not defined\n");
            return -1;
        }

        int codesize = (int)((e+1)-mem_pool);
        save(path,memp,mem_len,ID_VAL(idmain),codesize,bss);

        printf("oompiled to %db, running...\n",codesize);
        run(memp,mem_len,ID_VAL(idmain),codesize,bss);
    }
    free(memp);
    free(_src_mem);
    return 0;
}
