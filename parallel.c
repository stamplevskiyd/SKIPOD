#include <stdio.h>
#include <math.h>
#include <omp.h>


#define SEGNUM 1677216 // 2^24
#define A 1.0
#define B 10.0
#define F(x) x//fabs((log10(sqrt(exp(x+1.4) * 1.12 / (x*6.8*exp(5.2)))) * fabs(sin(atan(x*x*x-4.3*x))) - acos(cos(x-7.9)) / x + log(fabs(pow(atan(x), pow(1.12, exp(1)))))) / pow(2, exp(2*x / 101.3)))

int main() {
    int i;
    double result = 0.0, seglen = (B - A) / SEGNUM;
    double l, r, mid;
    #pragma omp parallel default(none) private(i, l, r, mid) shared(seglen) reduction(+: result)
    {
        int id = omp_get_thread_num();
        int numt = omp_get_num_threads();
        for (i = id + 1; i < SEGNUM; i += numt)
        {
            l = A + i * seglen;
            r = A + (i + 1) * seglen;
            mid = (l + r) / 2.0;
            result += seglen/6.0 * (F(l) + 4.0 * F(mid) + F(r));
        }

    }
    printf("%.20f", result);
    return 0;
}

