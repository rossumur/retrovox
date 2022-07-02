//
//  terminal.cpp
//  RetroVox
//
//  Created by Peter Barrett on 1/1/22.
//  Copyright © 2022 Peter Barrett. All rights reserved.
//

#include <stdio.h>
#include <math.h>
#include <unistd.h>

#include "streamer.h"
#include "utils.h"
#include "terminal.h"
using namespace std;

//====================================================================================
//====================================================================================
// low level drawing

// 5x3, lsb set for descenders
const uint16_t _descenders[96] = {
    0x0000, // ' '
    0x4904, // '!'
    0xB400, // '"'
    0xBEFA, // '#'
    0x7BBC, // '$'
    0xA54A, // '%'
    0x555C, // '&'
    0x4800, // '\''
    0x2922, // '('
    0x4494, // ')'
    0x1550, // '*'
    0x0BA0, // '+'
    0x0028, // ','
    0x0380, // '-'
    0x0004, // '.'
    0x2548, // '/'
    0x76DC, // '0'
    0x592E, // '1'
    0xC5CE, // '2'
    0xC51C, // '3'
    0x92F2, // '4'
    0xF39C, // '5'
    0x73DC, // '6'
    0xE548, // '7'
    0x77DC, // '8'
    0x779C, // '9'
    0x0820, // ':'
    0x0828, // ';'
    0x2A22, // '<'
    0x1C70, // '='
    0x88A8, // '>'
    0xE504, // '?'
    0x56C6, // '@'
    0x77DA, // 'A'
    0x775C, // 'B'
    0x7246, // 'C'
    0xD6DC, // 'D'
    0x73CE, // 'E'
    0x73C8, // 'F'
    0x72D6, // 'G'
    0xB7DA, // 'H'
    0xE92E, // 'I'
    0x64D4, // 'J'
    0xB75A, // 'K'
    0x924E, // 'L'
    0xBEDA, // 'M'
    0xD6DA, // 'N'
    0x56D4, // 'O'
    0xD7C8, // 'P'
    0x76F6, // 'Q'
    0x775A, // 'R'
    0x711C, // 'S'
    0xE924, // 'T'
    0xB6D6, // 'U'
    0xB6D4, // 'V'
    0xB6FA, // 'W'
    0xB55A, // 'X'
    0xB524, // 'Y'
    0xE54E, // 'Z'
    0x6926, // '['
    0x9112, // '\'
    0x6496, // ']'
    0x5400, // '^'
    0x000E, // '_'
    0x4400, // '`'
    0x0ED6, // 'a'
    0x9ADC, // 'b'
    0x0E46, // 'c'
    0x2ED6, // 'd'
    0x0EE6, // 'e'
    0x5668, // 'f'
    0x559D, // 'g'
    0x935A, // 'h'
    0x4122, // 'i'
    0x4129, // 'j'
    0x975A, // 'k'
    0x4922, // 'l'
    0x17DA, // 'm'
    0x1ADA, // 'n'
    0x0AD4, // 'o'
    0xD6E9, // 'p'
    0x76B3, // 'q'
    0x1748, // 'r'
    0x0F1C, // 's'
    0x9A46, // 't'
    0x16D6, // 'u'
    0x16DC, // 'v'
    0x16FA, // 'w'
    0x155A, // 'x'
    0xB595, // 'y'
    0x1CAE, // 'z'
    0x6B26, // '{'
    0x4924, // '|'
    0xC9AC, // '}'
    0x5400, // '~'
    0x56F0, // ''
};

