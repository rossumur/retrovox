//
//  tinybasic.cpp
//  RetroVox
//
//  Created by Peter Barrett on 12/7/21.
//  Copyright © 2021 Peter Barrett. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "streamer.h"
#include "terminal.h"
#include "utils.h"
using namespace std;

// A tiny basic

void sam_char(char c);
void sp0256_char(char v);

class FBasic {
public:
    const char* tb_statements[21] = {
        "LET",
        "PRINT",
        "PR",
        "IF",
        "THEN",
        "GOTO",
        "GOSUB",
        "RETURN",
        "REM",
        "INPUT",
        "CLS",
        "LIST",
        "RUN",
        "END",
        "LOAD",
        "SAVE",
        "NEW",
        "EXIT",
        "SAM",
        "SP",
        0
    };

    enum tb_statements_ {
        T_UNKNOWN = -1,
        T_LET,
        T_PRINT,
        T_PR,
        T_IF,
        T_THEN,
        T_GOTO,
        T_GOSUB,
        T_RETURN,
        T_REM,
        T_INPUT,
        T_CLS,
        T_LIST,
        T_RUN,
        T_END,

        T_LOAD,
        T_SAVE,
        T_NEW,
        T_EXIT,

        T_SAM,
        T_SP,
    };

    const char* tb_rel[8] = {
        "<=",
        ">=",
        "<>",
        "><",
        "<",
        ">",
        "=",
        0
    };

    enum tb_rel_ {
        T_LEQ,
        T_GEQ,
        T_NE,
        T_NE2,
        T_LE,
        T_GT,
        T_EQ
    };

    const char* tb_function[6] = {
        "ABS",  // 1
        "RND",  // 1
        "AND",  // 2
        "OR",   // 2
        "XOR",  // 2
        0
    };

    enum tb_function_ {
        T_ABS,
        T_RND,
        T_AND,
        T_OR,
        T_XOR,
    };

    enum {
        OK = 0,
        SYNTAX_ERR = -1,
        VAR_EXPECTED_ERR = -2,
        VALUE_EXPECTED_ERR = -3,
        COMPARE_ERR = -4,
        RELATION_EXPECTED_ERR = -5,
        TERM_EXPECTED_ERR = -6,
        INVALID_PRINT_ERR = -7,
        LINE_END_EXPECTED_ERR = -8,
        GOTO_LINENUMBER_EXPECTED_ERR = -9,
        GOSUB_LINENUMBER_EXPECTED_ERR = -10,
        LINE_MISSING_ERR = -11,
        EXTRA_RETURN_ERR = -12,
        LOAD_ERR = -13,
        SAVE_ERR = -14,
    };

    enum {
        RUNNING = 1,
        PROG_MEM = 0x10000,
        STK_SIZE = 32,
        LBUF_SIZE = 72
    };

    char prog[PROG_MEM];
    char* prog_end = prog;

    int line_num;   // current line number
    char lbuf[LBUF_SIZE];
    char* line;

    int16_t vars[26];
    int16_t stack[STK_SIZE];
    int16_t sp = 0;

    uint8_t state = 0;
    uint8_t cx = 0;     // for tabs

    void clear()
    {
        prog_end = prog;
        prog[0] = 0;
        sp = 0;
        clear_vars();
    }

    void cls()
    {
        putchar(033);
        putchar('c');
    }

    int _t; // current token
    void printchar(char c)
    {
        switch (_t) {
            case T_SAM: sam_char(c);    break;  // software automatic mouth
            case T_SP:  sp0256_char(c); break;  // SP0256 phonemmes
            default:
                putchar(c);
                cx++;
                break;
        }
    }

    void printtab()
    {
        while (cx & 7)
            printchar(' ');
    }

    void print(const char* s)
    {
        while (*s)
            printchar(*s++);
    }

    void println(const char* s = 0)
    {
        if (s) print(s);
        printchar('\n');
    }

    void printnum(int16_t n)
    {
        if (n < 0) {
            printchar('-');
            n = -n;
        }
        char b[8];
        int i = 0;
        do {
            b[i++] = n % 10;
            n /= 10;
        } while(n);
        while (i--)
            printchar(b[i]+'0');
    }

    int get_line(char* dst, int len, char prompt)
    {
        putchar(prompt);
        int i = getline_s(dst,len-2);
        putchar('\n');
        dst[i] = '\n';
        dst[i+1] = 0;
        return i+1;
    }

    void get_line()
    {
        get_line(lbuf,sizeof(lbuf)-2,'>');
        line = lbuf;
    }

