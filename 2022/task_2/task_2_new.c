#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <mpi-ext.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#define KILLED_PROCESS 3
#define SEG_COUNT 262144

#define F(x) (cos(pow(x, 1.04) * atan(5.2 * exp(x + 2.3))) + log(fabs(pow(x, 4.3) + asin(-0.4)*pow(1.0013, 13.001))) + atan(acos(1/x) + 7)) / 100
#define A 2.0
#define B 200

int rank, numprocs;

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

unsigned error_occured = 0;
MPI_Comm main_comm;

static void err_handler(MPI_Comm *comm, int *err, ...) {
    int len;
    char errstr[MPI_MAX_ERROR_STRING];
    MPI_Comm_rank(main_comm, &rank);
    MPI_Comm_size(main_comm, &numprocs);
    MPI_Error_string(*err, errstr, &len);
    printf("Rank %d / %d: notified of error %s\n", rank, numprocs, errstr);

    // создаем новый коммуникатор
    MPIX_Comm_shrink(main_comm, &main_comm);
    // процесс узнает новое кол-во процессов и свой номер
    MPI_Comm_rank(main_comm, &rank);
    MPI_Comm_size(main_comm, &numprocs);
}

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    main_comm = MPI_COMM_WORLD;

    ///Устанавливаем обработчик ошибок
    MPI_Errhandler errh;
    MPI_Comm_create_errhandler(err_handler, &errh);
    MPI_Comm_set_errhandler(main_comm, errh);
    MPI_Barrier(main_comm);

    double l = A, r = B;
    double h;
    long int local_segnum;
    double local_res, local_r, local_l;
    double res;
    double timer;

    int ret_code;
    timer = MPI_Wtime();

    if (rank == KILLED_PROCESS) {
        printf("Process %d died\n", rank);
        raise(SIGKILL);
    }

    ///Начало вычислений
    start_count:

    h = (r - l) / numprocs;
    local_segnum = SEG_COUNT / numprocs; // пусть будет поровну у каждого процесса
    local_l = l + rank * h;
    local_r = local_l + h;
    local_res = integral(local_l, local_r, local_segnum);

    ///Проверяем, все ли до сюда дожили
    ret_code = MPI_Barrier(main_comm);
    if (ret_code != MPI_SUCCESS){
        printf("%d processes alive\n", numprocs);
        goto start_count;
    }

    ///До этого момента точно дожили все процессы, собираем данные и печатаем ответ
    MPI_Reduce(&local_res, &res, 1, MPI_DOUBLE, MPI_SUM, 0,main_comm); // складываем все результаты и передаем процессу 0
    timer = MPI_Wtime() - timer;
    if (rank == 0) {
        printf("Processes number: %d, execution time, %.6f, result: %.10f\n", numprocs, timer, res);
    }

    MPI_Finalize();
    return 0;
}