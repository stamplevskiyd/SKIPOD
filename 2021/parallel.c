#include <stdio.h>
#include <omp.h>

#define F(x) 4.0 / (1.0 + x * x)  // вычисляет пи. для проверки корректности работы. Точность более-менее
#define A 0.0
#define B 1.0
#define NT 8

int main ()
{
    int segnum = 1000, i;
    double res = 0.0, x_l, x_r, x_mid;
    double step_size = (B - A) / (double) segnum; // на каждом сегменте есть промежуточная точка в середине. эти точки за границы сегмента не считаем
    #pragma omp parallel default (none) private (i, x_l, x_r, x_mid) shared (segnum, step_size) reduction(+: res) //num_treads(NT)
    {
        #pragma omp for schedule (static)
        for (i = 0; i < segnum; i++)
        {
            x_l = A + step_size * (double) i;
            x_mid = A + step_size * (double) (i + 0.5);
            x_r = A + step_size * (double) (i + 1);
            res += step_size/6.0 * (F(x_l) + 4.0 * F(x_mid) + F(x_r));
        }
    }
    printf("%.20f", res);
    return 0;
}
