//
//  voxel.cpp
//  RetroVox
//
//  Created by Peter Barrett on 12/11/21.
//  Copyright © 2021 Peter Barrett. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "streamer.h"
#include "terminal.h"
#include "utils.h"
using namespace std;

uint16_t _lfsr = 0xACE1u;
inline uint16_t lfsr()
{
    int lsb = _lfsr & 1;
    _lfsr >>= 1;
    if (lsb == 1)
        _lfsr ^= 0xB400u;
    return _lfsr;
}

//
void dither8rg(const uint8_t* src, uint8_t* red, uint8_t* green, int width, int height)
{
    typedef struct {
        int16_t r;
        int16_t g;
    } ep;
    ep ebuf[4*(64+4)] = {0};
    uint8_t out_r = 0;
    uint8_t out_g = 0;
    int p;

    for (int y = 0; y < height; y++) {
        ep* e0 = ebuf + (y&3)*(64+4)+2;
        ep* e1 = ebuf + ((y+1)&3)*(64+4)+2;
        ep* e2 = ebuf + ((y+2)&3)*(64+4)+2; // error buffers

        for (int x = 0; x < width; x++) {
            uint8_t sp = *src++;
            out_r <<= 1;
            out_g <<= 1;

            // red channel
            p = e0[x].r + (sp & 0xF0);
            if (p >= 0x80) {
                out_r |= 1;
                p -= 0xFF;
            }
            p >>= 3;                // error to diffuse
            e0[x+1].r += p;
            e0[x+2].r += p;
            e1[x-1].r += p;
            e1[x].r += p;
            e2[x+1].r += p;
            e2[x].r += p;

            // green channel
            p = e0[x].g + ((sp << 4) & 0xF0);
            if (p >= 0x80) {
                out_g |= 1;
                p -= 0xFF;
            }
            p >>= 3;                // error to diffuse
            e0[x+1].g += p;
            e0[x+2].g += p;
            e1[x-1].g += p;
            e1[x].g += p;
            e2[x+1].g += p;
            e2[x].g += p;

            if ((x & 7) == 7) {    // width must be multiple of 8
                *red++ = out_r;
                *green++ = out_g;
            }
            e0[x].r = e0[x].g = 0;
        }
    }
}

//
void dither8rg_noise(const uint8_t* src, uint8_t* red, uint8_t* green, int width, int height)
{
    uint8_t out_r = 0;
    uint8_t out_g = 0;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint8_t sp = *src++;
            int r = (sp & 0xF0) + (lfsr() & 0xFF) - 0x7F;
            lfsr();
            int g = ((sp << 4) & 0xF0) + (lfsr() & 0xFF) - 0x7F;
            out_r <<= 1;
            out_g <<= 1;
            if (r >= 0x80)
                out_r |= 1;
            if (g >= 0x80)
                out_g |= 1;
            if ((x & 7) == 7) {    // width must be multiple of 8
                *red++ = out_r;
                *green++ = out_g;
            }
        }
    }
}

//
void dither16rg(const uint16_t* src, uint8_t* red, uint8_t* green, int width, int height)
{
    typedef struct {
        int16_t r;
        int16_t g;
    } ep;
    ep ebuf[4*(64+4)] = {0};    // init with noise? todo
    uint8_t out_r = 0;
    uint8_t out_g = 0;
    int p;

    for (int y = 0; y < height; y++) {
        ep* e0 = ebuf + (y&3)*(64+4)+2;
        ep* e1 = ebuf + ((y+1)&3)*(64+4)+2;
        ep* e2 = ebuf + ((y+2)&3)*(64+4)+2; // error buffers

        for (int x = 0; x < width; x++) {
            uint16_t sp = *src++;
            out_r <<= 1;
            out_g <<= 1;

            // red channel
            p = e0[x].r + (sp >> 8);
            if (p >= 0x80) {
                out_r |= 1;
                p -= 0xFF;
            }
            p >>= 3;                // error to diffuse
            e0[x+1].r += p;
            e0[x+2].r += p;
            e1[x-1].r += p;
            e1[x].r += p;
            e2[x+1].r += p;
            e2[x].r += p;

            // green channel
            p = e0[x].g + (sp & 0xFF);
            if (p >= 0x80) {
                out_g |= 1;
                p -= 0xFF;
            }
            p >>= 3;                // error to diffuse
            e0[x+1].g += p;
            e0[x+2].g += p;
            e1[x-1].g += p;
            e1[x].g += p;
            e2[x+1].g += p;
            e2[x].g += p;

            if ((x & 7) == 7) {    // width must be multiple of 8
                *red++ = out_r;
                *green++ = out_g;
            }
            e0[x].r = e0[x].g = 0;
        }
    }
}

