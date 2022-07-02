//
//  shell.cpp
//  RetroVox
//
//  Created by Peter Barrett on 1/16/22.
//  Copyright © 2022 Peter Barrett. All rights reserved.
//

#include <stdio.h>
#include "streamer.h"
#include "terminal.h"
#include "utils.h"

Terminal _shell;
string WD;

void write_shell(const char* c, int n)
{
    while (n--)
        _shell.put(*c++);
}

#ifdef ESP32
struct _reent;
typedef int (*writer)(struct _reent* r, void *cookie, const char *buffer, int size);
static writer _old;
int ww(struct _reent* r, void* f, const char* c, int n)
{
    write_shell(c,n);
    return _old(r,f,c,n);
}
#else
typedef int (*writer)(void *cookie, const char *buffer, int size);
static writer _old;
int ww(void* f, const char* c, int n)
{
    write_shell(c,n);
    return _old(f,c,n);
}
#endif

void hook_stdout()
{
    _old = stdout->_write;
    stdout->_write = ww;
}

void unhook_stdout()
{
    stdout->_write = _old;
}

int getline_s(char* dst, int maxlen)
{
    string s = _shell.getline();
    if (s.length() > maxlen-1)
        s = s.substr(0,maxlen-1);
    strcpy(dst,s.c_str());
    return (int)s.length();
}

int getchar_s()
{
    int c = _shell.getchar_();
    if (c == '\r')
        c = '\n';
    putchar(c);
    return c;
}

void term_update()
{
    _shell.update();
}

string full_path(const string& p)
{
    return WD + p;
}

vector<string> tokens(const string& str)
{
    vector<string> tokens;
    const char* s = str.c_str();
    do {
        while (isspace(*s))
            s++;
        if (!*s)
            break;
        const char* t = s;
        if (*s == '"') {
            do {
                s++;
            } while (*s && *s != '"');
            if (*s)
                s++;
        } else {
            while (*s && !isspace(*s))
                s++;
        }
        if (s > t)
            tokens.push_back(string(t,s-t));
    } while (*s);
    return tokens;
}

vector<string> local_dir()
{
    vector<string> all = spiffs_dir();
    return prune(WD,all);
}

map<string,int> split_dirs(vector<string>& files)
{
    auto a = local_dir();
    map<string,int> dirs;
    for (auto f : local_dir()) {
        size_t i = f.find_first_of('/');
        if (i != string::npos)
            dirs[f.substr(0,i)]++;
        else
            files.push_back(f);
    }
    return dirs;
}

enum Cmd {
    BAD = -1,
    LS, RM, CD, CAT, CURL,
};

const char* procs[] {
    "ls",
    "rm",
    "cd",
    "cat",
    "curl",
    0
};

enum App {
    BAD_APP = -1,
    WEEC,WC,TEENYB,TB,SAM,FROTZ,APPLE,PLAY,CHEEP,MARS,LIFE,DOOMED,ELIZA,LATTICE,FACT,JOKE,BTC,SCAN,JOIN,CHAT
};

const char* apps[] {
    "weec",
    "wc",
    "teenyb",
    "tb",
    "sam",
    "frotz",
    "apple",
    "play",
    "cheep",
    "mars",
    "life",
    "doomed",
    "eliza",
    "lattice",
    "fact",
    "joke",
    "btc",
    "scan",
    "join",
    "chat",
    0
};

int tinybasic_main(int argc, const char * argv[]);
int weec_main(int argc, const char * argv[]);
int frotz_main(int argc, const char * argv[]);
int apple_main(int argc, const char * argv[]);
int play_main(int argc, const char * argv[]);
int mars_main(int argc, const char * argv[]);
int cheep_main(int argc, const char * argv[]);
int eliza_main(int argc, const char * argv[]);
int life_main(int argc, const char * argv[]);
int doomed_main(int argc, const char * argv[]);
int lattice_main(int argc, const char * argv[]);

int fact_main(int argc, const char * argv[]);
int joke_main(int argc, const char * argv[]);
int btc_main(int argc, const char * argv[]);
int scan_main(int argc, const char * argv[]);
int join_main(int argc, const char * argv[]);
int chat_main(int argc, const char * argv[]);

