//
//  phone_names.h
//  Voice
//
//  Created by Peter Barrett on 8/3/21.
//  Copyright © 2021 Peter Barrett. All rights reserved.
//

#ifndef phone_names_h
#define phone_names_h

enum {
   Pause0        = 0,                     ///< Pause 0ms
   Pause1        = 1,                     ///< Pause 100ms
   Pause2        = 2,                     ///< Pause 200ms
   Pause3        = 3,                     ///< Pause 700ms
   Pause4        = 4,                     ///< Pause 30ms
   Pause5        = 5,                     ///< Pause 60ms
   Pause6        = 6,                     ///< Pause 90ms

   Fast          = 7,                     ///< Next phoneme at 0.5 speed
   Slow          = 8,                     ///< Next phoneme at 1.5 speed
   Stress        = 14,                    ///< Next phoneme with some stress
   Relax         = 15,                    ///< Next phoneme with relaxation

   Wait          = 16,                    ///< Stops and waits for a Start (see manual)
   Soft          = 18,                    ///< Stops and waits for a Start (see manual)

   Volume        = 20,                    ///< Next octet is volume 0 to 127. Default 96
   Speed         = 21,                    ///< Next octet is speed 0 to 127. Default 114
   Pitch         = 22,                    ///< Next octet is pitch in Hz = to 255
   Bend          = 23,                    ///< Next octet is frequency bend  to 15. Default is 5
   PortCtr       = 24,                    ///< Next octet is port control value. See manual. Default is 7
   Port          = 25,                    ///< Next octet is Port Output Value. See manual. Default is 0
   Repeat        = 26,                    ///< Next octet is repeat count. 0 to 255
   CallPhrase    = 28,                    ///< Next octet is EEPROM phrase to play and return. See manual.
   GotoPhrase    = 29,                    ///< Next octet is EEPROM phgrase to go to. See manual.
   Delay         = 30,                    ///< Next octet is delay in multiples of 10ms. 0 to 255.
   Reset         = 31                    ///< Reset Volume Speed, Pitch, Bend to defaults.}
};

const char* cmd_name[] = {
    "Pause0",//       = 0,                     ///< Pause 0ms
    "Pause1",//        = 1,                     ///< Pause 100ms
    "Pause2",//        = 2,                     ///< Pause 200ms
    "Pause3",//        = 3,                     ///< Pause 700ms
    "Pause4",//        = 4,                     ///< Pause 30ms
    "Pause5",//        = 5,                     ///< Pause 60ms
    "Pause6",//        = 6,                     ///< Pause 90ms

    "Fast",//          = 7,                     ///< Next phoneme at 0.5 speed
    "Slow",//          = 8,                     ///< Next phoneme at 1.5 speed
    "9",
    "10",
    "11",
    "12",
    "13",
    "Stress",//        = 14,                    ///< Next phoneme with some stress
    "Relax",//         = 15,                    ///< Next phoneme with relaxation

    "Wait",//          = 16,                    ///< Stops and waits for a Start (see manual)
    "17",
    "Soft",//          = 18,                    ///< Stops and waits for a Start (see manual)
    "19",

    "Volume",//        = 20,                    ///< Next octet is volume 0 to 127. Default 96
    "Speed",//         = 21,                    ///< Next octet is speed 0 to 127. Default 114
    "Pitch",//         = 22,                    ///< Next octet is pitch in Hz = to 255
    "Bend",//          = 23,                    ///< Next octet is frequency bend  to 15. Default is 5
    "PortCtr",//       = 24,                    ///< Next octet is port control value. See manual. Default is 7
    "Port",//          = 25,                    ///< Next octet is Port Output Value. See manual. Default is 0
    "Repeat",//        = 26,                    ///< Next octet is repeat count. 0 to 255
    "27",
    "CallPhrase",//    = 28,                    ///< Next octet is EEPROM phrase to play and return. See manual.
    "GotoPhrase",//    = 29,                    ///< Next octet is EEPROM phgrase to go to. See manual.
    "Delay",//         = 30,                    ///< Next octet is delay in multiples of 10ms. 0 to 255.
    "Reset",//         = 31
};

//#include <ioctls.h>