void dither8mono(const uint8_t* src, uint8_t* red, uint8_t* green, int width, int height)
{
    const uint8_t _levels[] = {0x00,0x50,0xA0,0xF0};
    int16_t ebuf[4*(64+4)] = {0};
    uint8_t out_r = 0;
    uint8_t out_g = 0;
    int p;
    for (int y = 0; y < height; y++) {
        int16_t* e0 = ebuf + (y&3)*(64+4)+2;
        int16_t* e1 = ebuf + ((y+1)&3)*(64+4)+2;
        int16_t* e2 = ebuf + ((y+2)&3)*(64+4)+2; // error buffers

        for (int x = 0; x < width; x++) {
            p = e0[x] + *src++;
            if (p < 0) p = 0;
            if (p > 0xFF) p = 0xFF;
            int m = p >> 6;
            p -= _levels[m];
            out_r = (out_r << 1) | (m >> 1);
            out_g = (out_g << 1) | (m & 1);

            p >>= 3;                // error to diffuse
            e0[x+1] += p;
            e0[x+2] += p;
            e1[x-1] += p;
            e1[x] += p;
            e2[x+1] += p;
            e2[x] += p;
            if ((x & 7) == 7) {
                *red++ = out_r;
                *green++ = out_g;
            }
            e0[x] = 0;
        }
    }
}

void dither16rg_noise(const uint16_t* src, uint8_t* red, uint8_t* green, int width, int height)
{
    uint8_t out_r = 0;
    uint8_t out_g = 0;
    int p;
    for (int y = 0; y < height; y++) {
       for (int x = 0; x < width; x++) {
            out_r <<= 1;
            out_g <<= 1;

            // red channel
            uint16_t sp = *src++;
            p = (lfsr() & 0x7F) + (sp >> 8) - 0x3F;
            if (p >= 0x80)
                out_r |= 1;

            // green channel
            p = (lfsr() & 0x7F) + (sp & 0xFF) - 0x3F;
            if (p >= 0x80)
                out_g |= 1;

            if ((x & 7) == 7) {    // width must be multiple of 8
                *red++ = out_r;
                *green++ = out_g;
            }
        }
    }
}

void dither8mono_noise(const uint8_t* src, uint8_t* red, uint8_t* green, int width, int height)
{
    uint8_t out_r = 0;
    uint8_t out_g = 0;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int p = (lfsr() & 0x3F) + *src++ - 0xF;
            if (p > 0xFF) p = 0xFF;
            if (p < 0) p = 0;
            out_r = (out_r << 1) | (p >> 7);
            out_g = (out_g << 1) | ((p >> 6) & 1);
            if ((x & 7) == 7) {
                *red++ = out_r;
                *green++ = out_g;
            }
        }
    }
}

void dump(const uint8_t* r, int len, const char* name = "_rom");

//#define MONO_MAP
#define ROM_MAP 1
#ifdef ROM_MAP
#include "voxel_map.h"
#endif

class Phy {
public:
    int max_v;
    int val;
    int update(bool up, bool down)
    {
        if (up)
            val = min(val+2,max_v);
        else if (down)
            val = max(val-2,-max_v);
        else
            val = val < 0 ? -(-val*15/16) : val*15/16;
        return val;
    }
};

class Voxel {
#define SCREEN_WIDTH 32
#define SCREEN_HEIGHT 192
#ifdef MONO_MAP
    int mono = 1;
#else
    int mono = 0;
#endif
    int inited = 0;
    Streamer src;

    uint8_t buf[SCREEN_WIDTH*SCREEN_HEIGHT];
    uint8_t rbuf[4*SCREEN_HEIGHT];              // bit buffers for dither
    uint8_t gbuf[4*SCREEN_HEIGHT];              // will interleave to produce yeet buffers

    int angle = 0;
    int max_distance = 256;
    int horizon = 16;
    float camera_x = 64;
    float camera_y = 64;
    int camera_height = 64;
    vector<uint8_t> colors;
    vector<uint8_t> depth;

