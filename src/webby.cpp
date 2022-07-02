//
//  webby.cpp
//  RetroVox
//
//  Created by Peter Barrett on 4/21/22.
//  Copyright © 2022 Peter Barrett. All rights reserved.
//

#include "streamer.h"
#include "unistd.h"
#include "json.hpp"
using namespace std;
using namespace nlohmann;

void wbreak(const string& s)
{
    auto v = split(s,"\n");
    if (v.size() > 1) {
        for (string t : v)
            wbreak(t);
        return;
    }

    int x = 0;
    v = split(s," ");
    for (string t : v) {
        if ((t.length() + x) > 24) {
            printf("\n");
            x = 0;
        }
        printf("%s",t.c_str());
        x += t.length() + 1;
        if (x >= 24) {
            printf("\n");
            x = 0;
        } else {
            printf(" ");
        }
    }
}

int get_json(const string& url, json& j)
{
    vector<uint8_t> data;
    if (get_url("http://52.36.194.175:2601/https?url=" + escape(url),data) < 0)
        return -1;
    j = json::parse(string((const char*)&data[0],data.size()));
    return 0;
}

// http://api.aakhilv.me/fun/facts
int fact_main(int argc, const char * argv[])
{
    json o;
    if (get_json("https://api.aakhilv.me/fun/facts",o))
        return -1;
    wbreak(o[0]);
    return 0;
}

// http://v2.jokeapi.dev/joke/Programming
int joke_main(int argc, const char * argv[])
{
    json o;
    if (get_json("https://v2.jokeapi.dev/joke/Programming",o))
        return -1;
    if (o["type"] == "single") {
        wbreak(o["joke"]);
    } else {
        wbreak(o["setup"]);
        printf("\n\n");
        wbreak(o["delivery"]);
    }
    return 0;
}

int _don = 0;
void inbound(const string& k, const string& v, void* arg)
{
    _don = 1;
}

void pst()
{
    PubSub ps;
    ps.sub("demo/joy");
    for (;;) {
        auto m = ps.get();
        if (!m.empty()) {
            for (auto kv : m)
                printf("%s:%s\n",kv.first.c_str(),kv.second.c_str());
            break;
        }
        usleep(1000);
    }
}

// http://api.coindesk.com/v1/bpi/currentprice/USD.json
int btc_main(int argc, const char * argv[])
{
  //  pst();
    json o;
    if (get_json("https://api.coindesk.com/v1/bpi/currentprice/USD.json",o))
        return -1;
    float a = o["bpi"]["USD"]["rate_float"];
    printf("Bitcoin: $%6.2f\n",a);
    return 0;
}

int scan_main(int argc, const char * argv[])
{
    wifi_dump();
    return 0;
}

int join_main(int argc, const char * argv[])
{
    const char* ssid = "";
    const char* pass = "";
    if (argc > 1)
        ssid = argv[1];
    if (argc > 2)
        pass = argv[2];
    wifi_join(ssid,pass);
    return 0;
}

#include "terminal.h"
int chat_main(int argc, const char * argv[])
{
    string name = "anon";
    string topic = "general";
    if (argc > 1)
        name = argv[1];
    if (argc > 2)
        topic = argv[2];
    string intro = "[" + name + " joined " + topic + "]";
    string quit = "[" + name + " left " + topic + "]";
    topic = "chat/" + topic;

    PubSub ps;
    ps.sub(topic);
    ps.pub(topic,intro);
    string line;
    for (;;) {
        if (line.empty()) {
            line = name + ":";
            printf("%s",line.c_str());
        }

        // remember the line we type
        char c = get_key();
        if (c == '\r')
            c = '\n';
        switch (c) {
            case '\b':
                if (line.size() > (name.size() + 1))
                    line.resize(line.size()-1);
                else
                    c = 0;
                break;
            case '\n':
                if (line == name + ":quit") {
                    ps.pub(topic,quit);
                    return 0;
                }
                if (line.size() > (name.size() + 1)) // don't send return
                    ps.pub(topic,line);
                for (int i = 0; i < line.length(); i++)
                    putchar('\b');
                line.clear();
                c = 0;
                break;
            default:
                if (c)
                    line += c;
        }
        if (c)
            putchar(c);

        // undo typing before printing other chat
        auto m = ps.get();
        if (!m.empty()) {
            for (int i = 0; i < line.length(); i++)
                putchar('\b');
            for (auto kv : m)
                printf("%s\n",kv.second.c_str());
            for (int i = 0; i < line.length(); i++)
                putchar(line[i]);
        }
        term_update();
        usleep(33333);
    }
    return 0;
}
