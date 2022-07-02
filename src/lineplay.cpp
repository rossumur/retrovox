//
//  lineplay.cpp
//  RetroVox
//
//  Created by Peter Barrett on 1/22/22.
//  Copyright © 2022 Peter Barrett. All rights reserved.
//

#include <stdio.h>
#include "streamer.h"
#include "terminal.h"
#include "unistd.h"
using namespace std;

class LinePlayer {
    Streamer _lines;
public:
    LinePlayer()
    {
        start();
    }

    ~LinePlayer()
    {
    }

    void start()
    {
        _lines.get(FS_URL("doo.z"));
    }

    void draw(int keys)
    {
        uint8_t len = 0;
        if (_lines.read((uint8_t*)&len,1) != 1) {
            _lines.close();
            start();
            return;
        }
        if (!len)
            return;
        display_clear();
        while (len--) {
            uint8_t p[4];
            _lines.read(p,4);
            draw_line(p[0],p[1],p[2],p[3]);
        }
    }
};

/*
 vector<uint8_t> d;
 get_url("file://doo.dat",d);
 auto z = packz(&d[0],d.size());
 put_url("file://doo.z",z);
 */

int lineplay_main(int argc, const char * argv[])
{
    display_mode(Graphics96x96);
    LinePlayer* lp = new LinePlayer();
    for (;;) {
        lp->draw(0);
        if (get_key())
            break;
        usleep(33333/2);
    }
    delete lp;
    return 0;
}
