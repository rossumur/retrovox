//
//  streamer.hpp
//
//  Created by Peter Barrett on 6/10/20.
//  Copyright © 2020 Peter Barrett. All rights reserved.
//

#ifndef streamer_hpp
#define streamer_hpp

enum {
  PDM_START = 1,
  PDM_END,
  VIDEO_PES,
  AUDIO_PES,
  PUSH_AUDIO,
  PUSH_VIDEO,
  VIDEO_READY_P,
  WAIT_BUFFER,
  REQUEST_BUFFER,
  RECEIVED_BUFFER,
};

#if 0
#define PLOG(_x) plog(_x)
#define PLOGV(_x,_v) plogv(_x,_v)
void plog(int x);
void plogv(int x,int v);
#else
#define PLOG(_x)
#define PLOGV(_x,_v)
#endif

enum {
    DECODER_RUN = 2,
    DECODER_PAUSED = 4,
    AUDIO_READY = 8,
    VIDEO_READY = 16,
    DNS_READY = 256
};

#include <string.h>
#include <string>
#include <vector>
#include <map>

void espflix_run(int standard);

// deal with access points
enum WiFiState {
    WIFI_NONE,
    SCANNING,
    SCAN_COMPLETE,
    CONNECTING,
    CONNECTED
};

const char* FS_PATH(const char* path);
const char* FS_PATH(const std::string& path);
const char* FS_URL(const char* path);

std::vector<std::string> matches(const std::string& prefix, std::vector<std::string>& all);
std::vector<std::string> prune(const std::string& prefix, std::vector<std::string>& all);
std::vector<std::string> subdir(const std::string& prefix);
std::vector<std::string> spiffs_dir();

std::map<std::string,int>& wifi_list();
void wifi_join(const char* ssid, const char* pwd);
std::string wifi_ssid();
void wifi_dump();
WiFiState wifi_state();
void wifi_scan();
void wifi_disconnect();
void beep();

void nv_write(const char* str, int64_t pts);
int64_t nv_read(const char* str);

uint32_t cpu_ticks();
uint64_t us();
uint64_t ms();
void quiet();

extern "C"
void* malloc32(int size, const char* name);

#ifdef ESP_PLATFORM

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

class Q
{
    QueueHandle_t _q;
public:
    Q();
    void push(const void* evt);
    const void* pop();
    bool empty();
    int waiting();
};

#else

#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>

class Q
{
    std::queue<const void*> queue;
    mutable std::mutex guard;
    std::condition_variable signal;
public:
    void push(const void* data);
    const void* pop();
    bool empty();
    int waiting();
};
typedef void(*TaskFunction_t)(void *);

#define SIMULATOR
#define IRAM_ATTR
#define taskYIELD()

#endif

#ifndef SIMULATOR
#include "esp_heap_caps.h"
#include <lwip/sockets.h>
#include <lwip/dns.h>
#include <lwip/dns.h>
#include <time.h>
#include <sys/time.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

void start_thread(TaskFunction_t tf, void* arg, int core = 0);
int  get_events();
void clear_events(int i);
void wait_events(int i);
void set_events(int i);
void set_events_isr(int i);
int  coreid();
int  stack();

void mem(const char* t);

class AddTicks {
    uint32_t& _dst;
    uint64_t _start;
public:
    AddTicks(uint32_t& dst) : _dst(dst),_start(cpu_ticks()) {}
    ~AddTicks() { _dst += cpu_ticks()-_start; }
};

class Buffer {
public:
    uint32_t len;
    uint8_t data[8*188];
};

#define GENERIC_OTHER   0x8000

#define GENERIC_FIRE_X  0x4000  // RETCON
#define GENERIC_FIRE_Y  0x2000
#define GENERIC_FIRE_Z  0x1000
#define GENERIC_FIRE_A  0x0800
#define GENERIC_FIRE_B  0x0400
#define GENERIC_FIRE_C  0x0200

#define GENERIC_RESET   0x0100     // ATARI FLASHBACK
#define GENERIC_START   0x0080
#define GENERIC_SELECT  0x0040
#define GENERIC_FIRE    0x0020
#define GENERIC_RIGHT   0x0010
#define GENERIC_LEFT    0x0008
#define GENERIC_DOWN    0x0004
#define GENERIC_UP      0x0002
#define GENERIC_MENU    0x0001

int get_hid_ir(uint8_t* hid);       // get a fake ir hid event

class Reader;
class Streamer
{
    int _socket = 0;
    FILE* _file = 0;
    Reader* _reader = 0;
    const uint8_t* _rom = 0;

    int _content_length = 0;
    int _chunked = 0;
    int _chunk_mark;
    std::string _chunk;

    uint32_t _mark = 0;
    uint32_t _offset;
    uint64_t _start_ms;
    std::vector<std::string> req_headers;
    std::map<std::string,std::string> resp_headers;
    ssize_t read_chunked(uint8_t *dst, int len);
    ssize_t recv(uint8_t* dst, int len);

public:
    ~Streamer();
    int     get(const char* url, uint32_t offset = 0, uint32_t len = 0);
    int     get_url(const char* url, std::vector<uint8_t>& v, uint32_t offset = 0, uint32_t len = 0);
    void    get_rom(const uint8_t* rom, int len);
    void    header(const char* h);
    const char* get_resp_header(const char* h);
    ssize_t read(uint8_t* dst, uint32_t len, uint32_t* offset = 0);
    void    close();
    int     content_length() { return _content_length; };
};

