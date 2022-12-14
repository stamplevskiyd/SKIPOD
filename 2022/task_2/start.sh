source dockervars.sh
make
mpirun -np 10 --with-ft ulfm ./task_2