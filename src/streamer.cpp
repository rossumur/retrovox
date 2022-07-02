//
//  streamer.cpp
//
//  Created by Peter Barrett on 6/10/20.
//  Copyright © 2020 Peter Barrett. All rights reserved.
//

#include "stdlib.h"
#include "stdio.h"
#include "unistd.h"
#include "string.h"
#include "stdarg.h"

#include <vector>
#include <string>
#include <mutex>
using namespace std;

#include "streamer.h"

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

#include <sys/socket.h>
#include <sys/stat.h>
#include "miniz.h"

#ifdef SIMULATOR
const char* FS_ROOT = "./demo/";
#else
const char* FS_ROOT = "/demo/";
#endif

const char* FS_URL(const char* path)   // not thread safe
{
    if (strncasecmp(path,"http://",7) == 0)
        return path;
    if (strncasecmp(path,"file://",7) == 0)
        return path;

    static string p;
    #ifdef SIMULATOR
    p = string("file://demo/") + path;
    #else
    p = string("file:///demo/") + path;
    #endif
    return p.c_str();
}

const char* FS_PATH(const char* path)   // not thread safe
{
    static string p;
    p = string(FS_ROOT) + path;
    return p.c_str();
}
const char* FS_PATH(const string& path)
{
    return FS_PATH(path.c_str());
}

#ifdef SIMULATOR
// compress wo header to create .z files
vector<uint8_t> packz(const uint8_t* d, int len)
{
    vector<uint8_t> v;
    v.resize(len*2);
    mz_ulong dlen = v.size();
    compress(&v[0],&dlen,d,len);
    v.resize(dlen);
    return v;
}

int packz(const char* src, const char* dst)
{
    vector<uint8_t> data;
    if (get_url(FS_URL(src),data))
        return -1;
    data = packz(&data[0],(int)data.size());
    return put_url(FS_URL(dst),data);
}

#endif

#include "dirent.h"
vector<string> dir(const string& path)
{
    vector<string> v;
    DIR* dirp = opendir(path.c_str());
    if (dirp) {
        struct dirent * dp;
        while ((dp = readdir(dirp)) != NULL) {
            string name = dp->d_name;
            if (dp->d_type == DT_DIR)
                name += "/";
            if (name[0] != '.')
                v.push_back(name);
        }
        closedir(dirp);
    }
    return v;
}

void childs(const string& p, vector<string>& all)
{
    vector<string> d = dir(p);
    string pp = p == "." ? "" : p;
    for (auto f : d) {
        if (f[f.length()-1] == '/')
            childs(pp+f,all);
        else
            all.push_back(p+f);
    }
}

vector<string> matches(const string& prefix, vector<string>& all)
{
    int n = (int)prefix.length();
    vector<string> r;
    for (auto& s : all) {
        if (s.compare(0, n, prefix) == 0)
            r.push_back(s);
    }
    return r;
}

vector<string> prune(const string& prefix, vector<string>& all)
{
    auto a = matches(prefix,all);
    int n = (int)prefix.length();
    for (auto& s : a)
        s = s.substr(n);
    return a;
}

// get a dir that looks like SPIFFS
vector<string> spiffs_dir()
{
    vector<string> all;
#ifdef SIMULATOR
    childs(".",all);            // get all the files recursively
#else
    all = dir("/");
#endif
    return prune("demo/",all);  // remove prefix
}

std::vector<std::string> subdir(const std::string& prefix)
{
    auto d = spiffs_dir();
    return prune(prefix,d);
}

// Teeny printf that behaves itself in a FreeRTOS multithread enviroment.

#define PUT(_c) putchar(_c)
const char* _chr = "0123456789ABCDEF";
mutex _printf_guard;

IRAM_ATTR
int printf_nano(const char *fmt, ...)
{
    unique_lock<mutex> lock(_printf_guard);
    //PUT('!');

    int r,i,width,pad;
    uint32_t uv;
    const char* s;
    char buf[16];

    va_list ap;
    va_start (ap, fmt);

    while (*fmt) {
        if (*fmt != '%')
            PUT(*fmt++);
        else {
            ++fmt;
            char c = *fmt++;
            r = i = 16;

            // get widths
            width = 0;
            pad = ' ';
            while (c >= '0' && c <= '9') {
                if (c == '0') {
                    pad = c;
                    c = *fmt++;
                }
                if (c >= '1' && c <= '9') {
                    width = c - '0';
                    c = *fmt++;
                }
            }

            // do format
            switch (c) {
                case 's':
                    s = va_arg(ap,const char *);
                    while (*s) {
                        if (width) width--;
                        PUT(*s++);
                    }
                    while (width--)
                        PUT(' ');
                    break;
                case 'd':
                    r = 10;
                case 'X':
                case 'x':
                case 'c':
                    uv = va_arg(ap, int);
                    if (c == 'c')
                        PUT(uv);
                    else {
                        if (c == 'd' && ((int)uv < 0)) {
                            PUT('-');
                            uv = -(int)uv;
                        }
                    }
                    do {
                        buf[--i] = _chr[uv % r];
                        uv /= r;
                    } while (uv);
                    while (16-i < width)
                        buf[--i] = pad;
                    while (i < 16)
                        PUT(buf[i++]);
                    break;
                default:
                    PUT(c);
                    break;
            }
        }
    }
    return 0;
}

#ifdef ESP_PLATFORM

extern "C" void* malloc32(int x, const char* label)
{
   // printf("malloc32 %d free, %d biggest, allocating %s:%d\n",
   //        heap_caps_get_free_size(MALLOC_CAP_32BIT),heap_caps_get_largest_free_block(MALLOC_CAP_32BIT),label,x);
    void * r = heap_caps_malloc(x,MALLOC_CAP_32BIT);
    if (!r) {
        printf("malloc32 FAILED allocation of %s:%d!!!!####################\n",label,x);
        esp_restart();
    }
    else
        printf("malloc32 allocation of %s:%d %08X\n",label,x,r);
    return r;
}

void dns_cb(const char *name, const ip_addr_t *ipaddr, void *callback_arg)
{
    *((ip_addr_t *)callback_arg) = *ipaddr;
    set_events(DNS_READY);
}

