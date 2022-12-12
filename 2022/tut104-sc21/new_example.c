#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <math.h>
#include <mpi.h>
#include <mpi-ext.h>
#include <time.h>


int rank = MPI_PROC_NULL, verbose = 1; /* makes this global (for printfs) */
char **gargv;

int MPIX_Comm_replace(MPI_Comm comm, MPI_Comm *newcomm) {
    MPI_Comm icomm, /* the intercomm between the spawnees and the old (shrinked) world */
    scomm, /* the local comm for each sides of icomm */
    mcomm; /* the intracomm, merged from icomm */
    MPI_Group cgrp, sgrp, dgrp;
    int rc, flag, rflag, i, nc, ns, nd, crank, srank, drank;

    redo:
    if (comm == MPI_COMM_NULL) { /* am I a new process? */
        /* I am a new spawnee, waiting for my new rank assignment
         * it will be sent by rank 0 in the old world */
        MPI_Comm_get_parent(&icomm);
        scomm = MPI_COMM_WORLD;
        MPI_Recv(&crank, 1, MPI_INT, 0, 1, icomm, MPI_STATUS_IGNORE);
        if (verbose) {
            MPI_Comm_rank(scomm, &srank);
            printf("Spawnee %d: crank=%d\n", srank, crank);
        }
    } else {
        /* I am a survivor: Spawn the appropriate number
         * of replacement processes (we check that this operation worked
         * before we process further) */
        /* First: remove dead processes */
        MPIX_Comm_shrink(comm, &scomm);
        MPI_Comm_size(scomm, &ns);
        MPI_Comm_size(comm, &nc);
        nd = nc - ns; /* number of deads */
        if (0 == nd) {
            /* Nobody was dead to start with. We are done here */
            MPI_Comm_free(&scomm);
            *newcomm = comm;
            return MPI_SUCCESS;
        }
        /* We handle failures during this function ourselves... */
        MPI_Comm_set_errhandler(scomm, MPI_ERRORS_RETURN);

        rc = MPI_Comm_spawn(gargv[0], &gargv[1], nd, MPI_INFO_NULL,
                            0, scomm, &icomm, MPI_ERRCODES_IGNORE);
        flag = (MPI_SUCCESS == rc);
        MPIX_Comm_agree(scomm, &flag);
        if (!flag) {
            if (MPI_SUCCESS == rc) {
                MPIX_Comm_revoke(icomm);
                MPI_Comm_free(&icomm);
            }
            MPI_Comm_free(&scomm);
            if (verbose) fprintf(stderr, "%04d: comm_spawn failed, redo\n", rank);
            goto redo;
        }

        /* remembering the former rank: we will reassign the same
         * ranks in the new world. */
        MPI_Comm_rank(comm, &crank);
        MPI_Comm_rank(scomm, &srank);
        /* the rank 0 in the scomm comm is going to determine the
         * ranks at which the spares need to be inserted. */
        if (0 == srank) {
            /* getting the group of dead processes:
             *   those in comm, but not in scomm are the deads */
            MPI_Comm_group(comm, &cgrp);
            MPI_Comm_group(scomm, &sgrp);
            MPI_Group_difference(cgrp, sgrp, &dgrp);
            /* Computing the rank assignment for the newly inserted spares */
            for (i = 0; i < nd; i++) {
                MPI_Group_translate_ranks(dgrp, 1, &i, cgrp, &drank);
                /* sending their new assignment to all new procs */
                MPI_Send(&drank, 1, MPI_INT, i, 1, icomm);
            }
            MPI_Group_free(&cgrp);
            MPI_Group_free(&sgrp);
            MPI_Group_free(&dgrp);
        }
    }

    /* Merge the intercomm, to reconstruct an intracomm (we check
     * that this operation worked before we proceed further) */
    rc = MPI_Intercomm_merge(icomm, 1, &mcomm);
    rflag = flag = (MPI_SUCCESS == rc);
    MPIX_Comm_agree(scomm, &flag);
    if (MPI_COMM_WORLD != scomm) MPI_Comm_free(&scomm);
    MPIX_Comm_agree(icomm, &rflag);
    MPI_Comm_free(&icomm);
    if (!(flag && rflag)) {
        if (MPI_SUCCESS == rc) {
            MPI_Comm_free(&mcomm);
        }
        if (verbose) fprintf(stderr, "%04d: Intercomm_merge failed, redo\n", rank);
        goto redo;
    }

    /* Now, reorder mcomm according to original rank ordering in comm
     * Split does the magic: removing spare processes and reordering ranks
     * so that all surviving processes remain at their former place */
    rc = MPI_Comm_split(mcomm, 1, crank, newcomm);

    /* Split or some of the communications above may have failed if
     * new failures have disrupted the process: we need to
     * make sure we succeeded at all ranks, or retry until it works. */
    flag = (MPI_SUCCESS == rc);
    MPIX_Comm_agree(mcomm, &flag);
    MPI_Comm_free(&mcomm);
    if (!flag) {
        if (MPI_SUCCESS == rc) {
            MPI_Comm_free(newcomm);
        }
        if (verbose) fprintf(stderr, "%04d: comm_split failed, redo\n", rank);
        goto redo;
    }

    /* restore the error handler */
    if (MPI_COMM_NULL != comm) {
        MPI_Errhandler errh;
        MPI_Comm_get_errhandler(comm, &errh);
        MPI_Comm_set_errhandler(*newcomm, errh);
    }

    return MPI_SUCCESS;
}


