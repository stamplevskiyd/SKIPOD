#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>

#define PROC_COUNT 64
#define QUEUE_LENGTH 128

/**
 * Выполнить критическую секцию
 */
void pass_cs(){
        ;
        if (false) {
            ;
            ;
        }
        else{
            ;
            sleep(1);
            ;
        }
};

/**
 * Возможные состояния процесса
 */
enum states {IDLE, REQUESTS, CS, WAITING};

/**
 * Структура под маркер
 */
struct Marker{
    int LN[PROC_COUNT];
    int queue[QUEUE_LENGTH];
};

int main(int argc, char** argv){
    int numtasks, rank;
    int marker_owner;
    int Sn;
    const int root = 0;
    MPI_Status status;
    MPI_Request request = MPI_REQUEST_NULL;
    enum states current_state = REQUESTS;
    int got_message, elem_count;
    struct Marker marker;
    int RN[numtasks];
    int finished_all;
    int finished_current;
    bool has_marker;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    /**
     * Создание структуры для маркера
     */
    const int nitems = 2;
    int blocklengths[2] = {PROC_COUNT, QUEUE_LENGTH};
    MPI_Datatype Marker_Datatype;
    MPI_Datatype types[2] = {MPI_INT, MPI_INT};
    MPI_Aint offsets[2], address, start_address;
    MPI_Get_address(&marker, &start_address);
    offsets[0] = (MPI_Aint) 0;
    MPI_Get_address(&marker.queue, &address);
    offsets[1] = address - start_address;
    MPI_Type_create_struct(2, blocklengths, offsets, types, &Marker_Datatype);
    MPI_Type_commit(&Marker_Datatype);

    /**
     * Инициалзация RN
     * (номера последних запросов каждого процесса)
     */
    for (int i = 0; i < numtasks; i++){
        RN[i] = 0;
    }

    /**
     * Инициализация маркера
     */
    for (int i = 0; i < PROC_COUNT; i++){
        marker.LN[i] = 0;
    }
    for (int i = 0; i < QUEUE_LENGTH; i++){
        marker.queue[i] = 0;
    }

    /**
     * Выбор владельца маркера
     */
    if (rank == root){
        srand(time(0));
        marker_owner = rand() % numtasks;
    }
    MPI_Bcast(&marker_owner, 1, MPI_INT, root, MPI_COMM_WORLD);
    printf("Маркер у %d, я процесс номер %d\n", marker_owner, rank);
    has_marker = (rank == marker_owner);
    MPI_Barrier(MPI_COMM_WORLD); // ожидаем завершения инициализации всех процессов

    /**
     * Основной этап работы
     */
    while(true){
        sleep(1);
        /**
         * Получение сообщений
         */
        MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &got_message, &status);

        if (got_message){
            printf("Процесс %d получил сообщение от процесса %d\n", rank, status.MPI_SOURCE);
            MPI_Get_count(&status, MPI_INT, &elem_count);

            /**
             * Получено Sn
             */
            if (elem_count == 1){
                MPI_Recv(&Sn, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
                RN[status.MPI_SOURCE] = (RN[status.MPI_SOURCE] > Sn) ? RN[status.MPI_SOURCE] : Sn;
                printf("Процесс %d получил Sn = %d от процесса %d\n", rank, Sn, status.MPI_SOURCE);
                if (has_marker){
                    if (RN[status.MPI_SOURCE] == marker.LN[status.MPI_SOURCE] + 1){
                        has_marker = false;
                        MPI_Send(&marker, 1, Marker_Datatype, status.MPI_SOURCE, 1, MPI_COMM_WORLD);
                        printf("Процесс %d отправил маркер процессу %d\n", rank, status.MPI_SOURCE);
                    }
                }
            }
            /**
             * Получен маркер
             */
            else{
                MPI_Recv(&marker, 1, Marker_Datatype, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
                has_marker = true;
                printf("Процесс %d получил маркер от процесса %d\n", rank, status.MPI_SOURCE);
            }
        }

        /**
         * Запрошен вход в критическую секцию
         */
        if (current_state == REQUESTS){
            /**
             * Есть маркер
             */
            if (has_marker){
                printf("Процесс %d начал критическую секцию\n", rank);
                current_state = CS;
                pass_cs();
                marker.LN[rank] = RN[rank];
                /**
                 * Добавление процессов в очередь
                 */
                for (int i = 0; i < numtasks; i++){
                    if (RN[i] == marker.LN[i] + 1){
                        int j = 0;
                        while (true){
                            if (marker.queue[j] == i)
                                break;
                            else if (marker.queue[j] != 0)
                                j++;
                            else{
                                marker.queue[j] = i;
                                break;
                            }
                        }
                    }
                }

                /**
                 * Очередь непуста, передаем маркер
                 */
                if (marker.queue[0] != 0){
                    int marker_receiver = marker.queue[0];
                    for (int i = 0; i < QUEUE_LENGTH - 1; i++){
                        marker.queue[i] = marker.queue[i + 1];
                    }
                    has_marker = false;
                    MPI_Send(&marker, 1, Marker_Datatype, marker_receiver, 1, MPI_COMM_WORLD);
                    printf("Процесс %d отправил маркер процессу %d\n", rank, status.MPI_SOURCE);
                }
                current_state = IDLE;
                printf("Процесс %d закончил критическую секцию\n", rank);
            }

            /**
             * Нет маркера
             */
            else{
                RN[rank]++;
                Sn = RN[rank];
                for (int i = 0; i < numtasks; i++){
                    if (i != rank) {
                        MPI_Send(&Sn, 1, MPI_INT, i, 1, MPI_COMM_WORLD);
                        printf("Процесс %d послал Sn = %d процессу %d\n", rank, Sn, i);
                    }
                }
            }
        }

        finished_current = (current_state == IDLE) ? 1 : 0;
        MPI_Allreduce(&finished_current, &finished_all, 1, MPI_INT, MPI_SUM,MPI_COMM_WORLD);
        if (finished_all == numtasks)
            break;
    }
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
    return 0;
}