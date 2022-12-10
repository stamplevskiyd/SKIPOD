#include <stdio.h>
#include <mpi.h>
#include <math.h>
#include <stdlib.h>

#define F(x) (cos(pow(x, 1.04) * atan(5.2 * exp(x + 2.3))) + log(fabs(pow(x, 4.3) + asin(-0.4)*pow(1.0013, 13.001))) + atan(acos(1/x) + 7)) / 100
#define A 2.0
#define B 200

double integral(double l, double r, int segnum) // вычисляет интеграл с заданным числом сегментов
{
    double res = 0, F_l, F_r, F_mid;
    double step_size = (r - l) / (double) segnum; // на каждом сегменте есть промежуточная точка в середине. эти точки за границы сегмента не считаем
    int i;
    double x_l = l, x_r = x_l + step_size, x_mid = x_l + (step_size) / 2.0;
    for (i = 0; i < segnum; i++)
    {
        F_l = F(x_l);
        F_r = F(x_r);
        F_mid = F(x_mid);
        res += F_r + 4.0 * F_mid + F_l;
        x_l += step_size;
        x_mid += step_size;
        x_r += step_size;
    }
    res *= step_size / 6.0; //возможно, увеличит точность, так как не будем каждый раз делить результат и складывать очень маленькие числа (риск переполнения очень мал)
    return res;
}


int main (int argc, char **argv)
{
    int rank; // ранг текущего процесса
    int numprocs; // общее число проессов
    double l = A, r = B;
    unsigned long int Segment_Nums[10] = {262144, 524288, 1048576, 2097152, 4194304, 8388608, 16777216, 33554432, 67108864, 134217728};
    double h;
    double x_l, x_r; // локальные l и r
    long int local_segnum;
    double local_res, local_r, local_l;
    double res;
    double timer;
    int i;

    MPI_Init(&argc, &argv); // начало работы
    for (i = 0; i < 10; i++) {
        timer = MPI_Wtime();
        MPI_Comm_rank(MPI_COMM_WORLD, &rank); // получаем номер текущего процесса
        MPI_Comm_size(MPI_COMM_WORLD, &numprocs); // получение общего числа запущенных процессов

        MPI_Bcast(&l, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD); // процесс 0 рассылает данные
        MPI_Bcast(&r, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        MPI_Bcast(&Segment_Nums[i], 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD); // синхронизация процессов
        h = (r - l) / numprocs;
        local_segnum = Segment_Nums[i] / numprocs; // пусть будет поровну у каждого процесса
        local_l = l + rank * h;
        local_r = local_l + h;
        local_res = integral(local_l, local_r, local_segnum);
        MPI_Reduce(&local_res, &res, 1, MPI_DOUBLE, MPI_SUM, 0,
                   MPI_COMM_WORLD); // складываем все результаты и передаем процессу 0
        MPI_Barrier(MPI_COMM_WORLD);
        timer = MPI_Wtime() - timer;
        if (rank == 0) {
            printf("Processes number: %d, Segment number: %ld, execution time, %.6f, result: %.10f\n", numprocs, Segment_Nums[i],
                   timer, res);
        }
    }
    MPI_Finalize();
    return 0;
}
