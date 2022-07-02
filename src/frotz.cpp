//
//  frotz.cpp
//  RetroVox
//
//  Created by Peter Barrett on 12/28/21.
//  Copyright © 2021 Peter Barrett. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "streamer.h"
#include "utils.h"
#include "terminal.h"
#include <unistd.h>

using namespace std;

typedef unsigned char zchar;
typedef unsigned char zbyte;
typedef unsigned short zword;

#define u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t

#define LEFT_MARGIN 0
#define TOP_MARGIN 0
#define RIGHT_MARGIN 96
#define LINE_HEIGHT 5
#define VISIBLE_LINES 23

static
int DrawString(const char* c, int len, short dx, short dy, bool invert = false)
{
    while (len--)
        dx += draw_chr(*c++,dx,dy,invert,2);
    return dx;
}

//  stack is 2k
//  main mem is up to 256k
//  only a portion in RW
extern "C" {
    void frotzInit();
    void z_show_status();
    int interpret();
};

class ZContext {
public:
    Blob _data;
    zword _stack[1024];
    uint32_t pcp;                                           // program counter

    int _lines = 0;
    int _drawn = 0;

    uint8_t _cx = LEFT_MARGIN;
    uint8_t _cx0;
    uint8_t _wmark = 0;
    uint8_t _window = 0;
    char _wbuf[16];

    vector<string> text;
    string current_line;

    ~ZContext()
    {
    }
    
    int getchar()
    {
        int c;
        while (!(c=get_key()))
            usleep(1000);
        return c;
    }

    // load up a game file
    int load(const char* p)
    {
        int len = get_blob(FS_URL(p),0,0,_data);
        if (len <= 0)
            return -1;
        frotzInit();
        return 0;
    }

    int load_save(vector<uint8_t>& data, int sav)
    {
        FlushWord();
        current_line = sav ? "Save as:" : "Restore from:";
        char filename[256];
        console_read_input(sizeof(filename)-1,(zchar*)filename,0,0);
        newline();
        if (sav)
            return put_url(FS_URL(filename),data) ? 0 : 1;
        return get_url(FS_URL(filename),data)  ? 0 : 1;
    }

    int loop()
    {
        while (interpret() == 2)    // READ
            ;
        return 0;
    }

    void redraw()
    {
        for (int i = 0; i < VISIBLE_LINES; i++)
            draw_line(i);
    }

    void draw_str(const string& s, int line)
    {
        int y = (line+1)*LINE_HEIGHT;
        fill_rect(LEFT_MARGIN,y,RIGHT_MARGIN-LEFT_MARGIN,LINE_HEIGHT,0);
        DrawString(s.c_str(),(int)s.length(),LEFT_MARGIN,y);
    }

    void draw_line(int line)
    {
        string t = "";
        if (line <= text.size())
            t = line == text.size() ? current_line:text[line];
        draw_str(t,line);
    }

    void update()
    {
        FlushWord();
        z_show_status();
        redraw();   // needed?
    }

    void draw_input(const char* s, int len)
    {
        draw_str(current_line + string(s,len),_lines);
    }

    void more()
    {
        redraw();
        draw_str("[MORE]",_lines);
        getchar();
        _drawn = 0;
    }

    void newline()
    {
        _cx = LEFT_MARGIN;
        text.push_back(current_line);
        current_line.clear();
        if (text.size() >= VISIBLE_LINES) {
            if (_drawn >= VISIBLE_LINES-1)
                more();
            text.erase(text.begin());
        } else
            _lines++;
        _drawn++;
    }

    void FlushWord()
    {
        if (_wmark == 0)
            return;
        int w = measure_str(_wbuf,_wmark,2);
        if (_window == 0 && (w + _cx > RIGHT_MARGIN))
            newline();
        _cx += w;
        if (_window == 0) {
            current_line += string(_wbuf,_wmark);
        }
        else
        {
            int x = _cx-w;
            fill_rect(x,0,RIGHT_MARGIN-x,LINE_HEIGHT,1);    // drawing status bar
            DrawString(_wbuf,_wmark,x,0,1);
        }
        _wmark = 0;
    }

    void DrawChar(char c)
    {
        if (c == '\n')
        {
            FlushWord();
            newline();
            return;
        }
        _wbuf[_wmark++] = c;
        if (c == ' ' || _wmark == sizeof(_wbuf))
            FlushWord();
    }