    int input_number()
    {
        char b[16];
        get_line(b,sizeof(b),'?');
        if (b[1] == '\n') {
            char c = toupper(b[0]);
            if (c >= 'A' && c <= 'Z')
                return get_var(c);
        }
        return atoi(b);
    }

    char get()
    {
        skipw();
        return *line++;
    }

    char peek()
    {
        skipw();
        return line[0];
    }

    bool at_eol()
    {
        return peek() < ' ';
    }

    void clear_vars()
    {
        memset(vars,0,sizeof(vars));
        memset(prog_end,0,prog_end-prog);
    }

    // TODO : might want to be 1 based?
    // single 1 dimensional array at end of program memory
    int16_t* get_array(int16_t i)
    {
        char* p = prog_end + (i << 1);
        if ((p-prog) & 1)
            p++;
        if (i < 0 || p >= (prog + PROG_MEM))
            return 0;
        return (int16_t*)p;
    }

    void set_var(char n, int16_t v)
    {
        if (n == '@') {
            int16_t* p = get_array(pop());
            if (p)
                *p = v;
            // else RANGE ERROR
        } else {
            //printf("[%c=%d]\n",n,v);
            vars[n-'A'] = v;
        }
    }

    int16_t get_var(char n)
    {
        if (n == '@') {
            int16_t* p = get_array(pop());
            return p ? *p : 0;  // RANGE ERROR
        }
        return vars[n-'A'];
    }

    int tok(const char* t[])
    {
        skipw();
        for (int i = 0; t[i] ; i++) {
            const char* s = t[i];
            int j;
            for (j = 0; s[j] == toupper(line[j]); j++)
                ;
            if (s[j] == 0) {
                line += j;
                return i;
            }
        }
        return -1;
    }

    // skip whitespace
    void skipw()
    {
        while (line[0] == ' ')
            line++;
    }

    bool stop()
    {
        state &= ~RUNNING;
        return false;
    }

    void push(int16_t n)
    {
        if (sp == STK_SIZE)
            stop();
        else
            stack[sp++] = n;
    }

    int16_t pop()
    {
        if (!sp) {
            stop();
            return 0;
        }
        return stack[--sp];
    }

    void add()
    {
        --sp;
        stack[sp-1] += stack[sp];
    }

    void sub()
    {
        --sp;
        stack[sp-1] -= stack[sp];
    }

    void mul()
    {
        --sp;
        stack[sp-1] *= stack[sp];
    }

    void div()
    {
        --sp;
        stack[sp-1] /= stack[sp];
    }

    void neg()
    {
        stack[sp-1] = -stack[sp-1];
    }

    char match(char c)
    {
        if (peek() != c)
            return 0;
        return get();
    }

    bool number()
    {
        char neg = match('-');
        char c = peek();
        if (c < '0' || c > '9')
            return false;   // sign has been consumed already?
        int16_t n = read_number();
        push(neg ? -n : n);
        return true;
    }

    // put a variable number of things on the stack
    int params()
    {
        int i = 0;
        if (!match('('))
            return i;
        for (;;) {
            if (expression()) {
                i++;
                if (match(','))
                    continue;
            }
            return match(')') ? i : -1;
        }
    }

    // functions!
    void chk(bool ok, int e = SYNTAX_ERR)
    {
        if (!ok)
            printf("\nerror %d\n",e);
    }

    bool function()
    {
        auto f = tok(tb_function);
        if (f < 0)
            return false;   // not a recognized function

        int16_t r = 0;
        int p = params();
        if (f <= T_RND) {
            chk(p == 1);    // 1 param
            r = pop();
            switch (f) {
                case T_ABS: r = abs(r);         break;
                case T_RND: r = (rand() % r)+1; break;
            }
        } else {
            chk(p == 2);    // 2 params
            int16_t b = pop();
            int16_t a = pop();
            switch (f) {
                case T_AND: r = a & b; break;
                case T_OR:  r = a | b; break;
                case T_XOR: r = a ^ b; break;
            }
        }
        push(r);
        return true;
    }

    bool factor()
    {
        if (number())
            return true;    // was a number
        if (function())
            return true;    // was a function
        if (var()) {
            push(get_var(pop()));
            return true;
        }
        if (match('(')) {
            if (!expression())
                return false;   // err TODO
            if (match(')'))
                return true;    // err if missing
        }
        return false;
    }

