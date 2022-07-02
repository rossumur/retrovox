#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "reciter.h"
#include "sam.h"
#include "debug.h"

void PrintUsage()
{
    printf("usage: sam [options] Word1 Word2 ....\n");
    printf("options\n");
    printf("    -phonetic         enters phonetic mode. (see below)\n");
    printf("    -pitch number        set pitch value (default=64)\n");
    printf("    -speed number        set speed value (default=72)\n");
    printf("    -throat number        set throat value (default=128)\n");
    printf("    -mouth number        set mouth value (default=128)\n");
    printf("    -wav filename        output to wav instead of libsdl\n");
    printf("    -sing            special treatment of pitch\n");
    printf("    -debug            print additional debug messages\n");
    printf("\n");


    printf("     VOWELS                            VOICED CONSONANTS    \n");
    printf("IY           f(ee)t                    R        red        \n");
    printf("IH           p(i)n                     L        allow        \n");
    printf("EH           beg                       W        away        \n");
    printf("AE           Sam                       W        whale        \n");
    printf("AA           pot                       Y        you        \n");
    printf("AH           b(u)dget                  M        Sam        \n");
    printf("AO           t(al)k                    N        man        \n");
    printf("OH           cone                      NX       so(ng)        \n");
    printf("UH           book                      B        bad        \n");
    printf("UX           l(oo)t                    D        dog        \n");
    printf("ER           bird                      G        again        \n");
    printf("AX           gall(o)n                  J        judge        \n");
    printf("IX           dig(i)t                   Z        zoo        \n");
    printf("                       ZH       plea(s)ure    \n");
    printf("   DIPHTHONGS                          V        seven        \n");
    printf("EY           m(a)de                    DH       (th)en        \n");
    printf("AY           h(igh)                        \n");
    printf("OY           boy                        \n");
    printf("AW           h(ow)                     UNVOICED CONSONANTS    \n");
    printf("OW           slow                      S         Sam        \n");
    printf("UW           crew                      Sh        fish        \n");
    printf("                                       F         fish        \n");
    printf("                                       TH        thin        \n");
    printf(" SPECIAL PHONEMES                      P         poke        \n");
    printf("UL           sett(le) (=AXL)           T         talk        \n");
    printf("UM           astron(omy) (=AXM)        K         cake        \n");
    printf("UN           functi(on) (=AXN)         CH        speech        \n");
    printf("Q            kitt-en (glottal stop)    /H        a(h)ead    \n");
}

void sam_raw(unsigned char s);

int debug = 0;
int sam_main(int argc, const char **argv)
{
    int i;
    int phonetic = 0;

    char input[256] = {0};

    if (argc <= 1)
    {
        PrintUsage();
        return 1;
    }

    i = 1;
    while(i < argc)
    {
        if (argv[i][0] != '-')
        {
            strncat(input, argv[i], 255);
            strncat(input, " ", 255);
        } else
        {
            if (strcmp(&argv[i][1], "sing")==0)
            {
                EnableSingmode();
            } else
            if (strcmp(&argv[i][1], "phonetic")==0)
            {
                phonetic = 1;
            } else
            if (strcmp(&argv[i][1], "debug")==0)
            {
                debug = 1;
            } else
            if (strcmp(&argv[i][1], "pitch")==0)
            {
                SetPitch(atoi(argv[i+1]));
                i++;
            } else
            if (strcmp(&argv[i][1], "speed")==0)
            {
                SetSpeed(atoi(argv[i+1]));
                i++;
            } else
            if (strcmp(&argv[i][1], "mouth")==0)
            {
                SetMouth(atoi(argv[i+1]));
                i++;
            } else
            if (strcmp(&argv[i][1], "throat")==0)
            {
                SetThroat(atoi(argv[i+1]));
                i++;
            } else
            {
                PrintUsage();
                return 1;
            }
        }
        i++;
    } //while

    for(i=0; input[i] != 0; i++)
        input[i] = toupper((int)input[i]);
    //printf("{\"input\": \"%s\"}\n", input);

    if (debug)
    {
        if (phonetic) printf("phonetic input: %s\n", input);
        else printf("text input: %s\n", input);
    }

    if (!phonetic)
    {
        strncat(input, "[", 256);
        //dump_js("text input",input,256);
        if (!TextToPhonemes((unsigned char *)input))
            return 1;
        for (i = 0; (unsigned char)input[i] != 0x9B; i++)
            ;
        input[i] = 0;
        //printf("{\"phonemes\": \"%s\"}\n", input);
        input[i] = 0x9B;
    } else
        strncat(input, "\x9b", 256);

    SetInput(input);
    if (!SAMMain())
    {
        PrintUsage();
        return 1;
    }

    for (int i = 0; i < 22050; i++) // 1 second silence
        sam_raw(0);
    return 0;
}

