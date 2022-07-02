//
//  utils.hpp
//  RetroVox
//
//  Created by Peter Barrett on 8/31/21.
//  Copyright © 2021 Peter Barrett. All rights reserved.
//

#ifndef utils_hpp
#define utils_hpp

#include <stdio.h>
#include <string>
#include <vector>
#include <map>
using namespace std;

class GetArgs
{
    vector<const char*> av;
    vector<string> a;
    string t;
    void flush();
    void add(char c);
public:
    GetArgs(const char* s);
    void parse(const char *s);
    int argc();
    const char** argv();
};

/*
class YeetBuf {
public:
    //int index;
    //int used;
    uint8_t data[4+192*4];
};

YeetBuf* get_yeet();
void queue_yeet(YeetBuf* yb);

YeetBuf* display_yeet();
void release_yeet(YeetBuf* yb);
*/

void audio_write_1(int16_t s);

#endif /* utils_hpp */