    bool term()
    {
        factor();
        for (;;) {
            if (match('*')) {
                factor();
                mul();
            } else if (match('/')) {
                factor();
                div();
            } else
                return true;
        }
    }

    bool expression()
    {
        if (match('-')) {
            term();
            neg();      // unary minus
        } else {
            match('+'); // ignore unary plus
            term();
        }
        for (;;) {
            if (match('+')) {
                term();
                add();
            } else if (match('-')) {
                term();
                sub();
            } else
                return true;
        }
    }

    bool var()
    {
        if (match('@')) {         // our single array
            int i = params();
            if (i != 1)
                return false;   // TODO err
            push('@');          // @/index on stack
            return true;
        }
        char v = toupper(peek());
        if (v < 'A' || v > 'Z')
            return false;       // not a var
        push(v);                // get the numeric value of the var
        get();
        return true;
    }

    bool relop(int r)
    {
        int16_t s = pop();
        s = pop()-s;
        switch (r) {
            case T_LEQ: return s <= 0;
            case T_GEQ: return s >= 0;
            case T_NE:
            case T_NE2: return s != 0;
            case T_LE: return s < 0;
            case T_GT: return s > 0;
            case T_EQ: return s == 0;
        }
        return false;
    }

    // read line number
    int read_number()
    {
        while (*line <= ' ') {   // will skip errant newlines etc {
            line++;
            if (line >= prog_end)
                return 0;
        }
        int num = 0;
        while (line[0] >= '0' && line[0] <= '9') {
            num = num * 10 + line[0] - '0';
            line++;
        }
        return num;
    }

    // get the line >= target TODO 32767
    // advances line pointer to linenum of target
    int find_line(int16_t target, int& start, int& end)
    {
        int p = (int)(prog_end-prog);
        start = 0;
        end = 0;
        while (end < p) {
            if (prog[end] == '\n') {
                line = prog+start;
                int n = read_number();
                if (n >= target || n == 0) {
                    line = prog + start;
                    end++;
                    return n;
                }
                start = end+1;
            }
            end++;
        }
        return 0;
    }

    // insert lbuf into program
    bool insert_line(int num)
    {
        int len = 0;
        while (lbuf[len++] != '\n')         // length to insert
            ;
        if (prog_end + len > (prog + sizeof(prog)))
            return false;                   // won't fit

        int s,e;
        bool blank = at_eol();
        int new_num = find_line(num,s,e);
        if (new_num == num) {               // delete existing line
            memcpy(prog+s,prog+e,prog_end-(prog+e));
            prog_end -= e-s;
        }
        if (blank)
            return true;                    // blank line, removed line number
        if (new_num <= 0) {
            memcpy(prog_end,lbuf,len);  // append
        } else {
            memcpy(prog+s+len,prog+s,prog_end-(prog+s));
            memcpy(prog+s,lbuf,len);    // insert
        }
        prog_end += len;
        return true;
    }

    bool goto_(int16_t nline)
    {
        // printf("[goto %d]\n",nline);
        if (nline <= 0)
            return false;
        int s,e;
        if (find_line(nline,s,e) != nline)
            return false;       // didn't find the line
        return true;
    }

    int load_save(bool load, const char* path)
    {
        int i = 0;
        while (path[i] != '"' && path[i] != '\n')
            i++;
        path = FS_PATH(string(path,i));
        if (load) {
            clear();
            int n = read_file(path,prog,sizeof(prog)-2);
            if (n <= 0)
                return LOAD_ERR;
            if (prog[n-1] != '\n')
                prog[n++] = '\n';   // force a newline at end
            prog_end = prog + n;
        } else {
            if (write_file(path,prog,(int)(prog_end-prog)) < 0)
                return SAVE_ERR;
        }
        return 1;
    }