const char* _speakjet_phones[] = {
    "IY", // 128 IY See, Even, Feed 70 Voiced Long Vowel
    "IH", // 129 IH Sit, Fix, Pin 70 Voiced Short Vowel
    "EY", // 130 EY Hair, Gate, Beige 70 Voiced Long Vowel
    "EH", // 131 EH Met, Check, Red 70 Voiced Short Vowel
    "AY", // 132 AY Hat, Fast, Fan 70 Voiced Short Vowel
    "AX", // 133 AX Cotten 70 Voiced Short Vowel
    "UX", // 134 UX Luck, Up, Uncle 70 Voiced Short Vowel
    "OH", // 135 OH Hot, Clock, Fox 70 Voiced Short Vowel
    "AW", // 136 AW Father, Fall 70 Voiced Short Vowel
    "OW", // 137 OW Comb, Over, Hold 70 Voiced Long Vowel
    "UH", // 138 UH Book, Could, Should 70 Voiced Short Vowel
    "UW", // 139 UW Food, June 70 Voiced Long Vowel
    "MM", // 140 MM Milk, Famous, 70 Voiced Nasal
    "NE", // 141 NE Nip, Danger, Thin 70 Voiced Nasal
    "NO", // 142 NO No, Snow, On 70 Voiced Nasal
    "NGE", // 143 NGE Think, Ping 70 Voiced Nasal
    "NGO", // 144 NGO Hung, Song 70 Voiced Nasal
    "LE", // 145 LE Lake, Alarm, Lapel 70 Voiced Resonate
    "LO", // 146 LO Clock, Plus, Hello 70 Voiced Resonate
    "WW", // 147 WW Wool, Sweat 70 Voiced Resonate
    "RR", // 148 RR Ray, Brain, Over 70 Voiced Resonate
    "IYRR", // 149 IYRR Clear, Hear, Year 200 Voiced R Color Vowel
    "EYRR", // 150 EYRR Hair, Stair, Repair 200 Voiced R Color Vowel
    "AXRR", // 151 AXRR Fir, Bird, Burn 190 Voiced R Color Vowel
    "AWRR", // 152 AWRR Part, Farm, Yarn 200 Voiced R Color Vowel
    "OWRR", // 153 OWRR Corn, Four, Your 185 Voiced R Color Vowel
    "EYIY", // 154 EYIY Gate, Ate, Ray 165 Voiced Diphthong
    "OHIY", // 155 OHIY Mice, Fight, White 200 Voiced Diphthong
    "OWIY", // 156 OWIY Boy, Toy, Voice 225 Voiced Diphthong
    "OHIH", // 157 OHIH Sky, Five, I 185 Voiced Diphthong
    "IYEH", // 158 IYEH Yes, Yarn, Million 170 Voiced Diphthong
    "EHLL", // 159 EHLL Saddle, Angle, Spell 140 Voiced Diphthong
    "IYUW", // 160 IYUW Cute, Few, 180 Voiced Diphthong
    "AXUW", // 161 AXUW Brown, Clown, Thousand 170 Voiced Diphthong
    "IHWW", // 162 IHWW Two, New, Zoo 170 Voiced Diphthong
    "AYWW", // 163 AYWW Our, Ouch, Owl 200 Voiced Diphthong
    "OWWW", // 164 OWWW Go, Hello, Snow 131 Voiced Diphthong
    "JH", // 165 JH Dodge, Jet, Savage 70 Voiced Affricate
    "VV", // 166 VV Vest, Even, 70 Voiced Fictive
    "ZZ", // 167 ZZ Zoo, Zap 70 Voiced Fictive
    "ZH", // 168 ZH Azure, Treasure 70 Voiced Fictive
    "DH", // 169 DH There, That, This 70 Voiced Fictive
    "BE", // 170 BE Bear, Bird, Beed 45 Voiced Stop
    "BO", // 171 BO Bone, Book Brown 45 Voiced Stop
    "EB", // 172 EB Cab, Crib, Web 10 Voiced Stop
    "OB", // 173 OB Bob, Sub, Tub 10 Voiced Stop
    "DE", // 174 DE Deep, Date, Divide 45 Voiced Stop
    "DO", // 175 DO Do, Dust, Dog 45 Voiced Stop
    "ED", // 176 ED Could, Bird 10 Voiced Stop
    "OD", // 177 OD Bud, Food 10 Voiced Stop
    "GE", // 178 GE Get, Gate, Guest, 55 Voiced Stop
    "GO", // 179 GO Got, Glue, Goo 55 Voiced Stop
    "EG", // 180 EG Peg, Wig 55 Voiced Stop
    "OG", // 181 OG Dog, Peg 55 Voiced Stop
    "CH", // 182 CH Church, Feature, March 70 Voiceless Affricate
    "HE", // 183 HE Help, Hand, Hair 70 Voiceless Fricative
    "HO", // 184 HO Hoe, Hot, Hug 70 Voiceless Fricative
    "WH", // 185 WH Who, Whale, White 70 Voiceless Fricative
    "FF", // 186 FF Food, Effort, Off 70 Voiceless Fricative
    "SE", // 187 SE See, Vest, Plus 40 Voiceless Fricative
    "SO", // 188 SO So, Sweat 40 Voiceless Fricative
    "SH", // 189 SH Ship, Fiction, Leash 50 Voiceless Fricative
    "TH", // 190 TH Thin, month 40 Voiceless Fricative
    "TT", // 191 TT Part, Little, Sit 50 Voiceless Stop
    "TU", // 192 TU To, Talk, Ten 70 Voiceless Stop
    "TS", // 193 TS Parts, Costs, Robots 170 Voiceless Stop
    "KE", // 194 KE Cant, Clown, Key 55 Voiceless Stop
    "KO", // 195 KO Comb, Quick, Fox 55 Voiceless Stop
    "EK", // 196 EK Speak, Task 55 Voiceless Stop
    "OK", // 197 OK Book, Took, October 45 Voiceless Stop
    "PE", // 198 PE People, Computer 99 Voiceless Stop
    "PO", // 199 PO Paw, Copy 99 Voiceless Stop
    "R0", // 200 R0 80 Robot
    "R1", // 201 R1 80 Robot
    "R2", // 202 R2 80 Robot
    "R3", // 203 R3 80 Robot
    "R4", // 204 R4 80 Robot
    "R5", // 205 R5 80 Robot
    "R6", // 206 R6 80 Robot
    "R7", // 207 R7 80 Robot
    "R8", // 208 R8 80 Robot
    "R9", // 209 R9 80 Robot
    "A0", // 210 A0 300 Alarm
    "A1", // 211 A1 101 Alarm
    "A2", // 212 A2 102 Alarm
    "A3", // 213 A3 540 Alarm
    "A4", // 214 A4 530 Alarm
    "A5", // 215 A5 500 Alarm
    "A6", // 216 A6 135 Alarm
    "A7", // 217 A7 600 Alarm
    "A8", // 218 A8 300 Alarm
    "A9", // 219 A9 250 Alarm
    "B0", // 220 B0 200 Beeps
    "B1", // 221 B1 270 Beeps
    "B2", // 222 B2 280 Beeps
    "B3", // 223 B3 260 Beeps
    "B4", // 224 B4 300 Beeps
    "B5", // 225 B5 100 Beeps
    "B6", // 226 B6 104 Beeps
    "B7", // 227 B7 100 Beeps
    "B8", // 228 B8 270 Beeps
    "B9", // 229 B9 262 Beeps
    "C0", // 230 C0 160 Biological
    "C1", // 231 C1 300 Biological
    "C2", // 232 C2 182 Biological
    "C3", // 233 C3 120 Biological
    "C4", // 234 C4 175 Biological
    "C5", // 235 C5 350 Biological
    "C6", // 236 C6 160 Biological
    "C7", // 237 C7 260 Biological
    "C8", // 238 C8 95 Biological
    "C9", // 239 C9 75 Biological
    "D0", // 240 D0 0 95 DTMF
    "D1", // 241 D1 1 95 DTMF
    "D2", // 242 D2 2 95 DTMF
    "D3", // 243 D3 3 95 DTMF
    "D4", // 244 D4 4 95 DTMF
    "D5", // 245 D5 5 95 DTMF
    "D6", // 246 D6 6 95 DTMF
    "D7", // 247 D7 7 95 DTMF
    "D8", // 248 D8 8 95 DTMF
    "D9", // 249 D9 9 95 DTMF
    "D10", // 250 D10 * 95 DTMF
    "D11", // 251 D11 # 95 DTMF
    "M0", // 252 M0 Sonar Ping 125 Miscellaneous
    "M1", // 253 M1 Pistol Shot 250 Miscellaneous
    "M2", // 254 M2 WOW 530 Miscellaneous
    "A255", //
};

