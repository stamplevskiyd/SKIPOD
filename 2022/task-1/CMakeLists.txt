cmake_minimum_required(VERSION 3.23)
project(task_1 C)

set(CMAKE_CXX_STANDARD 17)

find_package(MPI REQUIRED)

set(CMAKE_C_COMPILER mpicc)
set(CMAKE_CXX_COMPILER mpirun)
set(MPI_GUESS_LIBRARY_NAME MPICH2)

add_executable(task_1 main.c)
