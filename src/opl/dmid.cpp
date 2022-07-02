

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

// from http://man.9front.org/1/dmid

#define RATE 44100

#include "genmidi.h"
#include "../streamer.h"

void    opl3init_(int);
void    opl3wr_(int, int);
void    opl3out_(int16_t*, int);    // 700ms at 44.1, 395ms at 22.05

void sysfatal(const char* e)
{
    printf("%s\n",e);
    exit(1);
}

typedef struct Inst Inst;
typedef struct Opl Opl;
typedef struct Chan Chan;
typedef struct Trk Trk;

enum{
    Ninst = 128 + 81-35+1,  // 175

    R_WSE = 0x01,
        Mwse = 1<<5,	/* wave selection enable */
    R_CTL = 0x20,
    R_SCA = 0x40,
        Mlvl = 63<<0,	/* total level */
        Mscl = 3<<6,	/* scaling level */
    R_STK = 0x60,
    R_SUS = 0x80,
    R_NUM = 0xa0,		/* f number lsb */
    R_OCT = 0xb0,
        Mmsb = 3<<0,	/* f number msb */
        Moct = 7<<2,
        Mkon = 1<<5,
    R_FED = 0xc0,
    R_WAV = 0xe0,
    R_OP3 = 0x105,
};

struct Inst {
    int fixed;
    int dbl;
    int fine;
    uint8_t n;
    uint8_t i[13];
    uint8_t i2[13];
    int16_t base[2];
};
Inst inst[Ninst];

struct Opl {
    Chan *c;
    int n;
    int midn;
    int blk;
    int v;
    int64_t t;
    uint8_t *i;
};
Opl opl[18], *ople = opl + 18;

const int port[] = {
    0x0, 0x1, 0x2, 0x8, 0x9, 0xa, 0x10, 0x11, 0x12,
    0x100, 0x101, 0x102, 0x108, 0x109, 0x10a, 0x110, 0x111, 0x112
};
const int sport[] = {
    0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8,
    0x100, 0x101, 0x102, 0x103, 0x104, 0x105, 0x106, 0x107, 0x108
};
const uint8_t ovol[] = {
    0, 32, 48, 58, 64, 70, 74, 77, 80, 83, 86, 88, 90, 92, 93, 95, 96,
    98, 99, 100, 102, 103, 104, 105, 106, 107, 108, 108, 109, 110, 111,
    112, 112, 113, 114, 114, 115, 116, 116, 117, 118, 118, 119, 119,
    120, 120, 121, 121, 122, 122, 123, 123, 124, 124, 124, 125, 125,
    126, 126, 126, 127, 127, 127, 128, 128
};

struct Chan {
    Inst *i;
    int v;
    int bend;
    int pan;
};
Chan chan[16];

struct Trk {
    Blob* blob;
    int start;
    int end;
    int mark;
    uint64_t t;
    int ev;
};
Trk *tr;

float freq[128];
int mfmt, ntrk, div_ = 1, tempo, stream;
uint64_t T;

uint8_t read8(const uint8_t* d);
uint8_t get8(Trk *x = 0)
{
    if (x->mark >= x->end) {
        printf("track overflow: %d\n",x->end-x->start);
    }
    return x->blob->get8(x->mark++);
}

uint16_t
get16(Trk *x = 0)
{
    uint16_t v;
    v = get8(x) << 8;
    return v | get8(x);
}

void putcmd(uint16_t r, uint8_t v, uint16_t dt);

void setinst(Opl *o, uint8_t *i)
{
    int p = sport[o - opl];
    putcmd(R_OCT+p, o->blk, 0);
    putcmd(R_FED+p, (i[6] & ~0x30) | o->c->pan, 0);
    p = port[o - opl];
    putcmd(R_CTL+p, i[0], 0);
    putcmd(R_STK+p, i[1], 0);
    putcmd(R_SUS+p, i[2], 0);
    putcmd(R_WAV+p, i[3] & 3, 0);
    putcmd(R_CTL+3+p, i[7], 0);
    putcmd(R_STK+3+p, i[8], 0);
    putcmd(R_SUS+3+p, i[9], 0);
    putcmd(R_WAV+3+p, i[10] & 3, 0);
    o->i = i;
}