    Phy player_v = {32,0};
    Phy player_av = {32,0};

public:
    void init()
    {
#ifndef ROM_MAP
        string p = "file:///Users/peterbarrett/Downloads/";
        string cname = mono ? "C3m.vox" : "C3.vox";
        string dname = "D3.vox";
       // cname = "C1W.vox";
       // dname = "D1.vox";
        src.get_url((p+cname).c_str(),colors);
        src.get_url((p+dname).c_str(),depth);
        dump(&colors[0],colors.size(),"colors_");
        dump(&depth[0],depth.size(),"depth_");
#else
        colors.resize(256*256);
        depth.resize(128*128);
        memcpy(&colors[0],colors_,sizeof(colors_));
        memcpy(&depth[0],depth_,sizeof(depth_));
#endif
        inited++;
    }

    void line(int x, int y0, int y1, int color, int depth)
    {
        uint8_t* dst = buf + x+y0*SCREEN_WIDTH;
        //color = 256-depth*2;  // Thermal/2001 mode!
        while (y0++ < y1) {
            *dst = color;
            dst += SCREEN_WIDTH;
        }
    }

    void render()
    {
        uint8_t miny[SCREEN_WIDTH];
        memset(miny,SCREEN_HEIGHT,SCREEN_WIDTH);
        memset(buf,0,SCREEN_HEIGHT*SCREEN_WIDTH);

        float a = lfsr()&0x7;     // adds life to dither
        a = a + angle*2*3.1415926;
        float sinang = sin(a/1024);
        float cosang = cos(a/1024);

        const uint8_t* depth_ = &depth[0];
        const uint8_t* color_ = &colors[0];

        uint8_t cx = camera_x;
        uint8_t cy = camera_y;
        int ch = camera_height + depth_[((cy >> 1) << 7) | (cx >> 1)];

        // Draw from front to back
        float deltaz = 1.;
        for (float z = 1; z < max_distance; z += deltaz)
        {
            float cz = cosang * z;
            float sz = sinang * z;
            float plx = -cz - sz;
            float ply =  sz - cz;
            float prx =  cz - sz;
            float pry = -sz - cz;
            float dx = (prx - plx) / SCREEN_WIDTH;
            float dy = (pry - ply) / SCREEN_WIDTH;

            plx += camera_x;
            ply += camera_y;
            float invz = 24/z;  // v scale TODO

            for (int i = 0; i < SCREEN_WIDTH; i++)
            {
                int x = (int)plx & 0xFF;
                int y = (int)ply & 0xFF;
                int d = depth_[((y >> 1) << 7) | (x >> 1)];
                int h = (ch - d) * invz + horizon;
                if (h < 0)
                    h = 0;
                if (h < miny[i]) {
                    line(i,h,miny[i],color_[(y << 8) | x],d);
                    miny[i] = h;
                }
                plx += dx;
                ply += dy;
            }
            deltaz += 0.005;
        }
    }

    // colors alternate each line
    void display()
    {
        uint8_t c0,c1;
        if (mono) {
            c0 = 0x44;
            c1 = 0x3A;
            //dither8mono(&buf[0],rbuf,gbuf,32,192);
            dither8mono_noise(&buf[0],rbuf,gbuf,32,192);
        } else {
            c0 = 0x32;
            c1 = 0xC6;
            dither8rg(&buf[0],rbuf,gbuf,32,192);
            //dither8rg_noise(&buf[0],rbuf,gbuf,32,192);
        }
        display_32x96(rbuf,gbuf,c0,c1);
    }

    void move(int keys)
    {
        int v = player_v.update(keys & GENERIC_UP,keys & GENERIC_DOWN);
        int av = player_av.update(keys & GENERIC_LEFT,keys & GENERIC_RIGHT);
        angle += av/2;
        float a = angle*2*3.1415926/1024;
        camera_x -= sin(a)*v/32;
        camera_y -= cos(a)*v/32;
    }

    void draw(int keys)
    {
        if (!inited)
            init();
        move(keys);
        render();
        display();
    }
};

int mars_main(int argc, const char * argv[])
{
    Voxel* v = new Voxel();
    display_mode(Playfield32x192);
    audio_play(FS_URL("mod/kult.mod"));
    for (;;) {
        v->draw(joy_map());
        if (get_key())
            break;
    }
    delete v;
    audio_stop();
    return 0;
}
