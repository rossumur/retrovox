//
//  core.cpp
//  RetroVox
//
//  Created by Peter Barrett on 11/18/21.
//  Copyright © 2021 Peter Barrett. All rights reserved.
//

#include <stdio.h>
#include <unistd.h>
#include "streamer.h"
#include "utils.h"
using namespace std;

int read_cmd(char* s, int s_max);
int vox_main(int argc, const char * argv[]);
int play_main(int argc, const char * argv[]);
int led_main(int argc, const char * argv[]);

int mod_loop();

extern "C"
int sam_main(int argc, const char * argv[]);

class Manifest
{
    string url;
    vector<string> items;
public:
    int open(const string& url);
    int select(int i);
};

Manifest _manifest;

int ex(int argc, const char * argv[])
{
    string s = argv[0];
    if (s == "l")
        return _manifest.open(argv[1]);
    if (s == "m")
        return _manifest.select(atoi(argv[1])); // execute manifext line
    if (s == "sam")
        return sam_main(argc,argv);
    if (s == "mod")
        return play_main(argc,argv);
    if (s == "mp3")
        return play_main(argc,argv);
    if (s == "vox")
        return vox_main(argc,argv);
    if (s == "led")
        return led_main(argc,argv);
    printf("ex unknown %s\n",argv[0]);
    return 1;
}

int Manifest::open(const string& url)
{
    Streamer s;
    vector<uint8_t> d;
    if (s.get_url(url.c_str(),d) < 0)
        return -1;
    items = split(string((const char*)&d[0],d.size()),"\n");
    return 0;
}

int Manifest::select(int i)
{
    if (i < 0 || i >= items.size())
        return -1;
    GetArgs a(items[i].c_str());
    return ex(a.argc(),a.argv());
}

void core_loop()
{
    char buf[256];
    if (read_cmd(buf,sizeof(buf))) {
        printf("EX: %s\n",buf);
        GetArgs a(buf);
        ex(a.argc(),a.argv());
    }
    // fancy select loop
    Player::loop_all();
}