void noteoff(Chan *c, int n, int v)
{
    Opl *o;
    for (o=opl; o<ople; o++) {
        if (o->c == c && o->midn == n) {
            putcmd(R_OCT+sport[o-opl], o->blk, 0);
            o->n = -1;
        }
    }
}

int tmn = 0;
Opl *getchannel(void)
{
    Opl *o, *p;
    p = opl;
    for (o=opl; o<ople; o++) {
        if (o->n < 0)
            return o;
        if (o->t < p->t)
            p = o;
    }
    printf("tmn %d\n",tmn++);   // too many notes
    return p;
}

void setoct(Opl *o)
{
    int n, b, f, d;
    double e;

    d = o->c->bend;
    d += o->i == o->c->i->i2 ? o->c->i->fine : 0;
    n = o->n + d / 0x1000 & 0x7f;
    e = freq[n] + (d % 0x1000) * (freq[n+1] - freq[n]) / 0x1000;
    if(o->c->i->fixed)
        e = (int)e;
    f = (e * (1 << 20)) / 49716;
    for(b=1; b<8; b++, f>>=1)
        if(f < 1024)
            break;
    o->blk = (b << 2 & Moct) | (f >> 8 & Mmsb);
    putcmd(R_NUM+sport[o-opl], f & 0xff, 0);
    putcmd(R_OCT+sport[o-opl], Mkon | o->blk, 0);
}

void setvol(Opl *o)
{
    int p, w, x;

    p = port[o - opl];
    w = o->v * o->c->v / 127;
    w = ovol[w * 64 / 127] * 63 / 128;
    x = 63 + (o->i[5] & Mlvl) * w / 63 - w;
    putcmd(R_SCA+p, (o->i[4] & Mscl) | x, 0);
    x = 63 + (o->i[12] & Mlvl) * w / 63 - w;
    putcmd(R_SCA+p+3, (o->i[11] & Mscl) | x, 0);
}

void putnote(Chan *c, int midn, int n, int v, int64_t t, uint8_t *i)
{
    Opl *o;
    o = getchannel();
    o->c = c;
    o->n = n;
    o->midn = midn;
    o->v = v;
    o->t = t;
    if(o->i != i)
        setinst(o, i);
    setvol(o);
    setoct(o);
}

void noteon(Chan *c, int n, int v, int64_t t)
{
    int x, m;

    m = n;
    if (c - chan == 9){
        /* asspull workaround for percussions above gm set */
        if(m == 85)
            m = 37;
        if(m == 82)
            m = 44;
        if(m < 35 || m > 81)
            return;
        c->i = inst + 128 + m - 35; // percussion
    }
    if (c->i->fixed)
        m = c->i->n;
    if (v == 0){
        noteoff(c, n, 0);
        return;
    }
    x = m + (c->i->fixed ? 0 : c->i->base[0]);
    while(x < 0)
        x += 12;
    while(x > 8*12-1)
        x -= 12;
    putnote(c, n, x & 0xff, v, t, c->i->i);
    if(c->i->dbl){
        x = m + (c->i->fixed ? 0 : c->i->base[1]);
        while(x < 0)
            x += 12;
        while(x > 95)
            x -= 12;
        putnote(c, n, x & 0xff, v, t, c->i->i2);
    }
}

void resetchan(Chan *c)
{
    Opl *o;
    for (o=opl; o<ople; o++) {
        if(o->c == c && o->n >= 0) {
            putcmd(R_FED+sport[o-opl], (o->i[6] & ~0x30) | c->pan, 0);
            setvol(o);
            setoct(o);
        }
    }
}

uint64_t tc(int n)
{
    return ((uint64_t)n * tempo * RATE / div_) / 1000000;
}

void skip(Trk *x, int n)
{
    while(n-- > 0)
        get8(x);
}