// 128-199
const char* _sj2sam[] = {
    "IY",
    "IX",
    "EY",
    "EH",
    "AE",
    "AH",
    "AH",
    "AA",
    "AA",
    "OW",
    "UW",
    "UX",
    "M",
    "N",
    "N",
    "NX",
    "NX",
    "L",
    "L",
    "WH",
    "R",
    "EY AH",
    "AX",
    "ER",
    "AA",
    "AO",
    "EY",
    "AY",
    "OY",
    "AY",
    "Y",
    "L",
    "UX",
    "AW",
    "UX",
    "AW",
    "OW",
    "J",
    "V",
    "Z",
    "ZH",
    "DH",
    "B",
    "B",
    "B",
    "B",
    "D",
    "D",
    "D",
    "D",
    "G",
    "G",
    "G",
    "G",
    "CH",
    "/H",
    "/H",
    "WH",
    "F",
    "S",
    "S",
    "SH",
    "TH",
    "T",
    "T",
    "T",
    "K",
    "K",
    "K",
    "K",
    "P",
    "P"
};

// 128-199
const char* _cmu_phonemes[] = {
    "IY",
    "IH",
    "EY",
    "EH",
    "AE",
    "AH",
    "AH",
    "AA",
    "AA",
    "OW",
    "UH",
    "UW",
    "M",
    "N",
    "N",
    "NG",
    "NG",
    "L",
    "L",
    "W",
    "R",
    "EY AH",
    "ER",
    "ER",
    "AA",
    "AO",
    "EY",
    "AY",
    "OY",
    "AY",
    "Y",
    "L",
    "UW",
    "AW",
    "UW",
    "AW",
    "OW",
    "JH",
    "V",
    "Z",
    "ZH",
    "DH",
    "B",
    "B",
    "B",
    "B",
    "D",
    "D",
    "D",
    "D",
    "G",
    "G",
    "G",
    "G",
    "CH",
    "HH",
    "HH",
    "W",
    "F",
    "S",
    "S",
    "SH",
    "TH",
    "T",
    "T",
    "T",
    "K",
    "K",
    "K",
    "K",
    "P",
    "P"
};