err_t gethostbyname(const char *hostname, ip_addr_t *addr)
{
    //printf("gethostbyname %s\n", hostname);
    clear_events(DNS_READY);
    if (dns_gethostbyname(hostname,addr,dns_cb,addr) == 0)
        return 0;
    wait_events(DNS_READY);
    //printf("gethostbyname %s:%s\n", hostname, ip4addr_ntoa(&addr->u_addr.ip4));
    return 0;
}

namespace std {
string to_string(int n)
{
    char buf[16];
    return itoa(n,buf,10);
}
}

int stack()
{
    return uxTaskGetStackHighWaterMark(NULL);
}

int coreid()
{
    return xPortGetCoreID();
}

Q::Q()
{
    _q = xQueueCreate(32,sizeof(void*));
}

void Q::push(const void* data)
{
    xQueueSend(_q,&data,portMAX_DELAY);
}

const void* Q::pop()
{
    void* data;
    xQueueReceive(_q,&data,portMAX_DELAY);
    return data;
}

bool Q::empty()
{
    return waiting() == 0;
}

int Q::waiting()
{
    return uxQueueMessagesWaiting(_q);
}

EventGroupHandle_t _event_group;
int get_events()
{
    return xEventGroupGetBits(_event_group);
}

void clear_events(int i)
{
    xEventGroupClearBits(_event_group, i);
}

void wait_events(int i)
{
    xEventGroupWaitBits(_event_group, i, false, true, portMAX_DELAY);
}

void set_events(int i)
{
    xEventGroupSetBits(_event_group,i);
}

IRAM_ATTR
void set_events_isr(int i)
{
    BaseType_t xHigherPriorityTaskWoken, xResult;
    xHigherPriorityTaskWoken = pdFALSE;
    xResult = xEventGroupSetBitsFromISR(_event_group,   /* The event group being updated. */
                                        i, /* The bits being set. */
                                        &xHigherPriorityTaskWoken );
    if(xHigherPriorityTaskWoken)
        portYIELD_FROM_ISR();
}

void start_thread(TaskFunction_t tf, void* arg, int core)
{
    xTaskCreatePinnedToCore(tf, "THREADNAME", 8*1024, arg, 5, NULL, core);
}

IRAM_ATTR
uint32_t cpu_ticks()
{
    return xthal_get_ccount();
}

IRAM_ATTR
uint64_t us()
{
    return esp_timer_get_time();
}

IRAM_ATTR
uint64_t ms()
{
    return esp_timer_get_time()/1000;
}

#else

#include <sys/time.h>
#include <time.h>

uint32_t cpu_ticks()
{
    unsigned int lo,hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}

double get_wall_time(){
    struct timeval time;
    if (gettimeofday(&time,NULL)){
        //  Handle error
        return 0;
    }
    return (double)time.tv_sec + (double)time.tv_usec * .000001;
}

uint64_t us()
{
    struct timeval time;
    gettimeofday(&time,NULL);
    return (uint64_t)time.tv_sec*1000000 + time.tv_usec;
}

uint64_t ms()
{
    return us()/1000;
}

typedef struct {
    uint32_t addr;
} ip4_addr_t;

typedef struct {
    union {
        //ip6_addr_t ip6;
        ip4_addr_t ip4;
    } u_addr;
    uint8_t type;
} ip_addr_t;

const char* ipaddr_ntoa(ip_addr_t* a)
{
    static char buf[16];
    uint32_t n = a->u_addr.ip4.addr;
    sprintf(buf,"%d.%d.%d.%d",(n >> 0)&0xFF,(n>>8)&0xFF,(n>>16)&0xFF,(n>>24)&0xFF);
    return buf;
}

int gethostbyname(const char *hostname, ip_addr_t *addr)
{
    struct hostent *hp = gethostbyname(hostname);
    if (!hp)
        return -1;
    auto* ha = (struct in_addr *)hp->h_addr_list[0];
    addr->u_addr.ip4.addr = (uint32_t)ha->s_addr;
    return 0;
}

std::mutex _event_guard;
std::condition_variable _event_signal;

int _event_group = 0;

int get_events()
{
    return _event_group;
}

void clear_events(int i)
{
    //std::thread::id this_id = std::this_thread::get_id();
    //printf("clear_events t:%d m:%04X\n",this_id,i);
    _event_group &= ~i;
    _event_signal.notify_all();
}

void wait_events(int i)
{
    //std::thread::id this_id = std::this_thread::get_id();
    //printf("wait_events t:%d m:%04X\n",this_id,i);

    std::unique_lock<std::mutex> lock(_event_guard);
    while (!(_event_group & i))
        _event_signal.wait(lock);
}

void set_events(int i)
{
    //std::thread::id this_id = std::this_thread::get_id();
    //printf("set_events t:%d m:%04X\n",this_id,i);

    _event_group |= i;
    _event_signal.notify_all();
    _event_signal.notify_all();
}

void set_events_isr(int i)
{
    set_events(i);
}

void Q::push(const void* data)
{
    {
        std::lock_guard<std::mutex> lock(guard);
        queue.push(data);
    }
    signal.notify_one();
}

const void* Q::pop()
{
    std::unique_lock<std::mutex> lock(guard);
    while (queue.empty())
        signal.wait(lock);
    const void* value = queue.front();
    queue.pop();
    return value;
}

bool Q::empty()
{
    std::unique_lock<std::mutex> lock(guard);
    return queue.empty();
}

int Q::waiting()
{
    return (int)queue.size();
}

int stack()
{
    return -1;
}

int coreid()
{
    return -1;
}

void start_thread(TaskFunction_t tf, void* arg, int core)
{
    new std::thread(tf,arg);
}

extern "C" void* malloc32(int x, const char* label)
{
    printf("malloc32 %s %d\n",label,x);
    return malloc(x);
}

void mem(const char* t)
{
    printf("mem:%s\n",t);
}

vector<string> get_ssids()
{
    vector<string> t = {"foobar","Stations WIFI","Presto-chango","Noisy WIFI"};
    return t;
}

string wifi_ssid()
{
    return "Stations WIFI";
}

void join_ssid(const string& ssid, const string& pass)
{
    printf("joining %s %s\n",ssid.c_str(),pass.c_str());
}

#endif

//========================================================================================
//========================================================================================
// Streamer.
// Read from files, memory or http urls

