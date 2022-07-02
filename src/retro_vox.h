//
//  retro_vox.h
//  Voice
//
//  Created by Peter Barrett on 8/28/21.
//  Copyright © 2021 Peter Barrett. All rights reserved.
//

#ifndef retro_vox_h
#define retro_vox_h

enum {
    RESET = 112,
    VOLUME,
    SPEED,
    PITCH,
    SPEED_SHORT,
    SILENCE
};

void retro_vox_cmd(uint8_t cmd);
int16_t retro_vox_sample();

int8_t retro_phone(const char* name);
const char* retro_name(int phone);

#endif /* retro_vox_h */
