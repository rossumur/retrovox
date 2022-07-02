//
//  badapple.cpp
//  RetroVox
//
//  Created by Peter Barrett on 1/13/22.
//  Copyright © 2022 Peter Barrett. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "streamer.h"
#include "terminal.h"
#include "utils.h"
#include "unistd.h"
using namespace std;


// bitmap of 8x8 blocks solid:color
int pack(uint8_t* src, uint8_t* dst)
{
    uint8_t* map = dst;
    uint8_t bits = 0;
    int n = 0;
    uint8_t* block = dst + 24;              // 24 bytes of bitmap

    for (int y = 0; y < 8; y++) {
        uint8_t* line = src + y*12*8;
        for (int x = 0; x < 12; x++) {
            for (int yy = 0; yy < 8; yy++)
                block[yy] = line[x+yy*12];  // fetch an 8x8 block
            bits <<= 2;
            int i = 1;
            uint8_t c = block[0];
            if (c == 0 || c == 0xFF) {
                for (; i < 8; i++) {
                    if (c != block[i])
                        break;
                }
            }
            if (i == 8)
                bits |= 2 | (c&1);          // flag solid and color
            else
                block += 8;
            if ((n & 3) == 3)
                *map++ = bits;
            n++;
        }
    }
    return (int)(block - dst);
}

void unpack(uint8_t* src, uint8_t* dst)
{
    uint8_t* map = src;
    src += 24;
    int n = 0;
    uint8_t bits = 0;
    for (int y = 0; y < 8; y++) {
        uint8_t* line = dst + y*12*8;
        for (int x = 0; x < 12; x++) {
            if (!(n & 3))
                bits = *map++;
            uint8_t shift = 3-(n&3);
            uint8_t b = (bits >> (shift << 1)) & 3;
            if (!b) {
                for (int yy = 0; yy < 8; yy++)
                   line[x+yy*12] = *src++;  // fetch an 8x8 block
            } else {
                b = (b & 1) ? 0xFF : 0x00;
                for (int yy = 0; yy < 8; yy++)
                   line[x+yy*12] = b;       // solid
            }
            n++;
        }
    }
}

// bill atkinson's packbits
int packbits_encode(uint8_t* src, int len, uint8_t* dst)
{
    int i = 0;
    uint8_t* s = dst;
    while (i < len)
    {
        uint8_t c = src[i];
        if (i == (len-1)) {
            *dst++ = 1;
            *dst++ = c;
            break;
        }

        if (c == src[i+1]) {
            // literal repeated
            int j = i + 1;
            int length = 2;
            while (++j < len && c == src[j] && length < 128)
                length++;
            *dst++ = 1-length;
            *dst++ = c;
            i = j;
        } else {
            // no literal repeated
            int j = i + 1;
            int length = 2;
            int prev = src[j];
            while (++j < len && prev != src[j] && length < 128) {
                length++;
                prev = src[j];
            }
            // rollback index if detect repeat
            if (prev == src[j]) {
                j--;
                length--;
            }
            *dst++ = length--;
            memcpy(dst,src+i,j-i);
            dst += j-i;
            i = j;
        }
    }
    return (int)(dst - s);
}

void loada()
{
    FILE* f = fopen("/Users/peterbarrett/Downloads/badapple.bin","rb");
    FILE* dst = fopen("/Users/peterbarrett/Downloads/badapplex.pak","wb");
    int frame = 0;
    int t = 0;
    uint8_t last[96/8*64] = {0};
    for (;;) {
        uint8_t fr[96/8*64];
        uint8_t p[96/8*64*2+24];
        if (fread(fr,sizeof(fr),1,f) <= 0)
            break;
#if 0
        uint8_t xord[96/8*64];
        for (int i = 0; i < 12*64; i++)
            xord[i] = fr[i] ^ last[i];
        memcpy(last,fr,12*64);
        int len = pack(xord,p+2);
#endif
        int len = pack(fr,p+2);
        p[0] = len;
        p[1] = len >> 8;
        len += 2;
        fwrite(p,1,len,dst);
        printf("%d\n",len);
        t += len;
        frame++;
    }
    printf("compressed %d frames, %d total\n",frame,t);
    fclose(f);
    fclose(dst);
}

class BadApple {
    Streamer _video;
public:
    BadApple()
    {
        start();
        display_clear();
    }

    ~BadApple()
    {
    }

    void start()
    {
        _video.get(FS_URL("ba/badapple.zip"));
    }

    void draw(int keys)
    {
        uint8_t p[12*64 + 24];          // max possible framesize
        uint16_t len = 0;

        uint8_t* fb = display_backbuffer();
        fb[0] = 0x0E;       // colors
        fb[1] = 0x00;
        fb += 4 + 12*12;    // center

        if (_video.read((uint8_t*)&len,2) != 2) {
            _video.close();
            start();
            return;
        }
        _video.read((uint8_t*)p,len);
        unpack(p,fb);
        display_swap();
    }
};

int apple_main(int argc, const char * argv[])
{
    BadApple* ba = new BadApple();
    display_mode(Graphics96x96);
    audio_play(FS_URL("ba/badapple.mid"));
    int64_t ft = -33333*40;
    for (;;) {
        while (ft > audio_elapsed_us())
           usleep(1000);
        ba->draw(0);
        if (get_key())
            break;
        ft += 33333;
    }
    delete ba;
    audio_stop();
    return 0;
}
