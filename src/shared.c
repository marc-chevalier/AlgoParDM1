#include <stdlib.h>
#include <stdio.h>
#include <mpi.h>
#include <stdbool.h>
#include <string.h>

/* Quelques structures utiles */
typedef struct coordinates {
    int x;
    int y;
} coordinates;

coordinates coordinates_init(int x, int y) {
    coordinates coord;
    coord.x = x;
    coord.y = y;
    return coord;
}

inline bool coordinates_equals(coordinates a, coordinates b) {
    return a.x == b.x && a.y == b.y;
}

/* Useful functions */
int get_my_id() {
    int my_id;
    MPI_Comm_rank(MPI_COMM_WORLD, &my_id);
    return my_id;
}

int get_number_of_cpu() {
    int number;
    MPI_Comm_size(MPI_COMM_WORLD, &number);
    return number;
}

coordinates get_my_cell_coordinates(coordinates grid_size) {
    int my_id = get_my_id();
    
    return coordinates_init(my_id / grid_size.y, my_id % grid_size.y);
}

int mod(int val, const int mod) {
    while(val < 0) {
        val += mod;
    }
    return val % mod;
}

int cpu_id_from_coordinates_with_mod(int x, int y, coordinates grid_size) {
    return mod(x, grid_size.x) * grid_size.y + mod(y, grid_size.y);
}

//do variable move in the matrix. from is the pid from which we shoud get data and to the pid to which we shoud send val
double move_double(int from, int to, double val) {
    MPI_Send(&val, 1, MPI_DOUBLE, to, 0, MPI_COMM_WORLD);
    MPI_Recv(&val, 1, MPI_DOUBLE, from, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    
    return val;
}

bool with_gui(int argc, char* argv[]) {
    for(int i = 0; i < argc; i++) {
        if(strcmp("-g", argv[i]) == 0) {
            return true;
        }
    }
    
    return false;
}
