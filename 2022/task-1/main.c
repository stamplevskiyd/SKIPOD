#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <time.h>
#include <stdbool.h>

#define PROC_COUNT 64

/**
 * Маркер
 */
struct Marker{
    /**
     * Массив с номерами последних удовлетворенных процессов
     */
    int LN[PROC_COUNT];

    /**
     * Очередь запросов
     * В идеале vector, но пока и так сойдет
     */
    int request_queue[4 * PROC_COUNT];

    /**
     * Номер последнего запроса в очереди
     */
    int last_request_num;
};

int main(int argc, char** argv){
    int numtasks, rank;
    const int root = 0;
    MPI_Status status;

    ///Скорее всего поменять константу, эти величины не связаны
    int RN[PROC_COUNT];  //Порядок запросов

    int Sn = 0; // Номер запроса
    struct Marker marker;
    int marker_owner; // ранг текущего владельца маркера
    int tag;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    for (int i = 0; i < numtasks; i++){
        marker.LN[i] = 0;  //  очередь пока пустая
        RN[i] = 0;  // запросы этого процесса
    }

    for (int i = 0; i < 4 * PROC_COUNT; i++){
        marker.request_queue[i] = -1;
    }
    marker.last_request_num = 0;

    ///Выбор владельца маркера
    srand(time(0));
    if (rank == root){
        marker_owner = rand() % numtasks;
    }
    MPI_Bcast(&marker_owner, 1, MPI_INT, root, MPI_COMM_WORLD);

    printf("Маркер у %d, я процесс номер %d\n", marker_owner, rank);
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
    return 0;
}