uint8_t spo2speakjet[64] = {
    4, 5, 6, 1, 2, 156, 155, 131, 195, 199, 165, 141, 129, 192, 153, 134, 140, 191, 169, 128, 154, 176, 139, 135, 136, 128, 132, 183, 170, 190, 138, 160, 163, 175, 180, 166, 179, 189, 168, 148, 186, 194, 194, 167, 144, 145, 147, 150, 185, 128, 182, 133, 151, 164, 169, 187, 142, 184, 153, 152, 149, 178, 159, 170
};

// binary mapping from sj >= 128 to speakjet
uint8_t speakjet2spo[200-128] = {
    19, // "IY", // 128 IY See, Even, Feed 70 Voiced" Long Vowel -> SPO 19 IY See
    12, // "IH", // 129 IH Sit, Fix, Pin 70 Voiced Short Vowel -> SPO 12 IH Sit
    20, // "EY", // 130 EY Hair, Gate, Beige 70 Voiced Long Vowel -> SPO 20 EY Beige
    7, // "EH", // 131 EH Met, Check, Red 70 Voiced Short Vowel -> SPO 7 EH End *
    6, // "AY", // 132 AY Hat, Fast, Fan 70 Voiced Short Vowel -> SPO 6 AY
    15, // "AX", // 133 AX Cotten 70 Voiced Short Vowel -> SPO 15 AX
    15, // "UX", // 134 UX Luck, Up, Uncle 70 Voiced Short Vowel -> SPO 15 AX Succeed *
    24, // "OH", // 135 OH Hot, Clock, Fox 70 Voiced Short Vowel -> SPO 24 AA Hot
    //59, // "AW", // 136 AW Father, Fall 70 Voiced Short Vowel -> SPO 59 AR Alarm *
    32, // "AW", // 136 AW Father, Fall 70 Voiced Short Vowel -> SPO AW Hot?
    53, // "OW", // 137 OW Comb, Over, Hold 70 Voiced Long Vowel -> SPO 53 OW Beau *
    30, // "UH", // 138 UH Book, Could, Should 70 Voiced Short Vowel -> SPO 30 UH Book
    31, // "UW", // 139 UW Food, June 70 Voiced Long Vowel -> SPO 31 UW2 Food
    16, // "MM", // 140 MM Milk, Famous, 70 Voiced Nasal -> SPO 16 MM Milk
    11, // "NE", // 141 NE Nip, Danger, Thin 70 Voiced Nasal -> SPO 11 NN1 Thin
    56, // "NO", // 142 NO No, Snow, On 70 Voiced Nasal -> SPO 56 NN2 No
    44, // "NGE", // 143 NGE Think, Ping 70 Voiced Nasal -> SPO 44 NG Anchor *
    44, // "NGO", // 144 NGO Hung, Song 70 Voiced Nasal -> SPO 44 NG Anchor *
    45, // "LE", // 145 LE Lake, Alarm, Lapel 70 Voiced Resonate -> SPO 45 LL Lake
    62, // "LO", // 146 LO Clock, Plus, Hello 70 Voiced Resonate -> SPO 62 EL Saddle *
    46, // "WW", // 147 WW Wool, Sweat 70 Voiced Resonate -> SPO 46 WW Wool
    39, // "RR", // 148 RR Ray, Brain, Over 70 Voiced Resonate -> SPO 39 RR2 Brain
    60, // "IYRR", // 149 IYRR Clear, Hear, Year 200 Voiced R Color Vowel -> SPO 60 YR Clear
    47, // "EYRR", // 150 EYRR Hair, Stair, Repair 200 Voiced R Color Vowel -> SPO 47 XR Repair
    52, // "AXRR", // 151 AXRR Fir, Bird, Burn 190 Voiced R Color Vowel -> SPO 52 ER2 Fir
    59, // "AWRR", // 152 AWRR Part, Farm, Yarn 200 Voiced R Color Vowel -> SPO 59 AR Alarm
    23, // "OWRR", // 153 OWRR Corn, Four, Your 185 Voiced R Color Vowel -> SPO 23 AO Aught
    20, // "EYIY", // 154 EYIY Gate, Ate, Ray 165 Voiced Diphthong -> SPO 20 EY Beige
    6, // "OHIY", // 155 OHIY Mice, Fight, White 200 Voiced Diphthong -> SPO 6 AY Sky
    5, // "OWIY", // 156 OWIY Boy, Toy, Voice 225 Voiced Diphthong -> SPO 5 OY Boy
    6, // "OHIH", // 157 OHIH Sky, Five, I 185 Voiced Diphthong -> SPO 6 AY Sky
    49, // "IYEH", // 158 IYEH Yes, Yarn, Million 170 Voiced Diphthong -> SPO 49 YY1 Yes
    62, // "EHLL", // 159 EHLL Saddle, Angle, Spell 140 Voiced Diphthong -> SPO 62 EL Saddle
    31, // "IYUW", // 160 IYUW Cute, Few, 180 Voiced Diphthong -> SPO 31 UW2 Food *
    32, // "AXUW", // 161 AXUW Brown, Clown, Thousand 170 Voiced Diphthong -> SPO 32 AW Out
    31, // "IHWW", // 162 IHWW Two, New, Zoo 170 Voiced Diphthong -> SPO 31 UW2 Food
    32, // "AYWW", // 163 AYWW Our, Ouch, Owl 200 Voiced Diphthong -> SPO 32 AW Out
    53, // "OWWW", // 164 OWWW Go, Hello, Snow 131 Voiced Diphthong -> SPO 53 OW Beau
    10, // "JH", // 165 JH Dodge, Jet, Savage 70 Voiced Affricate -> SPO 10 JH Dodge
    35, // "VV", // 166 VV Vest, Even, 70 Voiced Fictive -> SPO 35 VV Vest
    43, // "ZZ", // 167 ZZ Zoo, Zap 70 Voiced Fictive -> SPO 43 ZZ Zoo
    38, // "ZH", // 168 ZH Azure, Treasure 70 Voiced Fictive -> SPO 38 ZH Azure
    54, // "DH", // 169 DH There, That, This 70 Voiced Fictive -> SPO 54 DH2 Bath
    63, // "BE", // 170 BE Bear, Bird, Beed 45 Voiced Stop -> SPO 63 BB2 Business
    63, // "BO", // 171 BO Bone, Book Brown 45 Voiced Stop -> SPO 63 BB2 Business
    63, // "EB", // 172 EB Cab, Crib, Web 10 Voiced Stop -> SPO 63 BB2 Business
    63, // "OB", // 173 OB Bob, Sub, Tub 10 Voiced Stop -> SPO 63 BB2 Business
    33, // "DE", // 174 DE Deep, Date, Divide 45 Voiced Stop -> SPO 33 DD2 Do
    33, // "DO", // 175 DO Do, Dust, Dog 45 Voiced Stop -> SPO 33 DD2 Do
    33, // "ED", // 176 ED Could, Bird 10 Voiced Stop -> SPO 33 DD2 Do
    33, // "OD", // 177 OD Bud, Food 10 Voiced Stop -> SPO 33 DD2 Do
    61, // "GE", // 178 GE Get, Gate, Guest, 55 Voiced Stop -> SPO 61 GG2 Guest
    61, // "GO", // 179 GO Got, Glue, Goo 55 Voiced Stop -> SPO 61 GG2 Guest
    61, // "EG", // 180 EG Peg, Wig 55 Voiced Stop -> SPO 61 GG2 Guest
    61, // "OG", // 181 OG Dog, Peg 55 Voiced Stop -> SPO 61 GG2 Guest
    50, // "CH", // 182 CH Church, Feature, March 70 Voiceless Affricate -> SPO 50 CH Church
    57, // "HE", // 183 HE Help, Hand, Hair 70 Voiceless Fricative -> SPO 57 HH2 Hoe
    57, // "HO", // 184 HO Hoe, Hot, Hug 70 Voiceless Fricative -> SPO 57 HH2 Hoe
    48, // "WH", // 185 WH Who, Whale, White 70 Voiceless Fricative -> SPO 48 WH Whig
    40, // "FF", // 186 FF Food, Effort, Off 70 Voiceless Fricative -> SPO 40 FF Food
    55, // "SE", // 187 SE See, Vest, Plus 40 Voiceless Fricative -> SPO 55 SS Vest
    55, // "SO", // 188 SO So, Sweat 40 Voiceless Fricative -> SPO 55 SS Vest
    37, // "SH", // 189 SH Ship, Fiction, Leash 50 Voiceless Fricative -> SPO 37 SH Ship
    29, // "TH", // 190 TH Thin, month 40 Voiceless Fricative -> SPO 29 TH Thin
    17, // "TT", // 191 TT Part, Little, Sit 50 Voiceless Stop -> SPO 17 TT1 Part
    17, // "TU", // 192 TU To, Talk, Ten 70 Voiceless Stop -> SPO 17 TT1 Part
    17, // "TS", // 193 TS Parts, Costs, Robots 170 Voiceless Stop -> SPO 17 TT1 Part
    42, // "KE", // 194 KE Cant, Clown, Key 55 Voiceless Stop -> SPO 42 KK1 Cant
    42, // "KO", // 195 KO Comb, Quick, Fox 55 Voiceless Stop -> SPO 42 KK1 Cant
    42, // "EK", // 196 EK Speak, Task 55 Voiceless Stop -> SPO 42 KK1 Cant
    42, // "OK", // 197 OK Book, Took, October 45 Voiceless Stop -> SPO 42 KK1 Cant
    9, // "PE", // 198 PE People, Computer 99 Voiceless Stop -> SPO 9 PP Pow
    9, // "PO", // 199 PO Paw, Copy 99 Voiceless Stop -> SPO 9 PP Pow };
};