// no descenders for text mode blit
const uint16_t _5x3[96] = {
    0x0000, // ' '
    0x4904, // '!'
    0xB400, // '"'
    0xBEFA, // '#'
    0x7BBC, // '$'
    0xA54A, // '%'
    0x555C, // '&'
    0x4800, // '\''
    0x2922, // '('
    0x4494, // ')'
    0x1550, // '*'
    0x0BA0, // '+'
    0x0028, // ','
    0x0380, // '-'
    0x0004, // '.'
    0x2548, // '/'
    0x76DC, // '0'
    0x592E, // '1'
    0xC5CE, // '2'
    0xC51C, // '3'
    0x92F2, // '4'
    0xF39C, // '5'
    0x73DC, // '6'
    0xE548, // '7'
    0x77DC, // '8'
    0x779C, // '9'
    0x0820, // ':'
    0x0828, // ';'
    0x2A22, // '<'
    0x1C70, // '='
    0x88A8, // '>'
    0xE504, // '?'
    0x56C6, // '@'
    0x77DA, // 'A'
    0x775C, // 'B'
    0x7246, // 'C'
    0xD6DC, // 'D'
    0x73CE, // 'E'
    0x73C8, // 'F'
    0x72D6, // 'G'
    0xB7DA, // 'H'
    0xE92E, // 'I'
    0x64D4, // 'J'
    0xB75A, // 'K'
    0x924E, // 'L'
    0xBEDA, // 'M'
    0xD6DA, // 'N'
    0x56D4, // 'O'
    0xD7C8, // 'P'
    0x76F6, // 'Q'
    0x775A, // 'R'
    0x711C, // 'S'
    0xE924, // 'T'
    0xB6D6, // 'U'
    0xB6D4, // 'V'
    0xB6FA, // 'W'
    0xB55A, // 'X'
    0xB524, // 'Y'
    0xE54E, // 'Z'
    0x6926, // '['
    0x9112, // '\'
    0x6496, // ']'
    0x5400, // '^'
    0x000E, // '_'
    0x4400, // '`'
    0x0ED6, // 'a'
    0x9ADC, // 'b'
    0x0E46, // 'c'
    0x2ED6, // 'd'
    0x0EE6, // 'e'
    0x5668, // 'f'
    0x559C, // 'g'
    //0x559D, // 'g'
    //0x0E56, // G lower
    0x935A, // 'h'
    0x4122, // 'i'
    0x4128, // 'j'
    //0x4129, // 'j'
    0x975A, // 'k'
    0x4922, // 'l'
    0x17DA, // 'm'
    0x1ADA, // 'n'
    0x0AD4, // 'o'
    0x1AE8, // 'p'
    //0xD6E9, // 'p'
    0x0EB2, // 'q'
    //0x76B3, // 'q'
    0x1748, // 'r'
    0x0F1C, // 's'
    0x9A46, // 't'
    0x16D6, // 'u'
    0x16DC, // 'v'
    0x16FA, // 'w'
    0x155A, // 'x'
    0x01594, // 'y'
    //0xB595, // 'y'
    0x1CAE, // 'z'
    0x6B26, // '{'
    0x4924, // '|'
    0xC9AC, // '}'
    0x5400, // '~'
    0x56F0, // ''
};

const uint16_t _proportional[96] = {
    0x0000, // ' '
    0x9208, // '!'
    0xB400, // '"'
    0xBEFA, // '#'
    0x7BBC, // '$'
    0xA54A, // '%'
    0x555C, // '&'
    0x4800, // '''
    0x2922, // '('
    0x4494, // ')'
    0x1550, // '*'
    0x0BA0, // '+'
    0x0028, // ','
    0x0300, // '-'
    0x0008, // '.'
    0x2548, // '/'
    0x76DC, // '0'
    0x592E, // '1'
    0xC5CE, // '2'
    0xC51C, // '3'
    0x92F2, // '4'
    0xF39C, // '5'
    0x73DC, // '6'
    0xE548, // '7'
    0x77DC, // '8'
    0x779C, // '9'
    0x1040, // ':'
    0x0828, // ';'
    0x2A22, // '<'
    0x1860, // '='
    0x88A8, // '>'
    0xE504, // '?'
    0x56C6, // '@'
    0x77DA, // 'A'
    0x775C, // 'B'
    0x7246, // 'C'
    0xD6DC, // 'D'
    0x73CE, // 'E'
    0x73C8, // 'F'
    0x72D6, // 'G'
    0xB7DA, // 'H'
    0xE92E, // 'I'
    0x64D4, // 'J'
    0xB75A, // 'K'
    0x924E, // 'L'
    0xBEDA, // 'M'
    0xD6DA, // 'N'
    0x56D4, // 'O'
    0xD7C8, // 'P'
    0x76F6, // 'Q'
    0x775A, // 'R'
    0x711C, // 'S'
    0xE924, // 'T'
    0xB6D6, // 'U'
    0xB6D4, // 'V'
    0xB6FA, // 'W'
    0xB55A, // 'X'
    0xB524, // 'Y'
    0xE54E, // 'Z'
    0xD24C, // '['
    0x9112, // '\'
    0xC92C, // ']'
    0x5400, // '^'
    0x000C, // '_'
    0x4400, // '`'
    0x0ED6, // 'a'
    0x9ADC, // 'b'
    0x0A44, // 'c'
    0x2ED6, // 'd'
    0x0EE6, // 'e'
    0x5348, // 'f'
    0x559C, // 'g'
    0x9ADA, // 'h'
    0x8248, // 'i'
    0x4128, // 'j'
    0x975A, // 'k'
    0x9248, // 'l'
    0x17DA, // 'm'
    0x1ADA, // 'n'
    0x0AD4, // 'o'
    0x1AE8, // 'p'
    0x0EB2, // 'q'
    0x0A48, // 'r'
    0x0F1C, // 's'
    0x9A44, // 't'
    0x16D6, // 'u'
    0x16DC, // 'v'
    0x16FA, // 'w'
    0x155A, // 'x'
    0x1594, // 'y'
    0x1CAE, // 'z'
    0x6B26, // '{'
    0x4924, // '|'
    0xC9AC, // '}'
    0x5400, // '~'
    0x56F0, // ''
};

