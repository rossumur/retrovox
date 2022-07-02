//
//  life.cpp
//  RetroVox
//
//  Created by Peter Barrett on 1/20/22.
//  Copyright © 2022 Peter Barrett. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "streamer.h"
#include "terminal.h"

class Life {
public:
    uint8_t w,h;
    uint8_t* src;
    uint8_t* dst;
    uint8_t* buf;
    uint8_t phase = 0;

    Life(uint8_t w = 96, uint8_t h = 96) : w(w),h(h)
    {
        buf = new uint8_t[(w>>3)*h*2];
    }

    ~Life()
    {
        delete [] buf;
    }

    void setp(int x, int y, int v)
    {
        if (x < 0) x += w;
        if (x >= w) x -= w;
        if (y < 0) y += h;
        if (y >= h) y -= h;
        y = y*(w >> 3) + (x >> 3);
        if (v)
            dst[y] |= 0x80 >> (x & 7);
        else
            dst[y] &= ~(0x80 >> (x & 7));
    }

    void draw()
    {
        uint8_t* d = display_backbuffer();
        d[0] = 0x0E;
        d[1] = 0xC0;
        memcpy(d+4,dst,12*96);
        display_swap();
        //usleep(33*1000);
    }

    void ptrs()
    {
        int siz = (w >> 3)*h;
        src = dst = buf;
        if (phase++ & 1)
            dst += siz;
        else
            src += siz;
    }

    void mktab()
    {
        uint8_t tab[8*8] = {0};
        for (int m0 = 0; m0 < 8; m0++) {
            for (int m1 = 0; m1 < 8; m1++) {
                for (int m2 = 0; m2 < 8; m2++) {
                    int b = (m0 << 6) | (m1 << 3) | m2;
                    int n = 0;
                    for (int x = 0; x < 9; x++) {
                        if (b & (0x100 >> x))
                            if (x != 4)
                                n++;
                    }
                    if ((b & 0x10) ? (n == 2 || n == 3) : (n == 3))
                        tab[(m0 << 3) | m1] |= 1 << m2;
                }
            }
        }
        for (int i = 0; i < 64; i++) {
            printf("0x%02X,",tab[i]);
            if ((i & 0xF) == 0xF)
                printf("\n");
        }
    }

    const uint8_t _tab[64] = {
        0x80,0x68,0xE8,0x7E,0x68,0x16,0x7E,0x17,0x68,0x16,0x7E,0x17,0x16,0x01,0x17,0x01,
        0x68,0x16,0x7E,0x17,0x16,0x01,0x17,0x01,0x16,0x01,0x17,0x01,0x01,0x00,0x01,0x00,
        0x68,0x16,0x7E,0x17,0x16,0x01,0x17,0x01,0x16,0x01,0x17,0x01,0x01,0x00,0x01,0x00,
        0x16,0x01,0x17,0x01,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x00,0x00,0x00,0x00,
    };

    int evolve()
    {
        ptrs();
        uint8_t b = 0;
        uint8_t* d = dst;
#define W8 12
        for (int y = 0; y < h; y++) {
            const uint8_t* src1 = src + y*W8;
            const uint8_t* src0 = y ? src1 - W8 : src + (h-1)*W8;
            const uint8_t* src2 = y == (h-1) ? src : src1 + W8;
            int p0 = (src0[11] << 8) | src0[0]; // bits at 7,8,9
            int p1 = (src1[11] << 8) | src1[0];
            int p2 = (src2[11] << 8) | src2[0];

            int s = 6;
            int k = 1;
            int x = W8;
            while (x--) {
                int n = 8;
                while (n--) {
                    int m0 = (p0 >> s) & 0x7;
                    int m1 = (p1 >> s) & 0x7;
                    int m2 = (p2 >> s) & 0x7;
                    s--;
                    if (s < 0) {
                        p0 = (p0 << 8) | src0[k]; // bits at 7,8,9
                        p1 = (p1 << 8) | src1[k];
                        p2 = (p2 << 8) | src2[k];
                        s = 7;
                        if (++k == W8)  // w>>3
                            k = 0;
                    }
                    b = (b <<1) | ((_tab[(m0 << 3) | m1] >> m2) & 1);
                }
                *d++ = b;
            }
        }
/*
        for (int y = 0; y < h; y++) {
            src1 = src + y*12;
            src0 = y ? src1 - 12 : src + (h-1)*12;
            src2 = y == (h-1) ? src : src1 + 12;
            for (int x = 0; x < w; x++) {
                int n = nh(x,y);
                int p = getp(x,y);
                setp(x,y,p ? (n == 2 || n == 3) : (n == 3));
            }
        }
 */
        draw();
        return 0;
    }