    zword* GetStack(zword x)
    {
        return _stack + x;
    }

    void SetZByte(long a, zchar v)
    {
        _data.put8((int)a,v);
    }

    zchar GetZByte(long a)
    {
        return _data.get8((int)a);
    }

    // code?
    zbyte GetCByte()
    {
        return GetZByte(pcp++);
    }

    void SetPC(uint32_t a)
    {
        pcp = a;
    }

    long GetPC()
    {
        return pcp;
    }

    void set_status_x(int x)
    {
        FlushWord();
        _cx = x;
    }

    void restart_game()
    {
        text.clear();
        _drawn = _lines = 0;
    }

    void set_window(zword win)
    {
        FlushWord();
        _window = win;
        //printf("set_window(%d)\n",win);
        if (win == 7)
        {
            _cx0 = _cx;
            _cx = LEFT_MARGIN;
        } else {
            _cx = _cx0;
        }
    }

    #define ZC_NEW_STYLE 0x01
    #define ZC_NEW_FONT 0x02
    zchar _cflag = 0;
    void print_char (zchar c)
    {
        if (_cflag)
        {
            _cflag = 0;    // change mode
            return;
        }

        if (c == ZC_NEW_STYLE)
        {
            _cflag = 1;
        }
        else if (c == ZC_NEW_FONT)
        {
            _cflag = 2;
        }
        else
            DrawChar((char)c);
    }

    // todo borderzone....
    zchar console_read_input(int maxLen, zchar *buf, zword timeout, bool continued)
    {
        update();   // needed?
        int i = 0;
        while (i < maxLen-1) {
            int c = getchar();
            if (c == '\b' && i) {
                i--;
                draw_input((char*)buf,i);
                continue;
            }
            if (c == 0x0D)
                break;
            buf[i++] = c;
            draw_input((char*)buf,i);
        }
        buf[i] = 0;
        current_line += (char*)buf;
        _drawn = 0;
        newline();
        return 0x0D;
    }
};
ZContext* _zcontext = 0;

/*
extern "C"
u8* Scrollback(zword p, bool write)
{
    return _zcontext->Scrollback(p,write);
}
 */

const zchar zscii_to_latin1_t[] =
{
    0xe4, 0xf6, 0xfc, 0xc4, 0xd6, 0xdc, 0xdf, 0xab,
    0xbb, 0xeb, 0xef, 0xff, 0xcb, 0xcf, 0xe1, 0xe9,
    0xed, 0xf3, 0xfa, 0xfd, 0xc1, 0xc9, 0xcd, 0xd3,
    0xda, 0xdd, 0xe0, 0xe8, 0xec, 0xf2, 0xf9, 0xc0,
    0xc8, 0xcc, 0xd2, 0xd9, 0xe2, 0xea, 0xee, 0xf4,
    0xfb, 0xc2, 0xca, 0xce, 0xd4, 0xdb, 0xe5, 0xc5,
    0xf8, 0xd8, 0xe3, 0xf1, 0xf5, 0xc3, 0xd1, 0xd5,
    0xe6, 0xc6, 0xe7, 0xc7, 0xfe, 0xf0, 0xde, 0xd0,
    0xa3, 0x00, 0x00, 0xa1, 0xbf
};

extern "C"
zchar zscii_to_latin1(zbyte c)
{
    return zscii_to_latin1_t[c];
}

//=====================================================================
//=====================================================================