vector<string> split(const string &text, const string& sep)
{
    vector<string> tokens;
    ssize_t start = 0, end = 0;
    while ((end = text.find(sep, start)) != string::npos) {
        tokens.push_back(trim(text.substr(start, end - start)));
        start = end + sep.length();
    }
    if (start < text.length())
        tokens.push_back(trim(text.substr(start)));
    return tokens;
}

string to_lower(const string& s)
{
    string ls;
    for (char c : s)
        ls += ::tolower(c);
    return ls;
}

string to_hex(const string& binary)
{
    string a;
    char s[3] = {0};
    for (ssize_t i = 0; i < binary.length(); i++) {
        sprintf(s,"%02x",(unsigned char)binary[i]);
        a += s;
    }
    return a;
}

const string trim(const string& pString, const string& ws)
{
    const size_t beginStr = pString.find_first_not_of(ws);
    if (beginStr == std::string::npos)
        return "";
    const size_t endStr = pString.find_last_not_of(ws);
    const size_t range = endStr - beginStr + 1;
    return pString.substr(beginStr, range);
}

void replace(string& source, string const& find, string const& replace)
{
    for (string::size_type i = 0; (i = source.find(find, i)) != string::npos;)
    {
        source.replace(i, find.length(), replace);
        i += replace.length();
    }
}

string read_line(int s)
{
    char line[256];
    char c = 0;
    for (int i = 0; (i < sizeof(line)-1); i++) {
        if (::recv(s,&c,1,0) != 1)
            return "";
        if (c == '\n') {
            line[i-1] = 0;
            break;
        }
        line[i] = c;
    }
    return line;
}

int read_headers(int s, vector<string>& r)
{
    for (int h = 0; ; h++) {
        string line = read_line(s);
        if (line.empty())
            return 0;
        r.push_back(line);
    }
}

int http_headers(int s, map<string,string>& headers, vector<string>& status)
{
    vector<string> hdr;
    if (read_headers(s,hdr) || hdr.size() == 0)
        return -1;

    //printf("%s\n",hdr[0].c_str());
    vector<string> http;
    status = split(hdr[0]," ");
    for (ssize_t i = 1; i < hdr.size(); i++)
    {
        string str = hdr[i];
        size_t j = str.find(':');
        if (j == -1)
            return -1;
        string name = to_lower(trim(str.substr(0,j)));
        headers[name] = trim(str.substr(j+1));
        //printf("%s:%s\n",name.c_str(),headers[name].c_str());
    }
    return atoi(status[1].c_str());
}

string get_ext(const string& s)
{
    string ext;
    auto i = s.find_last_of(".");
    if (i > 0)
        return to_lower(s.substr(i+1));
    return ext;
}

class Reader {
protected:
    int _content_length = -1;
public:
    virtual ~Reader() {} ;
    virtual int open(const char* path) = 0;
    virtual int read(uint8_t* dst, int len) = 0;
    int content_length()
    {
        return _content_length;
    }
};

class ZipReader : public Reader {
public:
    mz_zip_archive src_archive;
    mz_zip_archive_file_stat file_stat;
    mz_zip_reader_extract_iter_state* iter = 0;

    virtual int open(const char* path)
    {
        memset(&src_archive, 0, sizeof(src_archive));
        if (!mz_zip_reader_init_file(&src_archive,path,0))
            return -1;
        if (mz_zip_reader_file_stat(&src_archive, 0, &file_stat) == 0)
            return -1;
        _content_length = (int)file_stat.m_uncomp_size;
        iter = mz_zip_reader_extract_file_iter_new(&src_archive, file_stat.m_filename, 0);
        return 0;
    }

    ~ZipReader()
    {
        if (iter)
            mz_zip_reader_extract_iter_free(iter);
        mz_zip_reader_end(&src_archive);
    }

    virtual int read(uint8_t* dst, int len)
    {
        return (int)mz_zip_reader_extract_iter_read(iter, dst, len);
    }
};

class ZReader {
    #define IN_BUF_SIZE (1536)
    #define OUT_BUF_SIZE (1024*32)  // this could be smaller if we created thee file

    const uint8_t *next_in = s_inbuf;
    uint8_t* next_out = s_outbuf;
    size_t avail_in = 0;
    size_t avail_out = OUT_BUF_SIZE;
    uint8_t s_outbuf[OUT_BUF_SIZE];
    uint8_t s_inbuf[IN_BUF_SIZE];

    FILE* _file;
    int _remaining;
    uint8_t* _mark;
    uint8_t* _end;
    tinfl_decompressor _dec;
public:
    ZReader(const char* path) : _remaining(-1),_mark(0),_end(0)
    {
        tinfl_init(&_dec);
        _file = fopen(path,"rb");
        if (_file) {
            fseek(_file, 0, SEEK_END);
            _remaining = (int)ftell(_file);
            fseek(_file, 0, SEEK_SET);
        }
    }

    ~ZReader()
    {
        if (_file)
            fclose(_file);
    }

    int remaining()
    {
        return _remaining;
    }

    int more()
    {
        for (;;)
        {
            if (!avail_in)
            {
                int n = min(IN_BUF_SIZE, _remaining);
                if (fread(s_inbuf, 1, n, _file) != n)
                    return -1;
                next_in = s_inbuf;
                avail_in = n;
                _remaining -= n;
            }

            size_t in_bytes = avail_in;
            size_t out_bytes = avail_out;
            tinfl_status status = tinfl_decompress(&_dec, next_in, &in_bytes, s_outbuf, next_out, &out_bytes,
            (_remaining ? TINFL_FLAG_HAS_MORE_INPUT : 0) | TINFL_FLAG_PARSE_ZLIB_HEADER);

            avail_in -= in_bytes;
            next_in = next_in + in_bytes;
            avail_out -= out_bytes;
            next_out = next_out + out_bytes;

            if ((status <= TINFL_STATUS_DONE) || (!avail_out))
            {
                int n = (int)(OUT_BUF_SIZE - avail_out);
                next_out = s_outbuf;
                avail_out = OUT_BUF_SIZE;
                _mark = s_outbuf;
                _end = _mark + n;
                return n;
            }
        }
    }

