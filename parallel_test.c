#include <stdio.h>
#include <omp.h>
#include <math.h>
#include <stdlib.h>

#define F(x) cos(pow(x, 1.04) * atan(5.2 * exp(x + 2.3))) + log(fabs(pow(x, 4.3) + asin(-0.4)*pow(1.0013, 13.001))) + atan(acos(1/x) + 7)
#define A 2.0
#define B 5.0

int main (int argc, char **argv)
{
    int segnum = 1000, NT = 8, i;
    if (argc == 3)
    {
        segnum = atoi(argv[1]); // чтобы не перекомпилировать при каждом изменении данных
        NT = atoi(argv[2]);
    }
    double res = 0.0, x_l, x_r, x_mid;
    double step_size = (B - A) / (double) segnum; // на каждом сегменте есть промежуточная точка в середине. эти точки за границы сегмента не считаем
    #pragma omp parallel default (none) private (i, x_l, x_r, x_mid) shared (segnum, step_size) reduction(+: res) //num_treads(NT)
    {
        #pragma omp for schedule (static) // распределит нагрузку на нити примерно поровну. кажется оптимальным
        for (i = 0; i < segnum; i++)
        {
            x_l = A + step_size * (double) i;
            x_mid = A + step_size * (double) (i + 0.5);
            x_r = A + step_size * (double) (i + 1);
            res += (F(x_l) + 4.0 * F(x_mid) + F(x_r));
        }
    }
    res *= step_size / 6.0; //немного уменьшим число вычислений. заодно увеличив точность (риск переполнения очень мал)
    printf("%.20f", res);
    return 0;
}