typedef struct {
    const char* name;
    float duration_ms;
} SPOName;

SPOName sponames[64] = {
{"PA1",6.4}, // PAUSE
{"PA2",25.6}, // PAUSE
{"PA3",44.8}, // PAUSE
{"PA4",96}, // PAUSE

{"PA5",198.4}, // PAUSE 4
{"OY",291.2}, // Boy
{"AY",172.9}, // Sky
{"EH",54.6}, // End

{"KK3",76.8}, // Comb 8
{"PP",147.2}, // Pow
{"JH",98.4}, // Dodge
{"NN1",172.9}, // Thin

{"IH",45.5}, // Sit 12
{"TT2",96}, // To
{"RR1",127.4}, // Rural
{"AX",54.6}, // Succeed

{"MM",182}, // Milk 16
{"TT1",76.8}, // Part
{"DH1",136.5}, // They
{"IY",172.9}, // See

{"EY",200.2}, // Beige 20
{"DD1",45.5}, // Could
{"UW1",63.7}, // To
{"AO",72.8}, // Aught

{"AA",63.7}, // Hot 24
{"YY2",127.4}, // Yes
{"AE",81.9}, // Hat
{"HH1",89.6}, // He

{"BB1",36.4}, // Business 28
{"TH",128}, // Thin
{"UH",72.8}, // Book
{"UW2",172.9}, // Food

{"AW",254.8}, // Out 32
{"DD2",72.1}, // Do
{"GG3",110.5}, // Wig
{"VV",127.4}, // Vest

{"GG1",72.1}, // Got 36
{"SH",198.4}, // Ship
{"ZH",134.1}, // Azure
{"RR2",81.9}, // Brain

{"FF",108.8}, // Food 40
{"KK2",134.4}, // Sky
{"KK1",115.2}, // Cant
{"ZZ",148.6}, // Zoo

{"NG",200.2}, // Anchor 44
{"LL",81.9}, // Lake
{"WW",145.6}, // Wool
{"XR",245.7}, // Repair

{"WH",145.2}, // Whig 48
{"YY1",91}, // Yes
{"CH",147.2}, // Church
{"ER1",109.2}, // Letter

{"ER2",209.3}, // Fir 52
{"OW",172.9}, // Beau
{"DH2",182}, // Bath
{"SS",64}, // Vest

{"NN2",136.5}, // No 56
{"HH2",126}, // Hoe
{"OR",236.6}, // Store
{"AR",200.2}, // Alarm

{"YR",245.7}, // Clear 60
{"GG2",69.4}, // Guest
{"EL",136.5}, // Saddle
{"BB2",50.2}, // Business
};