const uint8_t _proportional_widths[96] = {
    2, // ' '
    2, // '!'
    4, // '"'
    4, // '#'
    4, // '$'
    4, // '%'
    4, // '&'
    4, // '''
    4, // '('
    4, // ')'
    4, // '*'
    4, // '+'
    3, // ','
    3, // '-'
    2, // '.'
    4, // '/'
    4, // '0'
    4, // '1'
    4, // '2'
    4, // '3'
    4, // '4'
    4, // '5'
    4, // '6'
    4, // '7'
    4, // '8'
    4, // '9'
    2, // ':'
    3, // ';'
    4, // '<'
    3, // '='
    4, // '>'
    4, // '?'
    4, // '@'
    4, // 'A'
    4, // 'B'
    4, // 'C'
    4, // 'D'
    4, // 'E'
    4, // 'F'
    4, // 'G'
    4, // 'H'
    4, // 'I'
    4, // 'J'
    4, // 'K'
    4, // 'L'
    4, // 'M'
    4, // 'N'
    4, // 'O'
    4, // 'P'
    4, // 'Q'
    4, // 'R'
    4, // 'S'
    4, // 'T'
    4, // 'U'
    4, // 'V'
    4, // 'W'
    4, // 'X'
    4, // 'Y'
    4, // 'Z'
    3, // '['
    4, // '\'
    3, // ']'
    4, // '^'
    3, // '_'
    3, // '`'
    4, // 'a'
    4, // 'b'
    3, // 'c'
    4, // 'd'
    4, // 'e'
    3, // 'f'
    4, // 'g'
    4, // 'h'
    2, // 'i'
    3, // 'j'
    4, // 'k'
    2, // 'l'
    4, // 'm'
    4, // 'n'
    4, // 'o'
    4, // 'p'
    4, // 'q'
    3, // 'r'
    4, // 's'
    3, // 't'
    4, // 'u'
    4, // 'v'
    4, // 'w'
    4, // 'x'
    4, // 'y'
    4, // 'z'
    4, // '{'
    4, // '|'
    4, // '}'
    4, // '~'
    4, // ''
};

int _dmode = Text24x24;
// single frame of 32x192 = 1544
// single frame of 96x96 = 1156
// single frame of Text24x24 = 1444

#define FIELD_SIZE (4+(4*192))              // for 32*192
#define FRAME_SIZE (FIELD_SIZE*2)           // even/odd fields or
uint8_t _frame_buf[FRAME_SIZE*2] = {0};    // double buffer of frames
uint8_t _frame_phase = 0;                   // current display

uint8_t text_bits[4+5*12*24];               // Single buffered text

void display_mode(int n)
{
    _dmode = n;
    display_clear();
}

uint8_t* get_pix()
{
    return text_bits + 4;
}

void clear_pix()
{
    memset(get_pix(),0,12*120);
}

void set_pix(int x, int y, int c, uint8_t* d)
{
    if (x < 0 || y < 0 || x >= 96 || y >= 120)
        return;
    d = d ? d: text_bits;
    uint8_t* b = d + 4 + y*12 + (x >> 3);
    if (c)
        *b |= 0x80 >> (x & 7);
    else
        *b &= ~(0x80 >> (x & 7));
}

uint8_t get_pix(int x, int y)
{
    if (x < 0 || y < 0 || x >= 96 || y >= 120)
        return 0;
    uint8_t* b = text_bits + 4 + y*12 + (x >> 3);
    return *b & (0x80 >> (x & 7));
}

void hline(int x, int y, int w, int c)
{
    while (w--)
        set_pix(x++,y,c);
}

void fill_rect(int x, int y, int w, int h, int c)
{
    if (w < 0 || h < 0)
        return;
    while (h--)
        hline(x,y++,w,c);
}

// this works in text mode?
void draw_line(int x0, int y0, int x1, int y1)
{
    uint8_t* fb = display_backbuffer();
    int dx = abs(x1-x0), sx = x0<x1 ? 1 : -1;
    int dy = abs(y1-y0), sy = y0<y1 ? 1 : -1;
    int err = (dx>dy ? dx : -dy)/2, e2;
    for(;;)
    {
        set_pix(x0,y0,1,fb);
        if (x0==x1 && y0==y1) break;
        e2 = err;
        if (e2 >-dx) { err -= dy; x0 += sx; }
        if (e2 < dy) { err += dx; y0 += sy; }
    }
}