    int read(uint8_t* dst, int len)
    {
        int n = 0;
        while (n < len) {
            if (_mark == _end) {
                if (!_remaining)
                    return 0;   // done!
                if (more() <= 0)
                    return n;
            }
            int i = min((int)(_end - _mark),len-n);
            memcpy(dst+n,_mark,i);
            _mark += i;
            n += i;
        }
        if (n != len)
            printf("read fail\n");
        return n;
    }
};

Streamer::~Streamer()
{
    close();
}

int Streamer::get(const char* url, uint32_t offset, uint32_t len)
{
    //printf("get %s %d %d\n",url,offset,len);
    _start_ms = ms()-1;
    _content_length = -1;
    _chunked = 0;
    _chunk_mark = 0;
    _mark = 0;
    _offset = offset;
    close();

    if (strncmp(url,"file",4) == 0)
    {
        string u = string(url+7);
        string ext = get_ext(u);
        /*
        if (ext == "z")
        {
            auto z = new ZReader(u.c_str());    // 44.3k
            if ((_content_length = z->remaining()) == -1) { // TOOO - this is wrong
                delete z;
                return -1;
            }
            if (len)
                _content_length = min(_content_length,(int)len);
            _reader = z;
            return 200;
        }
         */

        if (ext == "zip")
        {
            auto zip = new ZipReader();
            if (zip->open(u.c_str()) < 0) {
                delete zip;
                return -1;
            }
            _content_length = zip->content_length();
            _reader = zip;
            return 200;
        }

        _file = fopen(u.c_str(),"rb");
        if (_file) {
            fseek(_file, 0, SEEK_END);
            _content_length = (int)ftell(_file) - offset;
            if (len)
                _content_length = min((int)len,_content_length);
            fseek(_file, offset, SEEK_SET);
        }
        return _file ? 200 : -1;
    }

    char host[100] = {0};
    char path[100] = {0};
    int host_port = 80;
    int n = sscanf(url, "http://%99[^:]:%99d/%99[^\n]",host, &host_port, path);
    if (n != 3) {
        n = sscanf(url, "http://%99[^:]:%99d/",host, &host_port);
        if (n != 2)
            n = sscanf(url, "http://%99[^/]/%99[^\n]",host, path);
    }

    _socket = connect_tcp(host,host_port);
    if (_socket == -1)
        return -1;

    // send an http request
    string req = "GET /" + string(path) + " HTTP/1.1\r\n";
    req += "Host: " + string(host) + ":" + to_string(host_port) + "\r\n";
    if (offset != 0 || len != 0) {
        req += "Range: bytes=" + to_string(offset) + "-";
        if (len)
            req += to_string(offset+len-1);
        req += "\r\n";
    }
    for (string h : req_headers)
        req += h + "\r\n";
    req += "User-Agent: retrovox\r\n\r\n";
    send(_socket,req.c_str(),req.length(),0);
    //printf("%s",req.c_str());

    // Content-Range: bytes 0-1023/146515

    // read response/headers
    vector<string> hdrs;
    if (read_headers(_socket,hdrs))
        return -1;

    string r = *hdrs.begin();
    hdrs.erase(hdrs.begin());
    int status = atoi(r.c_str()+9);

    for (string s : hdrs) {
        vector<string> hdr = split(s,":");
        if (hdr.size() != 2)
            continue;
        auto n = trim(to_lower(hdr[0]));
        auto v = trim(to_lower(hdr[1]));
        resp_headers[n] = v;
        if (n == "content-length")
            _content_length = atoi(v.c_str());
        if (n == "transfer-encoding" && v == "chunked")
            _chunked = 1;
    }
    return status;
}

int Streamer::get_url(const char* url, vector<uint8_t>& v, uint32_t offset, uint32_t len)
{
    if ((get(url,offset,len) != 200) || (_content_length == -1)) {
        close();
        return -1;
    }
    v.resize(_content_length);
    ssize_t n = read(&v[0],_content_length);
    close();
    return n >= 0 ? 0 : -1;
}

void Streamer::get_rom(const uint8_t* rom, int len)
{
    _rom = rom;
    _content_length = len;
    _mark = _offset = 0;
    _start_ms = ms();
}

void Streamer::header(const char* h)
{
    req_headers.push_back(h);
}

const char* Streamer::get_resp_header(const char* h)
{
    return resp_headers[h].c_str();
}

ssize_t Streamer::recv(uint8_t* dst, int len)
{
    int i = 0;
    while (i < len) {
        ssize_t n = ::recv(_socket,dst+i,len-i,0);
        if (n <= 0)
            return -1;
        i += n;
    }
    return len;
}

ssize_t Streamer::read_chunked(uint8_t* dst, int len)
{
    int i = 0;
    while (i < len) {
        if (_chunk.size() == _chunk_mark) {
            string length = read_line(_socket);
            size_t c = strtol(length.c_str(),0,16);
            _chunk.resize(c);
            if (c != recv((uint8_t*)&_chunk[0],c))
                return -1;
            read_line(_socket);
            _chunk_mark = 0;
        }
        int n = min(len-i,(int)(_chunk.size()-_chunk_mark));
        memcpy(dst+i,&_chunk[_chunk_mark],n);
        _chunk_mark += n;
        i += n;
    }
    return len;
}

ssize_t Streamer::read(uint8_t* dst, uint32_t len, uint32_t* offset)
{
    //printf("%dkbits/s\n",_mark*8/(int)(ms()-_start_ms));
    if (offset)
        *offset = _offset + _mark;
    len = min((uint32_t)(_content_length - _mark),len);
    if (_rom) {
        memcpy(dst,_rom,len);
        _rom += len;
        _mark += len;
        return len;
    }
    if (_reader) {
        _mark += len;
        return _reader->read(dst,len);
    }
    if (_file) {
        _mark += len;
        return fread(dst,1,len,_file);
    }
    if (_socket <= 0)
        return -1;
    if (_chunked)
        return read_chunked(dst,len);

    int i = 0;
    while (i < len) {
        ssize_t n = min((uint32_t)(_content_length - _mark),len-i);
        if (n == 0)
            break;
        n = recv(dst,n);
        //printf("recv:%d\n",n);
        if (n <= 0)
            break;
        _mark += n;
        dst += n;
        i += n;
    }
    return i;
}