int getvar(Trk *x)
{
    int v, w;

    w = get8(x);
    v = w & 0x7f;
    while(w & 0x80){
        if(v & 0xff000000)
            sysfatal("invalid variable-length number");
        v <<= 7;
        w = get8(x);
        v |= w & 0x7f;
    }
    return v;
}

int peekvar(Trk *x)
{
    int p = x->mark;
    int v = getvar(x);
    x->mark = p;
    return v;
}

void samp(int64_t tt)
{
    int dt;
    static uint64_t t;
    dt = tt - t;
    t += dt;
    while(dt > 0) {
        putcmd(0, 0, dt > 0xffff ? 0xffff : dt);
        dt -= 0xffff;
    }
}

void text(int m, const uint8_t* t, int len)
{
    char buf[32] = {0};
    if (len > 31) len = 31;
    for (int i = 0; i < len; i++)
        buf[i] = read8(t++);
    printf("%02d:%s\n",m,buf);
}

// http://www.music.mcgill.ca/~ich/classes/mumt306/StandardMIDIfileformat.html

void expression(uint8_t m)
{
}

void midi_ev(Trk *x)
{
    int e, n, m;
    Chan *c;

    samp(x->t += tc(getvar(x)));
    e = get8(x);
    if ((e & 0x80) == 0) {
        x->mark--;
        e = x->ev;
        if((e & 0x80) == 0)
            sysfatal("invalid event");
    } else
        x->ev = e;
    c = chan + (e & 15);
    n = get8(x);
    switch (e >> 4){
        case 0x8: noteoff(c, n, get8(x)); break;
        case 0x9: noteon(c, n, get8(x), x->t); break;
        case 0xB:
            m = get8(x);
            switch(n){
                case 0x00: case 0x01: case 0x20: break;
                case 0x07: c->v = m; resetchan(c); break;
                case 0x0A:
                    c->pan = m < 32 ? 1<<4 : m > 96 ? 1<<5 : 3<<4;
                    resetchan(c);
                    break;
                case 0x0B: expression(m); break;
                default:
                    /*
                     unknown controller 91
                     unknown controller 93
                     unknown controller 123
                     */
                    printf("unknown controller %d\n", n);
                    ;
            }
            break;
        case 0xc:
            c->i = inst + n;
            break;
        case 0xe:
            n = get8(x) << 7 | n;
            c->bend = n - 0x4000 / 2;
            resetchan(c);
            break;
        case 0xf:
            if((e & 0xf) == 0){
                while(get8(x) != 0xf7)
                    ;
                return;
            }
            m = getvar(x);
            switch(n){
                case 0x01:  // text
                case 0x02:  // copyright
                case 0x03:  // trackname
                case 0x04:  // instrument name
                case 0x05:  // lyric
                case 0x06:  // marker
                case 0x07:  // cue point
                    //text(n,x->p,m);
                    skip(x,m);
                    break;
                case 0x2F: x->mark = x->end; return; // END OF TRACK
                case 0x51: tempo = get16(x) << 8; tempo |= get8(x); break;

                // case 0x20: FF 20 01 cc MIDI Channel Prefix
                // case 0x21: FF 21 01 cc PortPrefix
                // case 0x54: // FF 54 05 hr mn se fr ff SMPTE Offset
                // case 0x58: // FF 58 04 nn dd cc bb Time Signature
                // case 0x59: // FF 59 02 sf mi Key Signature
                default:
                    printf("meta %02X:%d\n",n,m);
                    skip(x, m);
            }
            break;
        case 0xa:
        case 0xd: get8(x); break;
        default: sysfatal("invalid event");
    }
}

void init_instruments(const uint8_t* b)
{
    for (Inst* i = inst; i<inst+Ninst; i++)
    {
        // flags(2),fine tuning,note number
        int n = b[0];
        i->fixed = n & 1<<0;
        i->dbl = n & 1<<2;
        i->fine = (b[2] - 128) * 64;
        i->n = b[3];
        b += 4;

        // Voice 0 (16 bytes)
        memcpy(i->i,b,13);
        i->base[0] = *(int16_t*)(b + 14);
        b += 16;

        // Voice 1 (16 bytes)
        memcpy(i->i2,b,13);
        i->base[1] = *(int16_t*)(b + 14);
        b += 16;
    }
}

