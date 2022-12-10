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
    int tag, marker_owner, Sn;
    const int root = 0;
    MPI_Status status;
    MPI_Request request = MPI_REQUEST_NULL;
    enum states current_state = REQUESTS;
    int got_message, elem_count;
    struct Marker marker;
    int RN[numtasks];
    int finished_all;
    int finished_current;

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
    MPI_Barrier(MPI_COMM_WORLD); // ожидаем завершения инициализации всех процессов

    /**
     * Основной этап работы
     */
    while(true){
        //sleep(1);
//        printf("\nПроцесс %d\n", rank);
//        for (int i = 0; i < numtasks; i++){
//            printf("%d ", RN[i]);
//        }
//        printf("\n");
//        for (int i = 0; i < requests_length; i++){
//            printf("%d ", marker_requests[i]);
//        }
//        printf("\n");
//        if (current_state == IDLE){
//            printf("Процесс %d, владелец маркера %d прошел секцию Да\n", rank, marker_owner);
//        }
//        else{
//            printf("Процесс %d, владелец маркера %d прошел секцию Нет\n", rank, marker_owner);
//        }
        /**
         * Получение сообщений
         */
        MPI_Iprobe(MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &got_message, &status);
        MPI_Get_count(&status, MPI_INT, &elem_count);
        printf("Получено: %d, Число элементов %d\n", got_message, elem_count);

        /**
         * Получено Sn
         */
        if (got_message && elem_count == 1){
            MPI_Recv(&Sn, 1, MPI_INT, MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &status);
            RN[status.MPI_SOURCE] = (RN[status.MPI_SOURCE] > Sn) ? RN[status.MPI_SOURCE] : Sn;
            printf("Процесс %d получил Sn = %d от процесса %d\n", rank, Sn, status.MPI_SOURCE);
            if (rank == marker_owner){
                if (RN[status.MPI_SOURCE] == marker.LN[status.MPI_SOURCE] + 1){
                    MPI_Send(&marker, 1, Marker_Datatype, status.MPI_SOURCE, tag, MPI_COMM_WORLD);
                    printf("Процесс %d отправил маркер процессу %d\n", rank, status.MPI_SOURCE);
                }
            }
        }
        /**
         * Получен маркер
         */
        else if (got_message){
            MPI_Recv(&marker, 1, Marker_Datatype, MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &status);
            marker_owner = rank;
            printf("Процесс %d получил маркер от процесса %d\n", rank, status.MPI_SOURCE);
        }

        /**
         * Запрошен вход в критическую секцию
         */
        if (current_state == REQUESTS){
            /**
             * Есть маркер
             */
            if (rank == marker_owner){
                printf("Процесс %d начал критическую секцию\n", rank);
                current_state = CS;
                pass_cs();
                current_state = IDLE;
                marker.LN[rank] = RN[rank];
                /**
                 * Добавление процессов в очередь
                 */
                for (int i = 0; i < numtasks; i++){
                    if (RN[i] == marker.LN[i] + 1){
                        int j = 0;
                        while (true){
                            if (marker.queue[j] != 0){
                                j++;
                            }
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
                    int receiver = marker.queue[0];
                    marker_owner = receiver;
                    for (int i = 0; i < QUEUE_LENGTH - 1; i++){
                        marker.queue[i] = marker.queue[i + 1];
                    }
                    MPI_Send(&marker, 1, Marker_Datatype, receiver, tag, MPI_COMM_WORLD);
                }

                printf("Процесс %d прошел критическую секцию\n", rank);
            }

            /**
             * Нет маркера
             */
            else{
                RN[rank]++;
                Sn = RN[rank];
                for (int i = 0; i < numtasks; i++){
                    if (i != rank) {
                        MPI_Send(&Sn, 1, MPI_INT, i, tag, MPI_COMM_WORLD);
                    }
                }
                printf("Процесс %d разослал Sn = %d всем\n", rank, Sn);
            }
        }

        finished_current = (current_state == IDLE) ? 1 : 0;
        MPI_Reduce(&finished_current, &finished_all, 1, MPI_INT, MPI_SUM, 0,MPI_COMM_WORLD);
        printf("\nЧисло отработавших процессов %d\n", finished_current);
        if (finished_all == numtasks)
            break;
    }
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
    return 0;
}