    int statement()
    {
        int16_t i;
        int s,e;
        tb_statements_ t = (tb_statements_)tok(tb_statements);
        _t = t;
        switch (t) {
            case -1:        // optional LET
            case T_LET:
                if (!var())
                    return t == -1 ? SYNTAX_ERR : VAR_EXPECTED_ERR;
                if (get() != '=')
                    return SYNTAX_ERR;
                if (!expression())
                    return VALUE_EXPECTED_ERR;
                i = pop();
                set_var(pop(),i);
                break;

            case T_IF:
                if (!expression())
                    return VALUE_EXPECTED_ERR;

                i = tok(tb_rel);
                if (i < 0)
                    return RELATION_EXPECTED_ERR;

                if (!expression())
                    return VALUE_EXPECTED_ERR;

                if (relop(i)) {
                    char *tmp = line;
                    if (tok(tb_statements) != T_THEN)   // optional 'THEN'
                        line = tmp;
                    return statement();                 // condition true, execute statement after THEN
                }
                while (*line != '\n')
                    line++;     // condition falied, seek end of line
                return 0;

            case T_PRINT:
            case T_PR:
            case T_SAM:
            case T_SP:
                {
                    cx = 0;                 // tabs
                    char c = 0;
                    while (1) {
                        if (at_eol() || peek() == ':')
                            break;
                        if (peek() == '"') {
                            get();
                            while ((c = *line++) != '"') {
                                printchar(c);
                            }
                        } else if (!expression()) {
                            return INVALID_PRINT_ERR;
                        } else {
                            printnum(pop());
                        }

                        c = peek();
                        if (c == ',') {
                            printtab();         // pad to 8 spaces
                            get();
                        } else if (c == ';') {
                            get();              // no padding
                        }
                    }
                    if (c != ';')
                        println();
                }
                break;

            case T_GOTO:
                if (!expression())
                    return GOTO_LINENUMBER_EXPECTED_ERR;
                i = pop();
                if (!at_eol())
                    return LINE_END_EXPECTED_ERR;
                if (!goto_(i))
                    return LINE_MISSING_ERR;
                return 1;

            case T_GOSUB:
                if (!expression())
                    return GOSUB_LINENUMBER_EXPECTED_ERR;
                i = pop();
                if (peek() != '\n' && peek() != ':')
                    return LINE_END_EXPECTED_ERR;
                push((int)(line-prog));         // resume address
                if (!goto_(i))
                    return LINE_MISSING_ERR;
                return 1;

            case T_RETURN:
                if (!at_eol())
                    return LINE_END_EXPECTED_ERR;
                line = prog + (uint16_t)pop();
                return 1;

            case T_INPUT:
                for (;;) {
                    if (!var())
                        return VAR_EXPECTED_ERR;
                    set_var(pop(),input_number());  // how?
                    if (peek() == ',') {
                        get();
                        continue;
                    }
                    break;
                }
                break;

            case T_REM:
                while (*line != '\n')
                    line++;
                return 0;

            case T_NEW:
                clear();
                return 1;

            case T_CLS:
                cls();
                return 0;

            case T_END:
                stop();
                return 1;

            case T_LIST:
                i = expression() ? pop() : 0;
                i = find_line(i,s,e);
                while (line < prog_end)
                    printchar(*line++);
                if (line[-1] != '\n')
                    putchar('\n');
                return 1;

            case T_RUN:
                sp = 0;
                clear_vars();
                line = prog;
                state |= RUNNING;   // off and running
                return 1;

            case T_LOAD:
            case T_SAVE:
                if (get() != '"')
                    return SYNTAX_ERR;
                return load_save(t == T_LOAD,line);

            case T_EXIT:
                return T_EXIT;

            default:
                break;
        }
        return 0;
    }

    int statements()
    {
        for (;;) {
            int e = statement();
            if (e)
                return e;   // <= 0 error, 1 if line no longer points to statements, 2 waiting for input

            for (;;) {
                char c = get();
                if (c =='\n') return 0;     // done with line
                if (c == ':') break;        // more statements on this line
                if (c == '\r') continue;    // strip whitespace
                stop();                     // crap at end of line?
                return -1;
            }
        }
        return 0;
    }

    // do a statement at a time
    int interpret()
    {
        int e = 0;
        if (state & RUNNING) {
            line_num = read_number();
            if (!line_num || get_signal())
                stop();
            else
               e = statements();
        } else {
            get_line();
            if (!at_eol()) {
                if ((line_num = read_number()))
                    insert_line(line_num);
                else {
                    e = statements();
                    if (e == T_EXIT)
                        return -1;
                }
            }
        }
        if (e < 0) {
            print("Error ");
            printnum(e);
            if (line_num) {
                print(" at line ");
                printnum(line_num);
            }
            println(".");
        }
        return 0;
    }

    void splash()
    {
        cls();
        println("Welcome to Teeny Basic\n@ Copyleft 1975 Rossum\nAll wrongs reserved\n");
    }
};

int tinybasic_main(int argc, const char * argv[])
{
    FBasic* b = new FBasic();
    b->splash();
    if (argc >= 2)
        b->load_save(1,argv[1]);
    for (;;) {
        if (b->interpret() < 0)
            break;
    }
    delete b;
    return 0;
}