void Streamer::close()
{
    if (_socket)
        ::close(_socket);
    if (_reader)
        delete _reader;
    if (_file)
        fclose(_file);
    _rom = 0;
    _socket = 0;
    _reader = 0;
    _file = 0;
    _mark = 0;
}

string local_path(const string& url)
{
    string s;
    if (strncmp(url.c_str(),"file://",7) == 0)
        s += url.c_str() + 7;
    return s;
}

// really, really bad on SPIFFS
int get_file_size(const string& filename)
{
    struct stat stat_buf;
    int rc = stat(filename.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}

int read_file(const char* path, char* dst, int maxlen)
{
    FILE* f = fopen(path,"rb");
    if (!f)
        return -1;
    int n = (int)fread(dst,1,maxlen,f);
    fclose(f);
    return n;
}

int write_file(const char* path, const char* src, int len)
{
    FILE* f = fopen(path,"wb");
    if (!f)
        return -1;
    int n = (int)fwrite(src,1,len,f);
    fclose(f);
    return n;
}

int remove_file(const string& filename)
{
    return remove(filename.c_str());
}

int get_url(const string& url, vector<uint8_t>& data, int offset, int len)
{
    Streamer s;
    return s.get_url(url.c_str(),data,offset,len);
}

int get_url(const char* url, uint8_t* v, int max_len)
{
    vector<uint8_t> d;
    int len = get_url(url,d);
    if (len <= 0 || len > max_len)
        return -1;
    memcpy(v,&d[0],len);
    return len;
}

int put_url(const char* url, const uint8_t* v, int len)
{
    printf("put_url %d bytes to %s\n",len,url);
    string u = local_path(url);
    if (u.empty())
        return -1;
    FILE* f = fopen(u.c_str(),"w");
    if (!f)
        printf("put_url %s failed to create file\n",u.c_str());
    int i = fwrite(v,1,len,f);
    if (i != len)
        printf("put_url %s failed %d vs %d\n",u.c_str(),i,len);
    fclose(f);
    return 0;
}

int put_url(const string& url, const vector<uint8_t>& v)
{
    return put_url(url.c_str(),&v[0],(int)v.size());
}

void cpy32(uint32_t* dst, uint32_t* src, int len)
{
    while (len--)
        *dst++ = *src++;
}

// read into 32 bit aligned memory for the ESP32
int get_url_32(const char* url, uint32_t offset, uint32_t len, uint8_t** data32)
{
    *data32 = 0;
    Streamer s;
    if ((s.get(url,offset,len) != 200) || (s.content_length() == -1))
        return -1;

    int n = (s.content_length()+3) >> 2;
    uint32_t* b = (uint32_t*)malloc32(n*4,url);
    uint32_t buf[64];
    int i = 0;
    while (i < n) {
        int m = min(64,n-i);
        s.read((uint8_t*)buf,m*4);
        cpy32(b+i,buf,m);
        i += m;
    }
    *data32 = (uint8_t*)b;
    return s.content_length();
}

int get_blob(const char* url, uint32_t offset, uint32_t len, Blob& blob)
{
    Streamer s;
    if ((s.get(url,offset,len) != 200) || (s.content_length() == -1))
        return -1;

    int n = s.content_length();
    uint8_t buf[256];
    int i = 0;
    while (i < n) {
        int m = min((int)sizeof(buf),n-i);
        s.read(buf,m);
        blob.page(buf,i,m);
        i += m;
    }
    return n;
}

//  use these for reading, writing 32 bit aligned memory on ESP32
uint8_t read8(const uint8_t* d)
{
    uintptr_t p = (uintptr_t)d;
    int32_t a = *((int32_t*)(p & ~3));
    return a >> ((p & 3) << 3);  // aligned
}

void write8(const uint8_t* d, uint8_t b)
{
    uintptr_t p = (uintptr_t)d;
    int32_t* a = ((int32_t*)(p & ~3));
    p = (p & 3) << 3;
    *a = (*a & (~(0xFF << p))) | (b << p);
}

int connect_tcp(const char* host, int host_port)
{
    ip_addr_t host_ip;
    if (gethostbyname(host, &host_ip) == -1)   // blocking
        return -1;

    int s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    //printf("connecting to %s:%d %s : %08X\n",host,host_port,ipaddr_ntoa(&host_ip),host_ip.u_addr.ip4);
    addr.sin_addr.s_addr = host_ip.u_addr.ip4.addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(host_port);
    int err = ::connect(s, (struct sockaddr*)&addr, sizeof(addr));
   // printf("connect: %d\n",err);
    if (err)
        return -1;
    return s;
}

//========================================================================================
//========================================================================================

map<Player*,int> _players;
Player::Player()
{
    _players[this] = 1;
}

Player::~Player()
{
    _players.erase(this);
}

void Player::loop_all()
{
    for (auto p : _players)
        p.first->loop();
}

//========================================================================================
//========================================================================================
//  http server for config

const char* form_0 = R""""(
<!DOCTYPE html>
<html>
<title>retrovox</title>
<style>
body {font-family:Helvetica; font-size:16pt; padding:16px;margin:8px;}
h1   {color: #FFF;}
.fr {font-size:16pt;width:90%;}
#sid {background:#4A7;color:#FFF;width:512px;padding:16px;margin:8px;}
pre {width:512px;padding:16px;margin:8px;background:#EEE;}
</style>
<body>

<div id='sid'>
<h1>retrovox</h1>
Select Network SSID<br>
<select class='fr' name="s"></select>
<br>
<br>
Password<br>
<input id="pass" class='fr' type="password" name="p"></input>
<br>
<br>
<input type="button" value="Join" onclick="api_join()">
</div>

<pre id="info"></pre>
<pre id="status"></pre>
<script>
var _q = function(q) {
    return document.querySelector(q)
};
function status(d)
{
    _q("#status").innerText = d;
}
function info(d)
{
    _q("#info").innerText = d;
}
function escapeHTML(unsafeText) {
    let div = document.createElement('div');
    div.innerText = unsafeText;
    return div.innerHTML;
}

// connection to retrovox device
var api = new WebSocket("ws://" + location.host + "/api");
function api_q()
{
    api.send(JSON.stringify({cmd:"q"}));
}

var ap_list;
function api_join()
{
    if (!ap_list) return;
    var ssid = ap_list[parseInt(_q("select").value)];
    var pass = _q("#pass").value;
    api.send(JSON.stringify({cmd:"join",ssid:ssid,pass:pass}));
}
api.onmessage = function (m)
{
    var o = JSON.parse(m.data);
    // show access points
    if (o.ap_list) {
        var h = '';
        if (o.ap_list.length == 0) {
            status("scanning for access points...");
            setTimeout(api_q,1000);
        } else {
            ap_list = o.ap_list;
            for (var i = 0; i < o.ap_list.length; i++) {
                var s = o.ap_list[i];
                h += "<option " + (s == o.ap_ssid ? "selected" : "") + " value='" + i + "'>" + escapeHTML(s) + "</option>";
            }
            _q("select").innerHTML = h;
            status(o.ap_list.length + " access points found.");
        }
    }
};
api.onopen = function () {
    api_q();
};

// connection to retrovox pubsub service
// this won't work when connected to retrovox:192.168.1.1
var w = new WebSocket("ws://52.36.194.175:2600/pubsub");
w.onmessage = function (m) {
    info(m.data);
};
w.onopen = function () {
    info("connected to remote server");
    io({cmd:"q"});
};
function io(o)
{
    w.send(JSON.stringify(o));
}
function pub(k,v)
{
   io({k:k,v:v,cmd:"pub"});
}
function tst()
{
    pub("demo/joy","button pushed");
}
</script>
<input type="button" value="joy" onclick="tst()">
</body></html>
)"""";

int sends(int s, const char* str)
{
    return (int)send(s,str,(int)strlen(str),0);
}

int sends(int s, const string& str)
{
    return sends(s,str.c_str());
}

static int hexd(char c)
{
    c = tolower(c);
    if (c >='0' && c <= '9') return c - '0';
    if (c >='a' && c <= 'f') return 10 + c - 'a';
    return 0;
}

const char _hexx[] = "0123456789ABCDEF";
string escape(const string& s)
{
    string e;
    for (int i = 0; i < s.length(); i++) {
        char c = s[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            e += c;
            continue;
        }
        e += '%';
        e += _hexx[(c >> 4) & 0xF];
        e += _hexx[c & 0xF];
    }
    return e;
}

string unescape(const string& s)
{
    string u;
    int len = (int)s.length();
    for (int i = 0; i < len; i++)
    {
        char c = s[i];
        if (c == '%' && (i < len-2))
        {
            c = (hexd(s[i+1]) << 4) | hexd(s[i+2]);
            i += 2;
        }
        u += c;
    }
    return u;
}

map<string,string> qparams(const string& query)
{
    std::map<string,string> r;
    vector<string> args = split(query,"&");
    for (int i = 0; i < (int)args.size(); i++)
    {
        const char* n = args[i].c_str();  // Name
        const char* v = strchr(n,'=');    // scaped value
        if (v != NULL)
            r[string(n,v - n)] = unescape(v + 1);
    }
    return r;
}

static void on_form(std::map<string,string> params)
{
    const string& s = params["s"];
    if (!s.empty()) {
        auto ssidv = get_ssids();
        size_t i = atol(s.c_str());
        join_ssid(ssidv[i],params["p"]);
    }
}

int ws(int s, vector<string>& h);

#include "json.hpp"
using namespace nlohmann;
static string on_dat(map<string,string> params)
{
    json j;
    j["cc"] = "cunt";
    return j.dump();
}

const static char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void base64_encode (char* dst, const uint8_t* src, int len)
{
    uint8_t c,d;
    uint8_t end[3];

    for (; len >= 3; len -= 3) {
        c = *src++;
        *dst++ = b64[(int)(c >> 2)];
        d = (c << 4) & 0x30;

        c = *src++;
        d |= (c >> 4);
        *dst++ = b64[(int)d];
        d = (c << 2) & 0x3C;

        c = *src++;
        d |= (c >> 6);
        *dst++ = b64[(int)d];
        *dst++ = b64[(int)(c & 0x3F)];
    }

    if (len) {
        end[0] = *src++;
        if (--len) end[1] = *src++; else end[1] = 0;
        end[2] = 0;

        c = end[0];
        *dst++ = b64[(int)(c >> 2)];
        d = (c << 4) & 0x30;

        c = end[1];
        d |= (c >> 4);
        *dst++ = b64[(int)d];
        d = (c << 2) & 0x3C;

        if (len) *dst++ = b64[(int)d];
        else *dst++ = '=';
        *dst++ = '=';
    }
    *dst = 0;
}

const signed char _decode64[] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
    52,53,54,55,56,57,58,59,60,61,-1,-1,-1,64,-1,-1,
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
    15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
    -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
    41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
};