extern "C" int sam_main(int argc, const char * argv[]);

int run_app(App a, int argc, const char * argv[])
{
    switch (a) {
        case TB:
        case TEENYB:    return tinybasic_main(argc,argv);
        case WC:
        case WEEC:      return weec_main(argc,argv);
        case FROTZ:     return frotz_main(argc,argv);
        case SAM:       return sam_main(argc,argv);
        case APPLE:     return apple_main(argc,argv);
        case PLAY:      return play_main(argc,argv);
        case MARS:      return mars_main(argc,argv);
        case ELIZA:     return eliza_main(argc,argv);
        case CHEEP:     return cheep_main(argc,argv);
        case LIFE:      return life_main(argc,argv);
        case DOOMED:    return doomed_main(argc,argv);
        case LATTICE:   return lattice_main(argc,argv);

        case FACT:      return fact_main(argc,argv);
        case JOKE:      return joke_main(argc,argv);
        case BTC:       return btc_main(argc,argv);
        case SCAN:      return scan_main(argc,argv);
        case JOIN:      return join_main(argc,argv);
        case CHAT:      return chat_main(argc,argv);

        default:        break;
    }
    return -1;
}

static
int find_token(const string& s, const char** t)
{
    for (int i = 0; t[i]; i++)
        if (s == t[i])
            return i;
    return -1;
}

static
Cmd get_cmd(const string &s)
{
    return (Cmd)find_token(s,procs);
}

static
App get_app(const string &s)
{
    return (App)find_token(s,apps);
}

string common_prefix(const vector<string>& p)
{
    if (p.size() > 1)
    {
        char c;
        for (int i = 0; (c = p[0].c_str()[i]); i++) {
            for (auto& s : p) {
                if (s[i] != c)
                    return s.substr(0,i);
            }
        }
    }
    return p[0];
}

// autocomplete
string tab_hook_(const string& s)
{
    auto v = tokens(s);
    if (v.size() < 2)
        return s;

    vector<string> d;
    if (get_cmd(v[0]) == CD) {
        vector<string> f;
        for (auto kv : split_dirs(f))
            d.push_back(kv.first);
    } else
        d = local_dir();

    vector<string> m = matches(v[1],d);
    if (m.empty())
        return s;
    return v[0] + " " + common_prefix(m);
}

string prompt()
{
    string p = WD.empty() ? "" : WD.substr(0,WD.size()-1);
    printf("\n%s:",p.c_str());
    return _shell.getline(tab_hook_);
}

int ls(int argc, const char * argv[])
{
    vector<string> files;
    map<string,int> d = split_dirs(files);
    for (auto kv : d)
        printf("%7d %s/\n",kv.second,kv.first.c_str());
    for (auto f : files) {
        int len = get_file_size(FS_PATH(full_path(f)));
        printf("%7d %s\n",len,f.c_str());
    }
    return 0;
}

int rm(int argc, const char * argv[])
{
    if (argc < 2)
        return -1;
    return remove_file(FS_PATH(argv[1]));
}

int cd(int argc, const char * argv[])
{
    if (argc < 2)
        return -1;
    string p = argv[1];

    if (p == ".")
        return 0;
    if (p == "..")
    {
        int i = (int)WD.length()-1;
        if (i > 0) {
            while (i--) {
                if (i == 0 || WD[i] == '/') {
                    WD = WD.substr(0,i);
                    return 0;
                }
            }
        }
    } else {
        vector<string> files;
        auto dirs = split_dirs(files);
        string fp = full_path("");
        if (p.find(fp) == 0)
            p = p.substr(fp.length());
        if (dirs.find(p) != dirs.end()) {
            WD += p + "/";
            return 0;
        }
    }

    printf("No such directory\n");
    return -1;
}

int cat_url(const string& u)
{
    Streamer s;
    int r = s.get(u.c_str());
    if (r >= 0) {
        if (r != 200)
            printf("HTTP result:%d\n",r);
        char c;
        while (s.read((uint8_t*)&c,1) == 1)
            putchar(c);
        return 0;
    }
    return r;
}

