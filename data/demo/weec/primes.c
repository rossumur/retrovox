#include "stdio.h"

int primes(int limit)
{
	int count,n,k,p;
	n = 1;
	count = 0;
	while (n < limit) {
		k=3;
		p=1;
		n=n+2;
		while ((k*k<=n) && p)
        {
			p=n/k*k!=n;
			k=k+2;
		}
		if (p) {
			printf("%d is prime \n",n);
			count++;
		}
	}
    return count;
}

int main()
{
	int n;
    n = 1024;
    printf("Primes to %d:\n",n);
	printf("Primes found: %d\n",primes(n));
	return 0;
}