int measure_str(const char* s, int len, int font)
{
    if (font != 2)
        return 4*len;
    int w = 0;
    while (len--)
        w += _proportional_widths[*s++-32];
    return w;
}

int draw_chr(uint8_t c, int x, int y, bool invert, int font)
{
    /*
     for (int i = 0; i < 96; i++) {
     printf("0x%04X, // '%c'\n",_proportional[i*2+1],(char)(i+32));
     }
     */
    if (c & 0x80)
        invert = true;
    c &= 0x7F;
    if (c < 32)
        c = 0x7F;

    int w = (font == 2) ? _proportional_widths[c-32] : 4;
    uint16_t d = ((font == 2) ? _proportional : _5x3)[c-32];
    if (d & 1)
        y++;
    for (int yy = 0; yy < 5; yy++) {
        for (int xx = x; xx < x+3; xx++) {
            bool bit = (d & 0x8000) != 0;
            set_pix(xx,y,invert != bit);
            d <<= 1;
        }
        y++;
    }
    return w;
}

int draw_str(const char* str, int x, int y, int font)
{
    while (*str)
        x = draw_chr(*str++,x,y,font);
    return x;
}

void fill_buf(uint8_t* dst, uint8_t c0, uint8_t c1, const uint8_t* even, const uint8_t* odd)
{
    dst[0] = c0;
    dst[1] = c1;
    dst += 4;
    for (int y = 0; y < 192; y += 2) {
        odd += 4;
        for (int i = 0; i < 4; i++)
            *dst++ = *even++;
        for (int i = 0; i < 4; i++)
            *dst++ = *odd++;
        even += 4;
    }
}

void display_swap()
{
    _frame_phase ^= 1;
}

// double yeet?
void display_32x96(const uint8_t* rbuf, const uint8_t* gbuf, uint8_t c0, uint8_t c1)
{
    uint8_t* dst = display_backbuffer();
    fill_buf(dst,c0,c1,rbuf,gbuf);              // interleave, alternating colors
    fill_buf(dst+FIELD_SIZE,c1,c0,gbuf,rbuf);
    display_swap();
}

uint8_t* display_backbuffer()
{
    uint8_t* dst = _frame_buf;
    if ((_frame_phase & 1))
        dst += FRAME_SIZE;                      // other buffer
    return dst;
}

uint8_t* display_frontbuffer()
{
    auto* d = _frame_buf;
    if (!(_frame_phase & 1))
        d += FRAME_SIZE;
    return d;
}

void display_clear()
{
    uint8_t* d = display_backbuffer();
    memset(d,0,FRAME_SIZE);
    display_swap();
}

void poll_ir();

int _last_key = 0;
int get_key()
{
    poll_ir();
    int k = _last_key;
    _last_key = 0;
    return k;
}

void keydown(int c)
{
    _last_key = c;
}

int getchar_()
{
    int c;
    while (!(c = get_key())) {
        poll_ir();
        usleep(10000);
    }
    return c;
}

int get_signal(int s)
{
    return get_key() == -s;
}

int _keycode = 0;
int _pressed = 0;
int _joy_map = 0;

int joy_map()
{
    return _joy_map;
}

void joy_key(int scancode, int down, int mod)
{
    int m = 0;
    switch (scancode) {
        case 16:    // 'm'
            m = GENERIC_MENU;
            break;
        case 82:    // up
            m = GENERIC_UP;
            break;
        case 81:    // down
            m = GENERIC_DOWN;
            break;
        case 80:    // left
            m = GENERIC_LEFT;
            break;
        case 79:    // right
            m = GENERIC_RIGHT;
            break;
        case 224:   // left control key
            m = GENERIC_FIRE;
            break;  // FIRE
    }
    if (down)
        _joy_map |= m;
    else
        _joy_map &= ~m;
}


const char* _lower_keys = "\0\0\0\0abcdefghijklmnopqrstuvwxyz1234567890\r\e\b\t -=[]\\\\;'`,./";
const char* _upper_keys = "\0\0\0\0ABCDEFGHIGKLMNOPQRSTUVWXYZ!@#$%^&*()\r\e\b\t _+{}||:\"~<>?";
void gui_key(int keycode, int pressed, int mods)
{
    //printf("%d:%d:%d\n",keycode,pressed,mods);
    joy_key(keycode,pressed,mods);
    _keycode = keycode;
    _pressed = pressed;
    if (pressed) {
        if (keycode <= 56) {
            if (KEY_MOD_CTRL & mods) {
                keydown(-SIGINT);      // SIGINT
            } else if (KEY_MOD_SHIFT & mods) {
                keydown(_upper_keys[keycode]);
            } else {
                keydown(_lower_keys[keycode]);
            }
        }
    }
}
