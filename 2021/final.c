#include <stdio.h>
#include <omp.h>
#include <math.h>
#include <stdlib.h>

#define F(x) (cos(pow(x, 1.04) * atan(5.2 * exp(x + 2.3))) + log(fabs(pow(x, 4.3) + asin(-0.4)*pow(1.0013, 13.001))) + atan(acos(1/x) + 7)) / 100
#define A 2.0
#define B 200

double omp_get_wtime(void);

void Integral(long int segnum, int NT)
{
    double start, end;
    int i;
    double res = 0.0, x_l, x_r, x_mid;
    double step_size = (B - A) / (double) segnum; // на каждом сегменте есть промежуточная точка в середине. эти точки за границы сегмента не считаем
    start = omp_get_wtime(); // будем считать время только подсчета интеграла, без подсчета шага-входных данных (это время очень мало все равно)
    #pragma omp parallel default (none) num_threads(NT) private (i, x_l, x_r, x_mid) shared (segnum, step_size) reduction(+: res)
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
    res *= step_size / 6.0; //немного уменьшим число вычислений. заодно увеличив точность, так как не будем каждый раз значительно уменьшать результат (риск переполнения очень мал)
    end = omp_get_wtime();
    printf("Thread number: %d, segment number: %ld, execution time, %.6f, result: %.10f\n", NT, segnum, end - start, res);
}

int main (int argc, char **argv)
{
    int i, j;
    int NT = atoi(argv[1]); 
    unsigned long int Segment_Nums[10] = {262144, 524288, 1048576, 2097152, 4194304, 8388608, 16777216, 33554432, 67108864, 134217728};
    for (i = 0; i < 10; i++)
        Integral(Segment_Nums[i], NT);
    return 0;
}