    void rnd()
    {
        dst = buf;
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++)
                setp(x,y,rand()&1);
        }
        draw();
    }

    void clr()
    {
        memset(buf,0,(w>>3)*h*2);
    }

    /*
     20b2o$20bobo$22bo4b2o$18b4ob2o2bo2bo$18bo2bobobobob2o$21bobobobo$22b2o
     bobo$26bo2$12b2o$13bo7b2o$13bobo5b2o$14b2o25bo$39b3o$38bo$38b2o3$46b2o
     $24b2o21bo$24bo22bob2o$14b3o8b3o11b2o4b3o2bo$4bo11bo10bo11b2o3bo3b2o$
     2b5o8bo5b2o21b4o$bo5bo13bo8b2o15bo$bo2b3o12bobo7bobo12b3o$2obo15b2o8bo
     13bo$o2b4o21b2o14b5o$b2o3bo3b2o11bo22bo2bo$3b3o4b2o11b3o22b2o$3bo22bo$
     2obo21b2o$2ob2o3$11b2o$12bo$9b3o$9bo25b2o$28b2o5bobo$28b2o7bo$37b2o2$
     24bo$23bobob2o4b2o$23bobobobo2bo2bo$22b2obobobo3b2o$23bo2b2ob4o$23bo4b
     o3bo$24b3obo2bo$26bobobo$29bo!
     */

    int load(const char* path)
    {
        char str[256];
        FILE* f = fopen(FS_PATH(path), "r");
        if (!f)
            return -1;

        clr();
        ptrs();
        int x = 0;
        int y = 0;
        int xx = 0;
        int yy = 0;
        while (fgets(str, sizeof(str)-1, f)) {
            if (str[0] == '!' || str[0] == '#')
                continue;

            if (str[0] == 'x') {
                for (auto p : split(str,",")) {
                    auto nv = split(p,"=");
                    if (nv.size() == 2) {
                        auto n = nv[0];
                        auto v = atoi(nv[1].c_str());
                        switch (n[0]) {
                            case 'x': xx = v; break;
                            case 'y': yy = v; break;
                        }
                    }
                }
                continue;
            }

            if (xx) {
                // rle format
                const char* s = str;
                while (*s && !(*s == '\n' || *s == '\r')) {
                    int n = 0;
                    while (*s >= '0' && *s <= '9')
                        n = n*10 + *s++-'0';
                    if (!n)
                        n++;
                    switch (*s) {
                        case '!':
                        case '\r':
                        case '\n':  break;
                        case '$':   y += n; x = 0; break;
                        case 'b':   x += n; break;
                        case 'o':
                            while (n--)
                                setp((w-xx)/2+x++,(h-yy)/2+y,1);    // centered
                            break;
                    }
                    s++;
                }
            } else {
                // cell format
                x = 0;
                while (str[x]) {
                    setp(x,y,str[x] == 'O');
                    x++;
                }
                y++;
            }
        }
        fclose(f);
        draw();
        return 0;
    }
};

Life* _life = 0;

int life_main(int argc, const char * argv[])
{

    display_mode(Graphics96x96);
    _life = new Life();
    const char* path = "life/_life.rle";
    if (argc > 1)
        path = argv[1];
    _life->load(path);
    audio_play(FS_URL("mod/ballada.mod"));

    auto all = subdir("life/");
    int generations = 0;
    int i = 1;
    while (_life->evolve() == 0) {
        if (get_key())
            break;
        if (generations++ == 1024) {
            _life->load(("life/" + all[i++ % all.size()]).c_str()); // cycle thru greatest hits
            generations = 0;
        }
    }

    delete _life;
    _life = 0;
    return 0;
}
