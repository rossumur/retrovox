//
//  terminal.hpp
//  RetroVox
//
//  Created by Peter Barrett on 1/1/22.
//  Copyright © 2022 Peter Barrett. All rights reserved.
//

#ifndef terminal_hpp
#define terminal_hpp

#include <stdio.h>
#include <string.h>

// hid modifiers
#define KEY_MOD_LCTRL  0x01
#define KEY_MOD_LSHIFT 0x02
#define KEY_MOD_LALT   0x04
#define KEY_MOD_LGUI   0x08
#define KEY_MOD_RCTRL  0x10
#define KEY_MOD_RSHIFT 0x20
#define KEY_MOD_RALT   0x40
#define KEY_MOD_RGUI   0x80

#define KEY_MOD_CTRL   (KEY_MOD_LCTRL|KEY_MOD_RCTRL)
#define KEY_MOD_SHIFT  (KEY_MOD_LSHIFT|KEY_MOD_RSHIFT)
#define KEY_MOD_ALT    (KEY_MOD_LALT|KEY_MOD_RALT)
#define KEY_MOD_GUI    (KEY_MOD_LGUI|KEY_MOD_RGUI)

#ifndef SIGINT
#define SIGINT 2
#endif

enum {
    Playfield32x192 = 1,
    Playfield40x96,
    Graphics96x96,
    Text24x24           // single buffer
};
void display_mode(int n);
void display_32x96(const uint8_t* rbuf, const uint8_t* gbuf, uint8_t c0, uint8_t c1);
uint8_t* display_backbuffer();
uint8_t* display_frontbuffer();
void display_clear();
void display_swap();

#define FIELD_SIZE (4+(4*192))              // for 32*192
#define FRAME_SIZE (FIELD_SIZE*2)           // even/odd fields or
extern uint8_t _frame_buf[FRAME_SIZE*2];   // double buffer of frames
extern uint8_t _frame_phase;                // current display

extern int _dmode;
extern uint8_t text_bits[4+5*12*24];        // Single buffered text

int wait_key();
int get_key();
void gui_key(int keycode, int pressed, int mods);
int getchar_();
int joy_map();

int measure_str(const char* s, int len, int font = 0);
int draw_chr(uint8_t c, int x, int y, bool invert = false, int font = 0);
int draw_str(const char* str, int x, int y, int font = 0);

void fill_rect(int x, int y, int w, int h, int c);
void set_pix(int x, int y, int c, uint8_t* d = 0);
void draw_line(int x0, int y0, int x1, int y1);

int get_signal(int s = SIGINT);

// global terminal
typedef std::string (*tab_hook)(const std::string& s);
int getline_s(char* dst, int maxlen);
int getchar_s();
void term_update();

int audio_play(const std::string& url);
int audio_stop();

int64_t audio_elapsed_us();
bool audio_playing();

class Terminal
{
    int mode;
    int dirty;
    int cx;
    int cy;
    int width;
    int height;
    char lastc;
    uint8_t* buf;
    
public:
    Terminal(int mode = 0) : mode(mode),dirty(1),cx(0),cy(0),width(24),lastc(0)
    {
        height = mode ? 16 : 24;
        buf = new uint8_t[width*height]();
    }

    ~Terminal()
    {
        delete buf;
    }

    void set_color(uint8_t c0, uint8_t c1)
    {
        text_bits[0] = c0;
        text_bits[1] = c1;
    }
    
    void cls()
    {
        fill_rect(0,0,96,5*24,0);
        memset(buf,0,width*height);
        cx=cy=0;
        dirty++;
        update();
    }

    void get_pos(int& x, int &y)
    {
        x = cx;
        y = cy;
    }

    void set_pos(int x, int y)
    {
        cx = x;
        cy = y;
    }

    void clear(int line)
    {
        memset(buf+cy*width,0,width);
    }

    void scroll()
    {
        memcpy(buf,buf+width,width*(height-1));
        clear(--cy);
    }

    void draw_cursor(int s)
    {
        buf[cx+cy*width] = s ? 32 | 0x80 : 32;
    }

    void update(bool force = 0)
    {
        fflush(stdout);
        if (dirty == 0 && !force)
            return;
        draw_cursor(1);
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                uint8_t c = buf[x + y*width];
                if (!c) c = ' ';
                draw_chr(c,x*4,y*5,0);
            }
        }
        dirty = 0;
    }

    void newline()
    {
        cx = 0;
        cy++;
        if (cy == height)
            scroll();
    }

    void escape(char c)
    {
        switch (c) {
            case 'c': cls(); break;
        }
        lastc = c;
    }

    void put(char c)
    {
        if (lastc == 033) { // escape!
            escape(c);
            return;
        }
        lastc = c;
        dirty++;
        draw_cursor(0);
        switch (c) {
            case '\b':
                if (!cx) {
                    if (cy == 0)
                        return;
                    --cy;
                    cx = width-1;
                } else
                    --cx;
                buf[cx + cy*width] = 0;
                break;
            case '\t':
                put(' ');put(' ');
                break;
            case '\n':
                newline();
                break;
            default:
                if (cx == width)
                    newline();
                buf[cx++ + cy*width] = c;
                break;
        }
        draw_cursor(1);
        if (dirty > width*height)
            update();
    }

    void puts(const char* s)
    {
        while (*s)
            put(*s++);
    }

    int getchar_()
    {
        update();
        return ::getchar_();
    }
    
    std::string getline(tab_hook t = 0)
    {
        std::string s;
        for (;;) {
            char c = getchar_();
            if (c == '\t' && t) {   // tabbed subs
                int n = s.length();
                s = t(s);
                while (n--)
                    put('\b');
                puts(s.c_str());
                update();
                continue;           // shell complete
            }
            if (c == '\r')
                c = '\n';
            if (c == '\n')
                return s;
            if (c == '\b') {
                if (s.length())
                    s.resize(s.length()-1);
                else
                    continue;
            } else
                s += c;
            put(c);
            update();
        }
    }
};

class Texture {
public:
    float u,v,z,uz;
    float ustep,vstep;
};

void vline_(int x, int y1, int y2, int c, Texture& tx);

#endif /* terminal_hpp */