int base64_decode(const char* src, vector<uint8_t>& dst)
{
    ssize_t len = strlen(src);
    if (len & 3)
        return -1;
    dst.reserve(len/4*3);

    int s = 0;
    while (src[s])
    {
        int e1 = _decode64[src[s++]];
        int e2 = _decode64[src[s++]];
        int e3 = _decode64[src[s++]];
        int e4 = _decode64[src[s++]];

        if (e1 < 0 || e2 < 0 || e3 < 0 || e4 < 0)
            return -1;

        int chr1 = (e1 << 2) | (e2 >> 4);
        int chr2 = ((e2 & 15) << 4) | (e3 >> 2);
        int chr3 = ((e3 & 3) << 6) | e4;

        dst.push_back(chr1);
        if (e3 != 64)
            dst.push_back(chr2);
        if (e4 != 64)
            dst.push_back(chr3);
    }
    return 0;
}

void sha1_hex(const void* src, int len, char* hex);
void sha1_(const void* src, int len, unsigned char* hash);

string sha1(const string& data)
{
    char hash[20];
    sha1_(data.c_str(),(int)data.length(),(uint8_t*)hash);
    return string(hash,20);
}

string base64(const string& data)
{
    string dst;
    dst.resize(data.length()*2 + 3);
    base64_encode(&dst[0],(const uint8_t*)data.c_str(),(int)data.length());
    dst.resize(strlen(dst.c_str()));
    return dst;
}

string hash64(const string& key)
{
    return base64(sha1(key));
}


