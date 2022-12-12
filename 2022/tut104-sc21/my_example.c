#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <math.h>
#include <mpi.h>
#include <mpi-ext.h>
#include <time.h>

#define F(x) (cos(pow(x, 1.04) * atan(5.2 * exp(x + 2.3))) + log(fabs(pow(x, 4.3) + asin(-0.4)*pow(1.0013, 13.001))) + atan(acos(1/x) + 7)) / 100
#define A 2.0
#define B 200

int rank=MPI_PROC_NULL, verbose=0; /* makes this global (for printfs) */
char** gargv;

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

int main( int argc, char* argv[] ) {
    MPI_Comm world; /* a world comm for the work, w/o the spares */
    MPI_Comm rworld; /* and a temporary handle to store the repaired copy */
    int np, ret_code;

    double l = A, r = B;
    double h;
    long int segnum;
    double local_res, local_r, local_l;
    double res;
    double timer;

    gargv = argv;
    MPI_Init(&argc, &argv);


    /* Am I a spare ? */
    MPI_Comm_get_parent( &world );
    if( MPI_COMM_NULL == world ) {
        /* First run: Let's create an initial world,
         * a copy of MPI_COMM_WORLD */
        MPI_Comm_dup( MPI_COMM_WORLD, &world );
    } else {
        /* I am a spare, lets get the repaired world */
        MPIX_Comm_replace( MPI_COMM_NULL, &world );
    }

    MPI_Comm_size( world, &np );
    MPI_Comm_rank( world, &rank );
    /* We set an errhandler on world, so that a failure is not fatal anymore. */
    MPI_Comm_set_errhandler( world, MPI_ERRORS_RETURN );

    timer = MPI_Wtime();

    srand(time(NULL) + rank);

    while(1){
        ///Choosing victims
        if ((rand() % 10 == 1) && rank != 0) {
            printf("Rank %04d: committing suicide\n", rank);
            raise(SIGKILL);
        }

        ///Processing calculations
        h = (r - l) / np;
        segnum = 262144 / np;
        local_l = l + rank * h;
        local_r = local_l + h;
        local_res = integral(local_l, local_r, segnum);

        ///Processing errors
        ret_code = MPI_Allreduce(&local_res, &res, 1, MPI_DOUBLE, MPI_SUM, world); // складываем все результаты и передаем процессу 0
        if (ret_code != MPI_SUCCESS) {
            printf("Error happen, restarting process %d\n", rank);
            MPIX_Comm_replace(world, &rworld);
            MPI_Comm_free(&world);
            world = rworld;
        }
        else
            break;
    }

    MPI_Barrier(world);
    timer = MPI_Wtime() - timer;
    MPI_Comm_free(&world);

    if (rank == 0) {
        printf("Processes number: %d, execution time, %.6f, result: %.10f\n", np, timer, res);
    }

    MPI_Finalize();
    return 0;
}

