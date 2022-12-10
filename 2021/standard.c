#include <stdio.h>
#include <math.h>

//s(a, b) = [(b - a) / 6] * [f(a) + 4 * f((a + b) / 2) + f(b)]

#define SEGNUM 4096
#define A 0.0
#define B 10.0
#define SEGLEN (B - A) / SEGNUM
#define F(x) exp(x)

double integral(double left, double right)  // считает интеграл на одном сегменте методом Симпсона
{
    return SEGLEN/6.0 * (F(left) + 4.0 * F(0.5 * (left + right)) + F(right));
}

int main() {
    double result = 0.0;
    int i;
    for (i = 0; i < SEGNUM; i++)
        result += integral(A + i * SEGLEN, A + (i + 1) * SEGLEN);
    printf("%.20f", result);
    return 0;
}