const char* sam_phones[80] = {
    " ", // 0
    ".", // 1
    "?", // 2
    ",", // 3
    "-", // 4
    "IY", // 5
    "IH", // 6
    "EH", // 7
    "AE", // 8
    "AA", // 9
    "AH", // 10
    "AO", // 11
    "UH", // 12
    "AX", // 13
    "IX", // 14
    "ER", // 15
    "UX", // 16
    "OH", // 17
    "RX", // 18
    "LX", // 19
    "WX", // 20
    "YX", // 21
    "WH", // 22
    "R", // 23
    "L", // 24
    "W", // 25
    "Y", // 26
    "M", // 27
    "N", // 28
    "NX", // 29
    "DX", // 30
    "Q", // 31
    "S", // 32
    "SH", // 33
    "F", // 34
    "TH", // 35
    "/H", // 36
    "/X", // 37
    "Z", // 38
    "ZH", // 39
    "V", // 40
    "DH", // 41
    "CH", // 42
    "43", // 43
    "J", // 44
    "45", // 45
    "46", // 46
    "47", // 47
    "EY", // 48
    "AY", // 49
    "OY", // 50
    "AW", // 51
    "OW", // 52
    "UW", // 53
    "B", // 54
    "55", // 55
    "56", // 56
    "D", // 57
    "58", // 58
    "59", // 59
    "G", // 60
    "61", // 61
    "62", // 62
    "GX", // 63
    "64", // 64
    "65", // 65
    "P", // 66
    "67", // 67
    "68", // 68
    "T", // 69
    "70", // 70
    "71", // 71
    "K", // 72
    "73", // 73
    "74", // 74
    "KX", // 75
    "76", // 76
    "77", // 77
    "UL", // 78
    "UM", // 79
};