std::string escape(const std::string& s);

int get_url(const std::string& url, std::vector<uint8_t>& data, int offset = 0, int len = 0);
int put_url(const std::string& url, const std::vector<uint8_t>& v);

int put_url(const char* url, const uint8_t* v, int len);
int get_url(const char* url, uint8_t* v, int max_len);

int get_url_32(const char* url, uint32_t offset, uint32_t len, uint8_t** data32);
uint8_t read8(const uint8_t* d);
void write8(const uint8_t* d, uint8_t b);

std::vector<uint8_t> packz(const uint8_t* d, int len);
int packz(const char* src, const char* dst);

int connect_tcp(const char* host, int host_port);

class Player {
public:
    Player();
    virtual ~Player();
    virtual int open(const std::string& url) = 0;
    virtual int close() = 0;
    virtual int loop() = 0;
    static void loop_all();
};

int printf_nano(const char *fmt, ...);
//#define printf printf_nano

std::vector<std::string> get_ssids();
void join_ssid(const std::string& ssid, const std::string& pass);
void start_httpd();

std::vector<std::string> split(const std::string &text, const std::string& sep);
std::string to_lower(const std::string& s);
std::string to_hex(const std::string& binary);
const std::string trim(const std::string& pString, const std::string& ws = "\r\n ");
void replace(std::string& source, std::string const& find, std::string const& replace);

int get_file_size(const std::string& filename);
int remove_file(const std::string& filename);
int read_file(const char* path, char* dst, int maxlen);
int write_file(const char* path, const char* src, int len);

std::string get_file_contents(const std::string& filename, ssize_t offset, ssize_t len);
int set_file_contents(const std::string& filename, const std::string& data);
std::string get_ext(const std::string& s);

class Url {
public:
    Url();
    Url(const std::string& p,int default_port = 80);
    std::string protocol;
    std::string host;
    std::string path;
    std::string query;
    int port;
};

#include <set>
#include <mutex>
using namespace std;
int ps_pub(const string& k, const string& v);
int ps_sub(const string& k);
int ps_unsub(const string& k);

class PubSub
{
public:
    static mutex _m;
    static set<PubSub*> _set;
    static void on_(const string& k, const string& v)
    {
        lock_guard<mutex> lock(_m);
        for (auto p : _set)
            if (p->has_sub(k))
                p->on(k,v);
    }

    static bool sub_used(const string& k)
    {
        lock_guard<mutex> lock(_m);
        for (auto p : _set)
            if (p->has_sub(k))
                return true;
        return false;
    }

    std::map<string,string> _subs;
    std::map<string,string> _evts;
    PubSub()
    {
        lock_guard<mutex> lock(_m);
        _set.insert(this);
    }
    virtual ~PubSub()
    {
        while (!_subs.empty())
            unsub(_subs.begin()->first);
        lock_guard<mutex> lock(_m);
        _set.erase(this);
    }
    void pub(const string& k, const string& v)
    {
        ps_pub(k,v);
    }
    void sub(const string& k)
    {
        _subs[k] = k;
        ps_sub(k);
    }
    void unsub(const string& k)
    {
        _subs.erase(k);
        if (!sub_used(k))
            ps_unsub(k);
    }
    bool has_sub(const string& k)
    {
        return _subs.find(k) != _subs.end();
    }
    void on(const string& k, const string& v)
    {
        _evts[k] = v;
    }
    std::map<string,string> get()
    {
        lock_guard<mutex> lock(_m);
        std::map<string,string> m;
        m.swap(_evts);
        return m;
    }
};

// handles blobby access to 32 bit aligned mem on ESP32
class Blob {
public:
    const int BITS = 12;
    const int CHUNK = 1 << BITS;
    std::vector<uint8_t*> chunks;
    ~Blob()
    {
        for (int i = 0; i < chunks.size(); i++)
            delete chunks[i];
    }
    void trim(int offset)
    {
        int t = offset >> BITS;
        t = min(t,(int)chunks.size());
        for (int i = 0; i < t; i++)
        {
            delete chunks[i];
            chunks[i] = 0;
        }
    }
    void page(const uint8_t* data, int offset, int len)
    {
        len = (len+3) >> 2;
        uint32_t* src = (uint32_t*)data;
        uint32_t* dst = (uint32_t*)ptr(offset);
        while (len--)
            *dst++ = *src++;
    }
    uint8_t* ptr(int offset)
    {
        int i = offset >> BITS;
        if (chunks.size() < i+1)
            chunks.resize(i+1);
        if (!chunks[i])
            chunks[i] = (uint8_t*)malloc32(CHUNK,"blob");
        return chunks[i] + (offset & (CHUNK-1));
    }
    uint8_t get8(int offset)
    {
        return read8(ptr(offset));
    }
    void put8(int offset, uint8_t b)
    {
        write8(ptr(offset),b);
    }
    uint32_t be16(int b)
    {
        return (get8(b) << 8) | get8(b+1);
    }
    uint32_t be32(int b)
    {
        return (be16(b) << 16) | be16(b+2);
    }
};
int get_blob(const char* url, uint32_t offset, uint32_t len, Blob& blob);

#endif /* streamer_hpp */