/*
 void readinst(char *file)
 {
 uint8_t ibuf[36*175];
 uint8_t names[32*175];

 FILE* ib = fopen(file, "rb");
 fread(ibuf,1,8,ib);
 if(memcmp(ibuf, "#OPL_II#", 8) != 0)
 sysfatal("invalid patch file");
 fread(ibuf,1,sizeof(ibuf),ib);
 fread(names,1,sizeof(names),ib);
 fclose(ib);

 const uint8_t* b = ibuf;
 for(int k = 0; k < 175; k++)
 {
 for (int j = 0; j < 36; j++) {
 printf("0x%02X,",b[j]);
 if (j == 3 || j == 19)
 printf("\n");
 }
 printf("// %d %s\n",k,names + k*32);
 b += 36;
 }

 for(int k = 0; k < 175; k++)
 printf("\"%s\", // %d\n",names + k*32,k);
 }
 */

int load_midi(Blob* d, int len)
{
    tr = 0;
    int b = 0;
    if (d->be32(b) != 0x4d546864)
        return -1;
    if (d->be32(b+4) != 6)
        return -1;
    b += 8;
    mfmt = d->be16(b+0);
    ntrk = d->be16(b+2);
    b += 4;
    if(ntrk == 1)
        mfmt = 0;
    if (mfmt < 0 || mfmt > 1)
        return -1;

    div_ = d->be16(b+0);
    tr = (Trk*)malloc(ntrk * sizeof *tr);
    memset(tr,0,ntrk * sizeof *tr);
    b += 2;

    // point to all the tracks
    Trk *x;
    for (x=tr; x<tr+ntrk; x++)
    {
        if (d->be32(b) != 0x4d54726b)
            return -1;
        uint32_t n = d->be32(b+4);
        b += 8;
        x->blob = d;
        x->start = b;
        x->mark = x->start;
        b += n;
        x->end = x->start+n;
    }
    return 0;
}

void write_pcm(int16_t* pcm, int samples, int channels, int freq);

// 14318180Hz master clock / 288
// write to the hardware, read some samples

int16_t sb[1024];   // big stack TODO
void putcmd(uint16_t r, uint8_t v, uint16_t dt)
{
    opl3wr_(r, v);
    uint32_t n;

    static double _dt = 0;
    _dt += dt;
    while((n = _dt) > 0) {
        if (n > sizeof sb / 4)
            n = sizeof sb / 4;
        _dt -= n;
        opl3out_(sb,n);                         // # of stereo 16 samples
        write_pcm((int16_t*)sb,n,2,RATE);       // paly em
    }
}

int midi_init(Blob* data, int len)
{
    opl3init_(RATE);
    init_instruments(_hisymak_instruments);    // _instruments,_instruments_vanilla

    putcmd(R_WSE, Mwse, 0);
    putcmd(R_OP3, 1, 0);

    double f = pow(2, 1./12);
    for (int n=0; n<128; n++)
        freq[n] = 440 * pow(f, n - 69);

    for(auto c=chan; c<chan+16; c++) {
        c->v = 0x5a;
        c->bend = 0;
        c->pan = 3<<4;
        c->i = inst;
    }
    tmn = 0;

    for(auto o=opl; o<ople; o++)
        o->n = -1;
    tempo = 500000;

    return load_midi(data,len);
}

int midi_close()
{
    free(tr);
    tr = 0;
    return 0;
}

// http://www.mirsoft.info/gamemids.php
int midi_loop()
{
    Trk* minx = 0;
    uint32_t mint = 0;
    for (Trk *x=tr; x<tr+ntrk; x++) { // get the next event from list of tracks
        if (x->mark >= x->end)
            continue;
        uint32_t t = x->t + tc(peekvar(x));
        if (t < mint || minx == 0) {
            mint = t;
            minx = x;
        }
    }
    if (!minx) {
        free(tr);
        tr = 0;
        return 1;   // done
    }
    midi_ev(minx);  // process the event
    return 0;       // still playing
}
