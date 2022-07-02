
#include "stdlib.h"
#include "stdio.h"

// get an int!
int getnum()
{
    int c,d,i;
    d = 0;
    while ((c = getchar()) != '\n')
    {
        if (c >= '0' && c <= '9')
            d = d*10 + c-'0';
        else if (c == '\b')
            d = d/10;
    }
    return d;
}

int INPUT()
{
    printf("? ");
    return getnum();
}

int RND(int i)
{
    return 1 + rand() % i;
}

int A,B,D,E,F,G,I,L,M,P,R,S,T,V,Y;
void init()
{
    D=0;E=0;G=2800;I=5;L=1000;P=100;R=200;T=1;Y=3;
    printf("Try your hand at\ngoverning Sumeria\nfor ten year term!\n\n");
}

void report()
{
    //printf("T:%d D:%d I:%d Y:%d R:%d P:%d L:%d G:%d\n",T,D,I,Y,R,P,L,G);
    printf("\nHammurabi,\nI beg to report to you;\n");
    printf("In year %d, %d people\nstarved and %d came to\nthe city.\n",T,D,I);
    printf("You harvested %d bushels\nof grain per acre.\n",Y);
    printf("Rats destroyed %d\nbushels\n",R);
    printf("Population is now %d,\nand the city owns\n%d acres of land\n",P,L);
    printf("You have %d bushels\nof grain in store.\n",G);
}

int terrible()
{
    printf("\nYour performance has\nbeen so terrible that\n");
    printf("you were driven from\nyour throne after only\n%d years!\n",T);
    return -1;
}

int best()
{
    printf("\nYour expert\nstatesmanship is worthy\nof Hammurabi himself!\n");
    printf("The city will honor\nyour memory for all\nof eternity!\n");
    return 2;
}

int average()
{
    printf("\nYour competent rule is\nappreciated by your\ncitizens.\n");
    printf("They will remember you\nfondly for some time\nto come.\n");
    return 1;
}

int poor()
{
    printf("\nYour mismanagement left\nyour city in a very\npoor state.\n");
    printf("Your incompetence and\noppression will not be\nmissed by your people.\n");
    return 0;
}

//int hammurabi()
int main()
{
    init();
    while (T < 10) {
        report();
        if (D>P*45/100)
            break;
        
        V = 16+RND(10);
        printf("Land is trading at %d\nbushels per acre.\n",V);
        
        do {
            printf("How many acres do you\nwish to buy\n(0-%d)",G/V);
            B = INPUT();
        } while (B>G/V);
        
        if (B <= 0) {
            do {
                printf("How many acres do you\nwish to sell\n(0-%d)",L);
                B = INPUT();
            } while (B>L);
            B = -B;
        }
        
        do {
            printf("How many bushels to\nfeed the people\n(0-%d)",G-B*V);
            F = INPUT();
        } while (F > G-B*V);
        
        M= G-F-B*V;
        if (10*P<M) M=10*P;
        if (L+B<M) M=L+B;
        
        do {
            printf("How many acres do you\nwish to plant\n(0-%d)",M);
            S = INPUT();
        } while (S>M);
        
        // Work out the result
        L=L+B;
        G=G-B*V-F-S/2;
        
        // Yield
        Y=RND(5);
        
        // Rats
        R=0;
        if (RND(5) == 1 && G)
            R=RND(G);   // brutal
        
        // Recalculate grain
        G=G+S*Y-R;
        if (G<0) G=0;
        
        // Immigration/Birth
        A=RND(5);
        I=A*(L+S/20)/P/5+1;
        
        // Feeding the people
        D=0;
        if (!(P<=F/20)) {       // deaths
            D=P-F/20;
            E=((T-1)*E+(100*D/P))/(T+1);
        }
        P=P-D+I;
        
        if (P<=0) {
            printf("\nYou have starved\nyour entire kingdom\nto death!\n");
            break;
        }
        T = T + 1;
    }
    // Evaluation!
    printf("\nYour reign ends\nafter %d years.\n",T-1);
    printf("You leave your city\nwith %d people.\n",P);
    printf("You have %d acres\nof land to support them.\n",L);
    printf("%d bushels of grain\nremain in store.\n",G);
    
    if (E<=3 && L/P>=10) return best();
    if (E<=10 && L/P>=9) return average();
    if (E<=33 && L/P>=7) return poor();
    return terrible();
}