#include <sys/poll.h>

string hash64(const string& key);

Url::Url(){};
Url::Url(const string& str, int p) : port(p)
{
    const char* s = str.c_str();
    const char* a = strstr(s,"://");
    if (!a)
        return;
    protocol = string(s,a-s);
    s = a + 3;
    a = strchr(s,'/');
    if (a) {
        host = string(s,a-s);
        path = a;
    } else {
        host = s;
        path = '/';
    }
    a = strchr(host.c_str(),':');
    if (a) {
        port = ::atoi(a+1);
        host.resize(host.size() - strlen(a));
    }
    a = strchr(path.c_str(),'?');
    if (a) {
        query = a+1;
        path.resize(path.size() - strlen(a));
    }
}

class WebSocket {
public:
    int _fd;
    mutex _mutex;
    bool _client;
    uint8_t _buf[1024];
    enum {
        wsop_continuation = 0,
        wsop_text = 1,
        wsop_binary = 2,
        wsop_close = 8,
        wsop_ping = 9,
        wsop_pong = 10
    };

    function<void(WebSocket& t)> onopen;
    function<void(WebSocket& t)> onclose;
    function<void(WebSocket& t,int err)> onerror;
    function<void(WebSocket& t,const uint8_t* m, ssize_t len, int format)> onmessage;

    WebSocket() : _fd(-1),_client(true)
    {
    };

    virtual ~WebSocket()
    {
    };

    int fd() { return _fd; }

    int send(const uint8_t* d, int len)
    {
        return (int)::write(_fd,d,len);
    }

    int recv(uint8_t* d, int len)
    {
        return (int)::read(_fd,d,len);
    }

    int send(const string& s)
    {
        return send((const uint8_t*)s.c_str(),(int)s.length());
    }

    int send_msg(json& j)
    {
        string s = j.dump();
        return send_msg((const uint8_t*)s.c_str(),s.length(),1);
    }

    int send_msg(const uint8_t* m, ssize_t len, int format)
    {
        lock_guard<mutex> lock(_mutex);
        const uint8_t* d = (const uint8_t*)(&m[0]);
        if (len > 512*1024)
        {
            printf("msg too big: %d",(int)len);
            return -1;
        }
        uint8_t h[10] = {0};
        h[0] = 0x80 | format;    // final | text:binary
        h[1] = _client ? 0x80 : 0x00;
        if (len < 126)
        {
            h[1] |= len;
            send(h,2);
        } else if (len < 0x10000) {
            h[1] |= 126;
            h[2] = len >> 8;
            h[3] = len;
            send(h,4);
        } else {
            h[1] |= 127;
            h[6] = len >> 24;
            h[7] = len >> 16;
            h[8] = len >> 8;
            h[9] = len;
            send(h,10);
        };

        if (_client) {
            uint8_t mask[4] = {0x0};
            send(mask, 4);
        }

        send(d,len);
        return 0;
    }

    ssize_t recv_msg(uint8_t* m, ssize_t maxlen, int* op)
    {
        uint8_t h[2];
        uint8_t mask[4];
        if (recv(h,2) != 2)
            return -1;

        *op = h[0] & 0x0F;
        if (!_client && !(h[1] & 0x80))
            return -1;  // Mask bit not set

        uint64_t len = h[1] & 0x7F;
        if (len == 126)
        {
            if (recv(h,2) != 2)
                return -1;
            len = (h[0] << 8) | h[1];
        }
        else if (len == 127)
        {
            uint8_t longlen[8];
            if (recv(longlen,8) != 8)
                return -1;
            len = 0;
            for (int i = 0; i < 8; i++)
                len = (len << 8) | longlen[i];
        }
        if (len > maxlen)
            return -1;

        if (!_client && recv(mask,4) != 4)
            return -1;

        if (recv(m,(int)len) != len)
            return -1;

        for (int i = 0; !_client && i < len; i++)
            m[i] ^= mask[i&3];
        return len;
    }

    int serve(int fd, map<string,string>& hmap)
    {
        _fd = fd;
        _client = false;
        string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n";

        string key = hmap["sec-websocket-key"] + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        response += "Sec-WebSocket-Accept: " + hash64(key) + "\r\n\r\n";
        send(response);

        // Now listen for inbound
        if (onopen)
            onopen(*this);
        return 0;
    }

    int connect(const string& u)
    {
        _client = true;
        Url url(u);
        if (url.protocol.compare("ws"))
            return -1;

        _fd = connect_tcp(url.host.c_str(),url.port);
        if (_fd <= 0)
            return -1;

        const string request =
        "GET / HTTP/1.1\r\n"
        "Host: " + url.host + ":" + ::to_string(url.port) + "\r\n"
        "Connection: upgrade\r\n"
        "Origin: http://localhost\r\n"
        "Upgrade: websocket\r\n"
        "User-Agent: foo\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n";

        if (send(request) != request.size())
            return -1;

        vector<string> status;
        map<string,string> headers;
        if (http_headers(_fd, headers, status) != 101)
            return -1;

        auto it = headers.find("sec-websocket-accept");
        if (it != headers.end())
        {
            string acceptKey;
            acceptKey.append("dGhlIHNhbXBsZSBub25jZQ==");               // our private_ client key sent above
            acceptKey.append("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");   // magic/public key
            if (it->second.compare(hash64(acceptKey)) == 0)             // response should match
            {
                if (onopen)
                    onopen(*this);
                return 0;
            }
        }

        //failure to connect to a websocket server
        return -1;
    }

    int io()
    {
        int op;
        ssize_t len = recv_msg(_buf,sizeof(_buf),&op);
        //printf("io recv_msg %d\n",len);
        if (len <= 0 || op == wsop_close)
        {
            if (onclose)
                onclose(*this);
            return len < 0 ? -1 : 0;
        }
        if (op <= wsop_binary) {
            if (onmessage)
                onmessage(*this,_buf,len,op);
        }
        return 0;
    }
};

