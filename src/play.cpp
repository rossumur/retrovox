//
//  play.cpp
//  RetroVox
//
//  Created by Peter Barrett on 11/14/21.
//  Copyright © 2021 Peter Barrett. All rights reserved.
//

#include "streamer.h"
#include "terminal.h"
#include "utils.h"
#include "unistd.h"
using namespace std;

//#define MINIMP3_ONLY_MP3
//#define MINIMP3_ONLY_SIMD
#define MINIMP3_NO_SIMD
//#define MINIMP3_NONSTANDARD_BUT_LOGICAL
//#define MINIMP3_FLOAT_OUTPUT
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

#define POCKETMOD_IMPLEMENTATION 1
#include "pocketmod.h"

void write_pcm(int16_t* pcm, int samples, int channels, int freq);

//==================================================================================================================
//==================================================================================================================
//  MP3
/*
 typedef struct {
 int frame_bytes;
 int channels;
 int hz;
 int layer;
 int bitrate_kbps;
 } mp3dec_frame_info_t;
*/

void audio_write_1(int16_t s);

class MP3Player : public Player
{
    mp3dec_t mp3d;
    Streamer src;
    int icy_metaint;
    short   pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];   // 1152*2*2 bytes

#define MAX_FRAME 2048
    uint8_t buf[MAX_FRAME];
    uint8_t frame[MAX_FRAME];
    int buf_size;
    int mark;
    int meta_mark;

public:
    MP3Player() : mark(0)
    {
        mp3dec_init(&mp3d);
    }

    void meta(const uint8_t* m, int len)
    {
        printf("%s%d\n",(const char*)m,len);
    }

    int read_meta(uint8_t* dst, int len)
    {
        if (icy_metaint == -1)
            return src.read(dst,len);

        len = min(len,icy_metaint - meta_mark);
        ssize_t i = src.read(dst,len);
        if (i <= 0)
            return -1;
        meta_mark += len;

        // read metadata
        if (meta_mark == icy_metaint) {
            uint8_t c;
            uint8_t* p = (uint8_t*)pcm;
            if (src.read(&c,1) != 1)
                return -1;
            if (c) {
                if (src.read(p,c*16) != c*16)
                    return -1;
                meta(p,c*16);
            }
            meta_mark = 0;
        }
        return len;
    }

    virtual int open(const string& url)
    {
        icy_metaint = -1;
        src.header("Icy-MetaData:1");
        int err = src.get(url.c_str());
        if (err < 0)
            return err;

        string s = src.get_resp_header("icy-metaint");
        if (!s.empty())
            icy_metaint = atoi(s.c_str());  // has icy metadata

        mark = 0;
        buf_size = 0;
        meta_mark = 0;

        read();
        mark = 0;
        mp3dec_frame_info_t info = {0};
        mp3dec_decode_frame(&mp3d, buf, buf_size, pcm, &info);
        mark = info.frame_offset;
        return 0;
    }

    virtual int close()
    {
        return 0;
    }

    int read()
    {
        if (mark == buf_size) {
            buf_size = read_meta(buf,(int)sizeof(buf));
            if (buf_size <= 0)
                return -1;
            mark = 0;
        }
        return buf[mark++];
    }

    virtual int loop()
    {
        int i = 0;
        for (i = 0; i < 4; i++)
        {
            int b = read();
            if (b == -1)
                return -1;
            frame[i] = b;
            //printf("%02X",b);
        }
        //printf("\n");
        int frame_size = mp3dec_frame_size(&mp3d, frame);
        for (; i < frame_size; i++)
        {
            int b = read();
            if (b == -1)
                return -1;
            frame[i] = b;
        }

        mp3dec_frame_info_t info;
        int samples = mp3dec_decode_frame(&mp3d, frame, frame_size, pcm, &info);
        //printf("%m %d %d %d %d\n",frame_size,mark,samples,info.frame_bytes);
        if (samples)
            write_pcm(pcm,samples,info.channels,info.hz);
        return 0;
    }
};

//==================================================================================================================
//==================================================================================================================
//  MODs

class MODPlayer : public Player {
    pocketmod_context ctx;
    vector<uint8_t> data;
    int valid = 0;
    float buffer[1024]; // 512 samples, this does not need to be floats
    int16_t pcm[1024];
public:
    virtual int open(const string& url)
    {
        Streamer src;
        if (src.get_url(url.c_str(),data))
            return -1;
        valid = pocketmod_init(&ctx,&data[0],(int)data.size(),44100);
        return valid ? 0 : -1;
    }

    virtual int close()
    {
        return 0;
    }
    
    virtual int loop()
    {
        if (!valid)
            return -1;
        int n = sizeof(buffer)/8;
        pocketmod_render(&ctx,buffer,n*8);
        for (int i = 0; i < n*2; i++)
            pcm[i] = buffer[i]*(1<<16);
        write_pcm(pcm,n,2,44100);
/*
        for (int i = 0; i < n; i++) {
            int16_t s = (buffer[i*2] + buffer[i*2+1])*(1<<15);
            audio_write_1(s);
        }
 */
        return pocketmod_loop_count(&ctx);
    }
};

//==================================================================================================================
//==================================================================================================================

int midi_init(Blob* d, int len);
int midi_close();
int midi_loop();

class MIDIPlayer : public Player {
    Blob blob;
    int _len;
public:
    ~MIDIPlayer()
    {
    }

    virtual int open(const string& url)
    {
        _len = get_blob(url.c_str(),0,0,blob);
        if (_len <= 0)
            return -1;
        return midi_init(&blob,_len);
    }