const char* sjcode2sam[200-128] = {
"IY", // /IY {"Y":8,"IY":4} code: 128 mismatch lex: IY dic:Y
"IH", // /IH {"IH":31,"IX":12} code: 129 mismatch lex: IX dic:IH
"EY", // /EY {"EH":3} code: 130 mismatch lex: EY dic:EH
"EH", // /EH {"EH":34,"IX":11,"AX":3} code: 131
"AE", // /AY {"AE":26} code: 132
"ER", // /AX {"IX":3,"AX":2,"ER":9,"AH":2} code: 133 mismatch lex: AH dic:ER
"AH", // /UX {"AH":25} code: 134
"AA", // /OH {"AO":6,"AA":8,"AH":2} code: 135
"AA", // /AW {"AA":2} code: 136
"OH", // /OW {"OH":4,"OW":11} code: 137
"UH", // /UH {"UH":5} code: 138 mismatch lex: UW dic:UH
"UW", // /UW {"UW":8} code: 139 mismatch lex: UX dic:UW
"M", // /MM {"M":31} code: 140
"N", // /NE {"N":47,"D":2,"UN":3} code: 141
"N", // /NO {"N":18,"UN":2} code: 142
"NX", // /NGE {"N":1} code: 143 mismatch lex: NX dic:N
"NX", // /NGO {"NX":1} code: 144
"L", // /LE {"IY":3,"L":28,"UL":4} code: 145
"L", // /LO {"L":26,"UL":2} code: 146
"W", // /WW {"W":17} code: 147 mismatch lex: WH dic:W
"R", // /RR {"R":37} code: 148
"AH", // missing 149 EY AH    // TODO
"R", // /EYRR {"R":1} code: 150 mismatch lex: AX dic:R
"R", // /AXRR {"R":6,"ER":3} code: 151 mismatch lex: ER dic:R
"AA", // /AWRR {"AA":1} code: 152
"R", // /OWRR {"R":4} code: 153 mismatch lex: AO dic:R
"EY", // /EYIY {"EY":27} code: 154
"AY", // /OHIY {"AY":22} code: 155
"OY", // /OWIY {"OY":3} code: 156
"AY", // /OHIH {"AY":8} code: 157
"Y", // /IYEH {"Y":2} code: 158
"L", // missing 159 L
"UW", // /IYUW {"UW":3} code: 160 mismatch lex: UX dic:UW
"AW", // missing 161 AW
"UX", // missing 162 UX
"AW", // /AYWW {"AW":11} code: 163
"OW", // /OWWW {"OW":14} code: 164
"J", // /JH {"J":15} code: 165
"V", // /VV {"V":19} code: 166
"Z", // /ZZ {"Z":13} code: 167
"ZH", // /ZH {"Z":1} code: 168 mismatch lex: ZH dic:Z
"DH", // /DH {"DH":7} code: 169
"B", // /BE {"B":2} code: 170
"B", // /BO {"B":5} code: 171
"B", // /EB {"B":2} code: 172
"B", // /OB {"B":2} code: 173
"D", // /DE {"D":9} code: 174
"D", // /DO {"D":5} code: 175
"D", // /ED {"D":9} code: 176
"D", // /OD {"D":13} code: 177
"G", // /GE {"G":6} code: 178
"G", // /GO {"G":3} code: 179
"G", // missing 180 G
"G", // missing 181 G
"CH", // /CH {"CH":8} code: 182
"/H", // /HE {"/H":6} code: 183
"/H", // /HO {"/H":6} code: 184
"WH", // /WH {"WH":5} code: 185
"F", // /FF {"F":26} code: 186
"S", // /SE {"S":67} code: 187
"S", // /SO {"S":5} code: 188
"SH", // /SH {"S":4} code: 189 mismatch lex: SH dic:S
"TH", // /TH {"TH":9} code: 190
"T", // /TT {"T":84} code: 191
"T", // /TU {"T":2} code: 192
"T", // missing 193 T
"K", // /KE {"K":11} code: 194
"K", // /KO {"K":7} code: 195
"K", // /EK {"K":10} code: 196
"K", // /OK {"K":8} code: 197
"P", // /PE {"P":9} code: 198
"P", // /PO {"P":19} code: 199
};

const char* sam2spo[] = {
    "IY","IY",
    "IH","IH",
    "EH","EH",
    "AE","AE",
    "AA","AR",
    "AH","ER1",
    "AO","AO",
    "OH","OW",
    "UH","UH",
    "UX","UW2",
    "ER","ER2",
    "EY AH","YR",
    "AX","XR",
    "IX","IH",
    "EY","EY",
    "AY","AY",
    "OY","OY",
    "AW","AW",
    "OW","OW",
    "UW","UH",
    "R","RR2",
    "L","EL",
    "W","WW",
    "WH","WW",
    "Y","YY1",
    "M","MM",
    "N","NN2",
    "NX","NG",
    "B","BB2",
    "D","DD2",
    "G","GG2",
    "Z","ZZ",
    "J","JH",
    "ZH","ZH",
    "V","VV",
    "DH","DH2",
    "S","SS",
    "SH","SH",
    "F","FF",
    "TH","TH",
    "P","PP",
    "T","TT1",
    "K","KK1",
    "CH","CH",
    "/H","HH2",
    "Q",".",
    0,0
};

const char* spo_name(int i);

#endif /* phone_names_h */
