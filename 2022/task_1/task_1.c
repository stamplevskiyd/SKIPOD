#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <stdbool.h>
#include <unistd.h>

#define PROC_COUNT 64
#define QUEUE_LENGTH 128

#define SN_TAG 1
#define MARKER_TAG 2
#define EMPTY_CELL (-1)

void pass_cs(int proc_num){
    printf("Process %d started CS\n", proc_num);
    FILE *f;
    const char *filename = "critical.txt";
    f = fopen(filename, "rb+");
    if (!f){
        printf("Process %d: file does not exist\n", proc_num);
        f = fopen(filename, "w");
        sleep(2);
        remove(filename);
    }
    else{
        printf("Process %d: file exists, ERROR\n", proc_num);
        perror("File exists");
        exit(1);
    }
    printf("Process %d left CS\n", proc_num);
};

enum states {IDLE, REQUESTS, CS, WAITING};

struct Marker{
    int LN[PROC_COUNT];
    int queue[QUEUE_LENGTH];
};

int main(int argc, char** argv){
    int numtasks, rank, Sn, finished_all, finished_current, sender;
    const int root = 0;
    bool has_marker;
    MPI_Status status;
    MPI_Request request;
    enum states current_state = REQUESTS;
    int got_message;
    struct Marker marker;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int RN[numtasks];

    /**
     * Сreating marker structure
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
     * Initialising arrays
     */
    for (int i = 0; i < numtasks; i++){
        RN[i] = 0;
    }

    for (int i = 0; i < PROC_COUNT; i++){
        marker.LN[i] = 0;
    }
    for (int i = 0; i < QUEUE_LENGTH; i++){
        marker.queue[i] = EMPTY_CELL;
    }

    has_marker = (rank == root) ? true : false;

    /**
     * Main work loop
     */
    while(true){
        fflush(stdout);
        /**
         * Getting Sn's
         */
        while(true && current_state != CS) {
            MPI_Iprobe(MPI_ANY_SOURCE, SN_TAG, MPI_COMM_WORLD, &got_message, &status);
            if (got_message) {
                sender = status.MPI_SOURCE;
                MPI_Recv(&Sn, 1, MPI_INT, MPI_ANY_SOURCE, SN_TAG, MPI_COMM_WORLD, &status);
                RN[sender] = (RN[sender] > Sn) ? RN[sender] : Sn;
                printf("Process %d got Sn = %d from process %d\n", rank, Sn, sender);
                if (has_marker && (RN[sender] == marker.LN[sender] + 1)) {
                    has_marker = false;
                    MPI_Send(&marker, 1, Marker_Datatype, sender, MARKER_TAG, MPI_COMM_WORLD);
                    printf("Process %d sent marker to %d\n", rank, sender);
                }
            }
            else
                break;
        }

        /**
         * Getting marker
         */
        MPI_Iprobe(MPI_ANY_SOURCE, MARKER_TAG, MPI_COMM_WORLD, &got_message, &status);
        if (got_message && current_state != CS){
            sender = status.MPI_SOURCE;
            MPI_Recv(&marker, 1, Marker_Datatype, MPI_ANY_SOURCE, MARKER_TAG, MPI_COMM_WORLD, &status);
            has_marker = true;
            printf("Process %d got marker from %d\n", rank, sender);
        }

        /**
         * Requesting to enter CS
         */
         if (current_state == REQUESTS){
             if (has_marker) {
                 current_state = CS;
                 pass_cs(rank);
             }
             else{
                 RN[rank]++;
                 Sn = RN[rank];
                 for (int i = 0; i < numtasks; i++) {
                     if (i != rank) {
                         MPI_Isend(&Sn, 1, MPI_INT, i, SN_TAG, MPI_COMM_WORLD, &request);
                         printf("Process %d sent Sn = %d to process %d\n", rank, Sn, i);
                     }
                 }
                 current_state = WAITING;
              }
         }

         /**
          * Request was sent, waiting for marker
          */
         else if (current_state == WAITING) {
             if (has_marker) {
                 current_state = CS;
                 pass_cs(rank);
             }
         }

        /**
         * Leaving CS
         */
        else if (current_state == CS){
            marker.LN[rank] = RN[rank];
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

            if (marker.queue[0] != EMPTY_CELL){
                int marker_receiver = marker.queue[0];
                for (int i = 0; i < QUEUE_LENGTH - 1; i++){
                    marker.queue[i] = marker.queue[i + 1];
                }
                marker.queue[QUEUE_LENGTH - 1] = EMPTY_CELL;
                has_marker = false;
                MPI_Send(&marker, 1, Marker_Datatype, marker_receiver, MARKER_TAG, MPI_COMM_WORLD);
                printf("Process %d sent marker to process %d\n", rank, marker_receiver);
            }
            current_state = IDLE;
        }

        /**
         * Checking if all processes finished CS
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