#define  Max(a, b) ((a)>(b)?(a):(b))
#define  N   ((2<<5)+2)

int slider = 0;
double maxeps = 0.1e-7;
double A[2][N][N][N];
double receive_buf[N][N][N];

void init();

void verify();

int main(int argc, char **argv) {
    MPI_Comm world; /* a world comm for the work, w/o the spares */
    MPI_Comm rworld; /* and a temporary handle to store the repaired copy */
    int size, spare;
    int ret_code; /* error code from MPI functions */
    char estr[MPI_MAX_ERROR_STRING] = "";
    int strl; /* error messages */
    double start, tff = 0, twf = 0; /* timings */
    int was_error;

    gargv = argv;
    MPI_Init(&argc, &argv);


    /* Am I a spare ? */
    MPI_Comm_get_parent(&world);
    if (MPI_COMM_NULL == world) {
        /* First run: Let's create an initial world,
         * a copy of MPI_COMM_WORLD */
        MPI_Comm_dup(MPI_COMM_WORLD, &world);
        MPI_Comm_size(world, &size);
        MPI_Comm_rank(world, &rank);
        /* We set an errhandler on world, so that a failure is not fatal anymore. */
        MPI_Comm_set_errhandler(world, MPI_ERRORS_RETURN);
    } else {
        /* I am a spare, lets get the repaired world */
        MPIX_Comm_replace(MPI_COMM_NULL, &world);
        MPI_Comm_size(world, &size);
        MPI_Comm_rank(world, &rank);
        /* We set an errhandler on world, so that a failure is not fatal anymore. */
        MPI_Comm_set_errhandler(world, MPI_ERRORS_RETURN);
        //without goto
    }

    double time_start = MPI_Wtime();

    int line_start[size];
    int line_count[size];

    int n2 = N * N;

    for (int i = 0; i < size; i++) {
        line_count[i] = n2 / size;
    }

    for (int i = 0; i < n2 - (n2 / size) * size; i++) {
        line_count[i]++;
    }

    line_start[0] = 0;
    for (int i = 1; i < size; i++) {
        line_start[i] = line_start[i - 1] + line_count[i - 1];
    }

    if (rank == -1) { // just debug information
        for (int i = 0; i < size; i++) {
            printf("%5d", line_start[i]);
        }
        printf("\n");

        for (int i = 0; i < size; i++) {
            printf("%5d", line_count[i]);
        }
        printf("\n");
    }

    int receive_counts[size];
    int start_offset[size];
    for (int i = 0; i < size; i++) {
        receive_counts[i] = line_count[i] * N;
        start_offset[i] = line_start[i] * N;
    }

    if (rank == 0) { //init only in main process
        init();
    }

    int itmax = 100;

    srand(time(NULL));

    for (int iteration = 1; iteration < itmax; ++iteration) {
        MPI_Bcast(&iteration, 1, MPI_INT, 0, world);

        if ((rand() % 100 == 1) && rank != 0) {
            printf("Rank %04d: committing suicide\n", rank);
            raise(SIGKILL);
        }

        ret_code = MPI_Bcast(A[slider], N * N * N, MPI_DOUBLE, 0, world);;

        was_error = ret_code != MPI_SUCCESS;
        MPI_Allreduce(&was_error, &was_error, 1, MPI_INT, MPI_MAX, world);

        if (was_error) {
            printf("Bcast failed, restarting... %d\n", iteration);
            MPIX_Comm_replace(world, &rworld);
            MPI_Comm_free(&world);
            world = rworld;
            --iteration;
            continue;
        }

        double eps = 0.;
        //relax;
        int k;
        for (int iterat = line_start[rank]; iterat < line_start[rank] + line_count[rank]; iterat++) {
            int i_cof = iterat / N;
            int j_cof = iterat % N;
            if (i_cof >= 1 && i_cof <= (N - 2) && j_cof >= 1 && j_cof <= (N - 2)) {
                for (k = 1; k <= N - 2; k++) {
                    double e = A[slider][i_cof][j_cof][k];
                    A[slider ^ 1][i_cof][j_cof][k] = (A[slider][i_cof - 1][j_cof][k] + A[slider][i_cof + 1][j_cof][k]
                                                      + A[slider][i_cof][j_cof - 1][k] + A[slider][i_cof][j_cof + 1][k]
                                                      + A[slider][i_cof][j_cof][k - 1] +
                                                      A[slider][i_cof][j_cof][k + 1]) / 6.;
                    eps = Max(eps, fabs(e - A[slider ^ 1][i_cof][j_cof][k]));
                }
            }
        }

        slider ^= 1;

        ret_code = MPI_Gatherv(A[slider][line_start[rank] / N][line_start[rank] % N], line_count[rank] * N, MPI_DOUBLE,
                               receive_buf[line_start[rank] / N][line_start[rank] % N], receive_counts, start_offset,
                               MPI_DOUBLE, 0, world);

        slider ^= 1;

        memcpy(A[slider], receive_buf, sizeof(double) * N * N * N);

        was_error = ret_code != MPI_SUCCESS;
        MPI_Allreduce(&was_error, &was_error, 1, MPI_INT, MPI_MAX, world);

        if (was_error) {
            printf("Gatherv failed, restarting... %d\n", iteration);
            MPIX_Comm_replace(world, &rworld);
            MPI_Comm_free(&world);
            world = rworld;
            --iteration;
            continue;
        }

        if (rank == 0) {  // just debug information
            printf("it=%4i slider = %3d \n", iteration, slider);
            verify();
        }
    }


    MPI_Comm_free(&world);

    MPI_Finalize();
    return EXIT_SUCCESS;
}

void init() {
    int i, j, k;
    for (i = 0; i <= N - 1; i++) {
        for (j = 0; j <= N - 1; j++) {
            for (k = 0; k <= N - 1; k++) {
                if (i == 0 || i == N - 1 || j == 0 || j == N - 1 || k == 0 || k == N - 1) {
                    A[slider][i][j][k] = 0.;
                } else {
                    A[slider][i][j][k] = (4. + i + j + k);
                }
            }
        }
    }
}

void verify() {
    double s = 0.;
    int i, j, k;
    for (i = 0; i <= N - 1; i++) {
        for (j = 0; j <= N - 1; j++) {
            for (k = 0; k <= N - 1; k++) {
                s = s + A[slider][i][j][k] * (i + 1) * (j + 1) * (k + 1) / (N * N * N);
            }
        }
    }
}