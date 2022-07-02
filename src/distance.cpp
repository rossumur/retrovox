//
//  distance.cpp
//  RetroVox
//
//  Created by Peter Barrett on 12/7/21.
//  Copyright © 2021 Peter Barrett. All rights reserved.
//

#include <stdio.h>
#include <string>
#include <stdlib.h>
#include "utils.h"
#include "terminal.h"

void make_rgb(uint32_t* rgb, const uint8_t* red, const uint8_t* green, uint8_t red_color, uint8_t green_color);
void dither16rg(const uint16_t* src, uint8_t* red, uint8_t* green, int width, int height);
void dither16rg_noise(const uint16_t* src, uint8_t* red, uint8_t* green, int width, int height);

class LatticeState
{
public:
    int _delta = 17000;

    uint16_t buf[32*192];
    uint8_t green_bits[4*192];
    uint8_t red_bits[4*192];

    // returns R/G as 8 bit values in short
    int probe(short* o, int* d)
    {
        uint8_t stepshift = 7;
        uint8_t hit = 0;
        uint8_t lasthit = 0;
        uint8_t i;
        uint8_t it = 0;
        for (it = 0; it < 100; it++)
        {
            //    step
            for (i = 0; i < 3; i++)
            {
                short dd = d[i] >> stepshift;
                if (hit)
                    dd = -dd;
                o[i] += dd;
            }

            short test = 0x3000;        // at 6
            long blowup = 0x8C00 >> stepshift;

            // hit
            long ax = abs(o[0]);    // only need to be long because of integer type promotion
            long ay = abs(o[1]);
            long az = abs(o[2]);
            hit = 0;

            //    octohedron
            long t = ax+ay+az;
            if ((t - test*2) < blowup)
                hit = 1;

            //    bar
            t = abs(ax-az) + abs(ay-ax) + abs(az-ay);
            //        t = o[0]*o[0] + o[1]*o[1] + o[2]*o[2];    // ballz etc
            if (t - test < blowup)
            {
                hit = 2;
                if ((o[0] ^ o[1] ^ o[2]) & 0x8000)
                    hit = 3;
            }

            if (hit)
            {
                stepshift++;        // finer
                t = 8 - (it >> 3);
                if (stepshift >= t)
                {
                    int c = 31-it+stepshift;    // color map
                    if (c < 0)
                        c = 0;
                    else if (c > 31)
                        c = 31;
                    switch (hit) {
                        case 1: return c << 3;
                        case 2: return c << 11;
                        default:return (c<<3) | (c << 11);
                    }
                }
                it--;
            } else {
                if (lasthit == 0 && stepshift > 1)
                    stepshift--;    // coarser
            }
            lasthit = hit;
        }
        return 0;
    }

    void init()
    {
        _delta = 17000;
    }

    uint8_t rbuf[4*192];
    uint8_t gbuf[4*192];

    int draw(int keys)
    {
        _delta += 0x100;
        uint16_t* rg16 = &buf[0];
        int xx,yy;
        int d[3];

        int scale = 3;
        int dx = scale*156/2;
        int dy = scale*156*4;

        d[0] = -32767;    // x
        for (xx = 0; xx < 192; xx++)    // W/H backwards here
        {
            d[1] = -24576;    // y
            long x = d[0];
            short x2 = x*x >> 16;
            for (yy = 0; yy < 32; yy++)
            {
                long y = d[1];
                short y2 = y*y >> 16;
                d[2] = 21168 - x2 - y2;

                short o[3];
                o[0] = _delta+0;
                o[1] = _delta+45056;
                o[2] = _delta+24576;    // origin

                *rg16++ = probe(o,d);
                d[1] += dy;
            }
            d[0] += dx;
        }

        dither16rg(&buf[0], rbuf, gbuf, 32, 192);
        display_32x96(rbuf,gbuf,0x48,0xD6);
        return 0;
    }
};
LatticeState _ls;

int lattice_main(int argc, const char * argv[])
{
    display_mode(Playfield32x192);
    for (;;) {
        if (get_key())
            break;
        _ls.draw(0);
    }
    return 0;
}