    virtual int close()
    {
        return midi_close();
    }

    virtual int loop()
    {
        return midi_loop();
    }
};

void silence()
{
    int16_t pcm[16*2] = {0};
    for (int i = 0; i < 512; i++)
        write_pcm(pcm,16,2,44100);
}

//==================================================================================================================
//==================================================================================================================
//  AudioPlayer

string lower(string s)
{
    for (int i = 0; i < s.length(); i++)
        s[i] = tolower(s[i]);
    return s;
}

class AudioPlayer {
    class Msg {
    public:
        int msg = 0;
        vector<string> params;
    };
    Q _msg_q;
    int64_t _start_us = 0;

public:
    int state = 0;
    int loop()
    {
        for (;;) {
            auto* p = (Msg*)_msg_q.pop();
            state = 1;
            switch (p->msg) {
                case 0: play_(p->params[0]); break;
                case 1: printf("AudioPlayer stop"); break;
            }
            state = 0;
            delete p;
        }
    }

    Player* mk_player(const string& ext)
    {
        if (ext == "mod")
            return new MODPlayer();
        if (ext == "mp3")
            return new MP3Player();
        if (ext == "mid" || ext == "midi")
            return new MIDIPlayer();
        return 0;
    }

    string guess_ext(const string& url)
    {
        if (get_ext(url) == "zip")
            return get_ext(url.substr(0,url.length()-4));

        // read some data to figure out what it is
        Streamer s;
        uint8_t data[4];
        int err = s.get(url.c_str());
        if (err < 0)
            return "";
        string ct = s.get_resp_header("content-type");
        if (ct == "audio/mpeg")
            return "mp3";
        if (ct == "audio/midi")
            return "mid";
        if (s.read(data,4) <= 0)
            return "";
        if (data[0] == 'M' && data[1] == 'T')
            return "mid";
        if (data[0] == 0x73 && data[1] == 0x75)
            return "mod";
        return "";
    }

    string get_m3u(const string& url)
    {
        vector<uint8_t> data;
        if (get_url(url,data) < 0)
            return url;
        auto list = split(string((const char*)&data[0],data.size()),"\n");
        return trim(list[0]);
    }

    Player* new_player(const string& url)
    {
        string ext = get_ext(url);
        Player* p = mk_player(ext);
        if (p)
            return p;
        return mk_player(guess_ext(url));
    }

    int64_t elapsed_us()
    {
        if (_start_us == 0)
            return 0;
        return us() - _start_us;
    }

    void play_(const string& u)
    {
        _start_us = 0;
        string url = u;
        if (get_ext(u) == "m3u")
            url = get_m3u(url);

        Player* player = new_player(url);
        if (player && player->open(url) == 0) {
            while (player->loop() == 0) {
                if (_start_us == 0)
                    _start_us = us();
                if (!_msg_q.empty())
                    break;
            }
        }
        silence();
        delete player;
    }

    void play(const string& url)
    {
        Msg* m = new Msg();
        m->params.push_back(url);
        _msg_q.push(m);
    }

    void stop()
    {
        Msg* m = new Msg();
        m->msg = 1; // stop
        _msg_q.push(m);
    }

    bool playing()
    {
        return state || !_msg_q.empty();
    }
};
AudioPlayer _audio_player;

void audio_init_hw();
void audio_loop(void*)
{
    audio_init_hw();
    _audio_player.loop();
}

void start_audio_play()
{
    start_thread(audio_loop,0); // use core 1 for audio
}

/*
 string url = "http://us5.internet-radio.com:8166/stream";
    url = "http://stream.live.vc.bbcmedia.co.uk/bbc_world_service?s=1636823867&e=1636838267&h=7b70eddaee75c617e1ffa0bb40a5aa8b";
    url = "http://stream.live.vc.bbcmedia.co.uk/bbc_radio_one";
 */

// http://kcrw.streamguys1.com/kcrw_192k_mp3_on_air_internet_radio
// http://us4.internet-radio.com:8258/
int audio_play(const string& p)
{
    _audio_player.play(p);
    return 0;
}

int audio_stop()
{
    _audio_player.stop();
    return 0;
}

int64_t audio_elapsed_us()
{
    return _audio_player.elapsed_us();
}

bool audio_playing()
{
    return _audio_player.playing();
}

//==================================================================================================================
//==================================================================================================================

void retro_phones(const char* s);

int play_main(int argc, const char * argv[])
{
#if 0
    retro_phones("FF EH EH PA1 BB2 RR2 UW2 XR IY");
    retro_phones("SS SS IH PA3 KK2 SS PA2 PA3 TT2 IY NN1");
    retro_phones("FF EH EH PA1 BB2 RR2 UW2 XR IY");
    retro_phones("SS SS IH PA3 KK2 SS PA2 PA3 TT2 IY NN1");
    retro_phones("FF EH EH PA1 BB2 RR2 UW2 XR IY");
    retro_phones("SS SS IH PA3 KK2 SS PA2 PA3 TT2 IY NN1");
    retro_phones("FF EH EH PA1 BB2 RR2 UW2 XR IY");
    retro_phones("SS SS IH PA3 KK2 SS PA2 PA3 TT2 IY NN1");
     return 0;
#endif
    if (argc < 2)
        return -1;

    audio_play(FS_URL(argv[1]));
    while (audio_playing()) {
        char c = get_key();
        if (c)
            break;
        usleep(33333);
    }
    audio_stop();
    return 0;
}
