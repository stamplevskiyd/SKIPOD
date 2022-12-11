#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>

#define PROC_COUNT 64
#define QUEUE_LENGTH 128

#define SN_TAG 1
#define MARKER_TAG 2
#define EMPTY_CELL (-1)

/**
 * Выполнить критическую секцию
 */
void pass_cs(){
        srand(0);
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
    int numtasks, rank, Sn, finished_all, finished_current, sender;
    const int root = 0;
    bool has_marker;
    MPI_Status status;
    enum states current_state = REQUESTS;
    int got_message;
    struct Marker marker;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int RN[numtasks];

    /**
     * Создание структуры для маркера
     */
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
        marker.queue[i] = EMPTY_CELL;
    }

    has_marker = (rank == root) ? true : false;

    /**
     * Основной этап работы
     */
    while(true){
        //sleep(1);
//        if (has_marker){
//            for (int i = 1; i < 10; i++)
//                printf("%d ", marker.queue[i]);
//            printf("\n");
//        }

        /**
         * Получение всех Sn
         */
        while(true) {
            MPI_Iprobe(MPI_ANY_SOURCE, SN_TAG, MPI_COMM_WORLD, &got_message, &status);
            if (got_message) {
                sender = status.MPI_SOURCE;
                MPI_Recv(&Sn, 1, MPI_INT, MPI_ANY_SOURCE, SN_TAG, MPI_COMM_WORLD, &status);
                RN[sender] = (RN[sender] > Sn) ? RN[sender] : Sn;
                printf("Процесс %d получил Sn = %d от процесса %d\n", rank, Sn, sender);
                if (has_marker && (RN[sender] == marker.LN[sender] + 1)) {
                    has_marker = false;
                    MPI_Send(&marker, 1, Marker_Datatype, sender, MARKER_TAG, MPI_COMM_WORLD);
                    printf("Процесс %d отправил маркер процессу %d\n", rank, sender);
                }
            }
            else
                break;
        }

        /**
         * Получение маркера
         */
        MPI_Iprobe(MPI_ANY_SOURCE, MARKER_TAG, MPI_COMM_WORLD, &got_message, &status);
        if (got_message){
            sender = status.MPI_SOURCE;
            MPI_Recv(&marker, 1, Marker_Datatype, MPI_ANY_SOURCE, MARKER_TAG, MPI_COMM_WORLD, &status);
            has_marker = true;
            printf("Процесс %d получил маркер от процесса %d\n", rank, sender);
        }

        /**
         * Запрос входа в критическую секцию
         */
         if (current_state == REQUESTS){
             if (has_marker) {
                 printf("Процесс %d начал критическую секцию\n", rank);
                 current_state = CS;
                 pass_cs();
             }
             else{
                 RN[rank]++;
                 Sn = RN[rank];
                 for (int i = 0; i < numtasks; i++) {
                     if (i != rank) {
                         MPI_Send(&Sn, 1, MPI_INT, i, SN_TAG, MPI_COMM_WORLD);
                         printf("Процесс %d послал Sn = %d процессу %d\n", rank, Sn, i);
                     }
                 }
                 current_state = WAITING;
              }
         }

         /**
          * Запрос отправлен, ждем маркер
          */
         else if (current_state == WAITING) {
             if (has_marker) {
                 printf("Процесс %d начал критическую секцию\n", rank);
                 current_state = CS;
                 pass_cs();
             }
         }

        /**
         * Выход из критической секции
         */
        else if (current_state == CS){
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
                        else if (marker.queue[j] != EMPTY_CELL)
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
            if (marker.queue[0] != EMPTY_CELL){
                int marker_receiver = marker.queue[0];
                for (int i = 0; i < QUEUE_LENGTH - 1; i++){
                    marker.queue[i] = marker.queue[i + 1];
                }
                marker.queue[QUEUE_LENGTH - 1] = EMPTY_CELL;
                has_marker = false;
                MPI_Send(&marker, 1, Marker_Datatype, marker_receiver, MARKER_TAG, MPI_COMM_WORLD);
                printf("Процесс %d отправил маркер процессу %d\n", rank, status.MPI_SOURCE);
            }
            current_state = IDLE;
            printf("Процесс %d закончил критическую секцию\n", rank);
        }

        /**
         * Проверяем, все ли прошли критическую секцию
         */
        finished_current = (current_state == IDLE) ? 1 : 0;
        MPI_Allreduce(&finished_current, &finished_all, 1, MPI_INT, MPI_SUM,MPI_COMM_WORLD);
        if (finished_all == numtasks)
            break;
    }
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
    return 0;
}