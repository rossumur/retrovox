//
//  utils.cpp
//  RetroVox
//
//  Created by Peter Barrett on 8/31/21.
//  Copyright © 2021 Peter Barrett. All rights reserved.
//

#include <iostream>
#include <string>
#include <vector>
#include "utils.h"
using namespace std;

GetArgs::GetArgs(const char* s)
{
    parse(s);
}
void GetArgs::flush()
{
    if (t.empty())
        return;
    a.push_back(t);
    t.clear();
}

void GetArgs::add(char c)
{
    t.push_back(c);
}

void GetArgs::parse(const char *s)
{
    a.clear();
    char c;
    bool quote = false;
    bool escape = false;
    while ((c = *s++))
    {
        if (escape) {
            add(c);
            escape = false;
            continue;
        }
        switch (c) {
            case ' ':
                if (!quote)
                    flush();
                else
                    add(c);
                break;
            case '"':
                flush();
                quote = !quote;
                break;
            case '\\':
                escape = true;
                break;
            default:
                add(c);
        }
    }
    flush();
    av.resize(a.size());
    for (auto i = 0; i < a.size(); i++)
        av[i] = a[i].c_str();
}

int GetArgs::argc()
{
    return (int)av.size();
}

const char** GetArgs::argv()
{
    return &av[0];
}