int exists(const string& p)
{
    if (p.empty() || p[0] == '"' || p[0] == '-')
        return -1;
    return get_file_size(FS_PATH(p));
}

int cat(int argc, const char * argv[])
{
    if (argc < 2 || exists(argv[1]) == -1)
        return -1;
    FILE* f = fopen(FS_PATH(argv[1]),"rb");
    if (!f)
        return -1;
    char c;
    while (fread(&c,1,1,f)==1)
        putchar(c);
    return fclose(f);
}

int curl(int argc, const char * argv[])
{
    if (argc < 2)
        return -1;
    string u = argv[1];
    if (u.substr(0,4) != "http")
        u = "http://" + u;
    return cat_url(u);
}

void full_paths(string& s)
{
    if (s == "." || s == "..")
        return;
    string p = full_path(s);
    if (exists(p) != -1)
        s = p;
}

void shell(void*)
{
    hook_stdout();
    _shell.set_color(0x0E,0x84);
    printf("Retrovox shell");
    for (;;) {
        string line = trim(prompt());
        if (line.empty())
            continue;
        putchar('\n');

        auto p = tokens(line);
        Cmd c = get_cmd(p[0]);
        for (int i = 1; i < (int)p.size(); i++)
            full_paths(p[i]);

        vector<const char*> argv;
        for (string& s : p) {
            if (s[0] == '"')
                s =  s.substr(1,s.length()-2);  // remove quotes
            argv.push_back(s.c_str());
        }

        int r = -1;
        if (c == BAD) {
            App a = get_app(p[0]);
            if (a == BAD_APP)
                printf("'%s' not found\n",p[0].c_str());
            else {
                r = run_app(a,(int)argv.size(),&argv[0]);
                display_mode(Text24x24);
                if (r < 0)
                    printf("'%s' returned %d\n",p[0].c_str(),r);
            }
            continue;
        }

        switch (c) {
            case LS:    r = ls((int)argv.size(),&argv[0]); break;
            case RM:    r = rm((int)argv.size(),&argv[0]); break;
            case CD:    r = cd((int)argv.size(),&argv[0]); break;
            case CAT:   r = cat((int)argv.size(),&argv[0]); break;
            case CURL:  r = curl((int)argv.size(),&argv[0]); break;
            default: break;
        }
        if (r)
            printf("error %d\n",r);
    }
}

//===========================================================================================
//===========================================================================================
//  sam, sp0256

map<string,string> _sam_state;

void sam_reset()
{
    _sam_state.clear();
}

void sam_str(string& s)
{
    if (s == "[]") {
        sam_reset();
        return;
    }
    vector<string> p = {"sam"};
    if (s[0] == '[') {
        auto i = s.find_first_of(']');
        if (i != string::npos) {
            for (auto& param : split(s.substr(1,i-1),",")) {
                param = trim(param);
                if (param.empty())
                    continue;
                auto n = param.substr(1);
                switch (tolower(param[0])) {
                    case 'p': _sam_state["-pitch"] = n; break;
                    case 's': _sam_state["-speed"] = n; break;
                    case 'm': _sam_state["-mouth"] = n; break;
                    case 't': _sam_state["-throat"] = n; break;
                }
            }
            s = s.substr(i+1);
        }
    }
    if (s.empty())
        return;
    for (auto&kv : _sam_state) {
        p.push_back(kv.first);
        p.push_back(kv.second);
    }
    if (s[0] == '#') {
        s = s.substr(1);
        p.push_back("-phonetic");
    }
    p.push_back(s);

    vector<const char*> argv;
    for (string& s : p)
        argv.push_back(s.c_str());
    _shell.update();
    sam_main((int)argv.size(),&argv[0]);
}

void sp0256_str(const string s)
{

}

string _sam_str;
void sam_char(char c)
{
    if (c == '\n') {
        sam_str(_sam_str);
        _sam_str.clear();
    } else
        _sam_str += c;
}

string _sp0256_str;
void sp0256_char(char c)
{
    if (c == '\n') {
        sp0256_str(_sp0256_str);
        _sp0256_str.clear();
    } else
        _sp0256_str += c;
}