static int http_client(int s, const string& method, const string& path, map<string,string>& headers)
{
    printf("http_client:%d %s %s\n",s,method.c_str(),path.c_str());

    // handle form
    printf("path:%s\n",path.c_str());

    if (strncmp(path.c_str(),"/f?",3) == 0) {
        on_form(qparams(path.c_str()+3));
        sends(s,"HTTP/1.1 302 Found\r\nLocation: /\r\nConnection: close\r\n\r\n");
        return 0;
    }

    if (strncmp(path.c_str(),"/dat?",5) == 0) {
        sends(s,"HTTP/1.1 200 OK\r\nContent-type:application/json\r\nConnection: close\r\n\r\n");
        sends(s,on_dat(qparams(path.c_str()+5)));
        return 0;
    }

    // handle unknowns
    if (path != "/") {
        sends(s,"HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n");
        return 0;
    }

    sends(s,"HTTP/1.1 200 OK\r\nContent-type:text/html\r\nConnection: close\r\n\r\n");
    sends(s,form_0);
    return 0;
}


// handle message from web gui
static void api_message(WebSocket& ws, const uint8_t* data, int len, int op)
{
    string s = string((const char*)data,len);
    printf("api:%s\n",s.c_str());
    json j = json::parse(s);
    if (j.count("cmd")) {
        auto& s = j["cmd"];
        if (s == "q") {
            json o;
            o["ap_list"] = get_ssids();
            o["ap_ssid"] = wifi_ssid();
            ws.send_msg(o);
        } else if (s == "join") {
            string pass = j["pass"];
            string ssid = j["ssid"];
            wifi_join(ssid.c_str(),pass.c_str());
        }
    }
}

mutex PubSub::_m;
set<PubSub*> PubSub::_set;

// handle pubsub messages from server
static void pubsub_message(WebSocket& ws, const uint8_t* data, int len, int op)
{
    string s = string((const char*)data,len);
    json j = json::parse(s);
    if (j.count("k"))
        PubSub::on_(j["k"],j["v"]);
}

#include <set>

// serve http and ws requests
WebSocket* _pubsub = 0;

int ps_pub(const string& k, const string& v)
{
    if (!_pubsub)
        return -1;
    json o;
    o["cmd"] = "pub";
    o["k"] = k;
    o["v"] = v;
    return _pubsub->send_msg(o);
}

int ps_sub(const string& k)
{
    if (!_pubsub)
        return -1;
    json o;
    o["cmd"] = "sub";
    o["k"] = k;
    return _pubsub->send_msg(o);
}

int ps_unsub(const string& k)
{
    if (!_pubsub)
        return -1;
    json o;
    o["cmd"] = "unsub";
    o["k"] = k;
    return _pubsub->send_msg(o);
}

bool ap_started();
bool sta_has_ip();

static int server(void *pvParameters)
{
    bool server_inited = false;
    printf("server waintt\n");
    while (!ap_started())
        usleep(50*1000);
    printf("server starting!\n");

    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_sock < 0) {
        printf("Unable to create socket: errno %d", errno);
        return -1;
    }
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in dest_addr = {0};
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(80);
    int err = ::bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0)
        printf("Error occurred during bind: errno %d\n", errno);

    err = listen(listen_sock, 1);
    if (err != 0) {
        printf("Error occurred during listen: errno %d\n", errno);
    } else {
        map<int,unique_ptr<WebSocket>> ws_list;
        while (1) {

            // connect to pubsub server
            if (!server_inited && sta_has_ip())
            {
                _pubsub = new WebSocket();
                if (_pubsub->connect("ws://52.36.194.175:2600/pubsub"))
                {
                    printf("server connect failed\n\n");
                    delete _pubsub;
                    _pubsub = 0;
                } else {
                    _pubsub->onmessage = pubsub_message;
                    ws_list[_pubsub->fd()] = unique_ptr<WebSocket>(_pubsub);    // it is a websocket
                    server_inited = true;
                }
            }

            #define MAX_CLIENTS 16
            struct pollfd pollfds[MAX_CLIENTS + 1] = {0};
            pollfds[0].fd = listen_sock;
            pollfds[0].events = POLLIN;
            int clients = 0;
            for (auto& kv : ws_list) {
                clients++;
                pollfds[clients].fd = kv.first;
                pollfds[clients].events = POLLIN;
            }
            int p = poll(pollfds, clients + 1, 333);  // 10ms
            if (p <= 0) {
                //printf(".\n");
                continue;
            }

            // server
            if (pollfds[0].revents & POLLIN)
            {
                struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
                socklen_t addr_len = sizeof(source_addr);
                int client_fd = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
                if (client_fd < 0)
                    continue;
                #ifdef SO_NOSIGPIPE
                        int set = 1;
                        setsockopt(client_fd, SOL_SOCKET, SO_NOSIGPIPE, (void *)&set, sizeof(int)); //prevent SIGPIPE from being triggered
                #endif
                ws_list[client_fd] = 0;
               // printf("accepted %d\n",client_fd);
                continue;
            }

            // http/websocket io
            for (int i = 1; i <= clients; i++)
            {
                int fd = pollfds[i].fd;
                if (fd > 0 && (pollfds[i].revents & POLLIN))
                {
                    if (!ws_list[fd]) {
                        vector<string> status;
                        map<string,string> headers;
                        if (http_headers(fd, headers, status) == 0)  // BLOCKING HERE?
                        {
                            if (headers.find("sec-websocket-key") != headers.end()) {
                                WebSocket* ws = new WebSocket();
                                ws->onmessage = api_message;
                                ws->serve(fd,headers);
                                ws_list[fd] = unique_ptr<WebSocket>(ws);        // it is a websocket
                            } else {
                                http_client(fd, status[0], status[1], headers); // simple http request
                                shutdown(fd,SHUT_RDWR);
                                close(fd);
                                ws_list.erase(fd);
                            }
                        } else {
                            printf("http_headers failed\n");
                            close(fd);
                            ws_list.erase(fd);
                        }
                        continue;
                    }

                   // printf("%d io\n",fd);
                    if (ws_list[fd]->io() != 0) {
                       // printf("erasing %d\n",fd);
                        ws_list.erase(fd);
                    }
                }
            }
        }
    }
    close(listen_sock);
    return 0;
}

static void tcp_server_task(void *pvParameters)
{
    server(0);
    printf("tcp_server_task bailing\n");
#ifdef vTaskDelete
    vTaskDelete(NULL);
#endif
}

void start_httpd()
{
    start_thread(tcp_server_task,0,0);
}