extern "C" {
void set_status_x(int x)
{
    _zcontext->set_status_x(x);
}

void set_window (zword win)
{
    _zcontext->set_window(win);
}

void flush_buffer()
{
}

void print_char(zchar c)
{
    _zcontext->print_char(c);
}

void new_line()
{
    print_char('\n');
}

void deadmeat()
{
    printf("######## deadmeat\n");
}

void erase_window (zword win)
{
}

void erase_screen(zword win)
{
}

void z_sound_effect()
{
}

zchar console_read_input(int maxLen, zchar *buf, zword timeout, bool continued)
{
    return _zcontext->console_read_input(maxLen,buf,timeout,continued);
}

zchar stream_read_input(int max, zchar *buf,
              zword timeout, zword routine,
              bool hot_keys,
              bool no_scripting )
{
    return console_read_input (max, buf, timeout, true);
}

void z_input_stream()
{
}

void z_output_stream()
{
}

zchar stream_read_key( zword timeout, zword routine, bool hot_keys )
{
    return _zcontext->getchar();
}

void     os_fatal (const char *){};
void     os_restart_game (int) {
    _zcontext->restart_game();
};
void     os_init_screen (void){};
void     os_reset_screen (void) {};
int      os_random_seed (void){return rand();};

zword* GetStack(zword x)
{
    return _zcontext->GetStack(x);
}

zword* GetStackW(zword x)
{
    return GetStack(x);
}

void SetZByte(long a, zchar v)
{
    _zcontext->SetZByte(a,v);
}

zchar GetZByte(long a)
{
    return _zcontext->GetZByte(a);
}

void SetZWord(long a, zword v)
{
    SetZByte(a,v >> 8);
    SetZByte(a+1,v);
}

zword GetZWord(long a)
{
    int h = GetZByte(a);
    u8 l = GetZByte(a+1);
    return (h << 8) | l;
}

// code?
zbyte GetCByte()
{
    return _zcontext->GetCByte();
}

zword GetCWord(long a)
{
    int h = GetCByte();
    u8 l = GetCByte();
    return (h << 8) | l;
}

void SetPC(uint32_t a)
{
    _zcontext->SetPC(a);
}

long GetPC()
{
    return _zcontext->GetPC();
}

//    Buffered to avoid thrashing?
class SaveGame
{
    int _mark = 0;
public:
    vector<uint8_t> _data;
    SaveGame()
    {
    }
    void Write(u16 w)
    {
        Write8(w);
        Write8(w >> 8);
    }
    void Write8(u8 b)
    {
        _data.push_back(b);
    }
    u16 Read()
    {
        u16 a = Read8();
        return a | (((u16)Read8()) << 8);
    }
    u8 Read8()
    {
        return _data[_mark++];
    }
};

extern zword sp;
extern zword fp;
extern zword h_checksum;
extern zword h_dynamic_size;
extern zword h_version;
void store (zword);
void branch (int);

void z_save()
{
    //ShowStatus("Saving...");

    zword success = 1;
    SaveGame game;

    game.Write(h_checksum);
    game.Write8(0);        // name string

    game.Write(_zcontext->pcp);    // PC
    game.Write(_zcontext->pcp >> 16);
    game.Write(sp);        // stack
    game.Write(fp);        // frame pointer

    //    Write stack
    for (u16 i = sp; i < 1024; i++)
        game.Write(*GetStack(i));

    //    Size of dynamic area
    game.Write(h_dynamic_size);
    for (u16 i = 0; i < h_dynamic_size; i++)
        game.Write8(GetZByte(i));

    success = _zcontext->load_save(game._data,1);

    if (h_version <= 3)
        branch (success);
    else
        store (success);
}

void z_restore (void)
{
    u8 success = 1;
    //ShowStatus("Restoring...");

    SaveGame game;
    if (_zcontext->load_save(game._data,0) == 1 && game.Read() == h_checksum)
    {
        u8 n;
        while ((n = game.Read8()) != 0)
            ;    // Read name

        long pc = game.Read();
        pc |= ((long)game.Read()) << 16;
        sp = game.Read();
        fp = game.Read();

        //    Read stack
        for (u16 i = sp; i < 1024; i++)
            *GetStackW(i) = game.Read();

        //    Size of dynamic area
        
        game.Read();    // must == h_dynamic_size
        for (u16 i = 0; i < h_dynamic_size; i++)
            SetZByte(i,game.Read8());

        SetPC(pc);
    } else
        success = 0;

    if (h_version <= 3)
        branch (success);
    else
        store (success);
}/* z_restore */

}   // extern "C"

int frotz_main(int argc, const char * argv[])
{
    //load_("sampler1.z5");
    //load_("sampler2.z3");
    //load_("brdrzone.z5");
    //const char* p = "frotz/minizork.z3";
    const char* p = "frotz/zork1.z5";
    if (argc > 1)
        p = argv[1];

    if (!_zcontext) {
        _zcontext = new ZContext();
        _zcontext->load(p);
    }
    _zcontext->loop();
    delete _zcontext;
    _zcontext = 0;
    putchar(033);
    putchar('c');
    return 0;
}
