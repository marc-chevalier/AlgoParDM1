#include <stdlib.h>
#include <stdio.h>
#include <mpi.h>
#include <stdbool.h>
#include "shared.c"

/* Quelques structures utiles */
typedef struct matrix {
    coordinates size;
    double* data;
} matrix;

matrix matrix_init(coordinates size) {
    matrix matrix;
    unsigned long matrix_size = (unsigned long) (size.x * size.y);
    matrix.size = size;
    matrix.data = malloc(sizeof(double) * matrix_size);
    for(unsigned long i = 0; i < matrix_size; i++) {
        matrix.data[i] = 0;
    }
    return matrix;
}

double matrix_get_case(matrix matrix, coordinates coord) {
    return matrix.data[coord.x * matrix.size.y + coord.y];
}

void matrix_set_case(matrix* matrix, double value, coordinates coord) {
    matrix->data[coord.x * matrix->size.y + coord.y] = value;
}

void matrix_destruct(matrix* matrix) {
    free(matrix->data);
}

typedef struct environment {
    double p;
    int t;
    matrix matrix;
} environment;


/* Input parsing */
environment parse_file_header() {
    environment data;
    coordinates matrix_size;

    if(scanf("%d %d %lf %d", &matrix_size.y, &matrix_size.x, &data.p, &data.t) != 4) {
        exit(EXIT_FAILURE);
    }
    data.matrix = matrix_init(matrix_size);

    return data;
}

coordinates parse_entry_until_request(environment* environment, bool update_matrix) {
    coordinates coord;
    int type;
    double x;

    while(scanf("%d %d %d %lf", &type, &coord.x, &coord.y, &x) == 4) {
        switch(type) {
            case 0:
            case 1:
                if(update_matrix) {
                    matrix_set_case(&environment->matrix, x, coord);
                }
                break;
            case 2:
                return coord;
            default:
                fprintf(stderr, "Unknown description type %d\n", type);
        }
    }

    return coordinates_init(-1, -1);
}


/* Main code */
int main(int argc, char* argv[])
{
    MPI_Init(&argc, &argv);

    int my_id = get_my_id();
    environment environment;
    double my_value;
    int number_of_cpu = get_number_of_cpu();
    coordinates target, my_coordinates;

    if(my_id == 0) {
        environment = parse_file_header();
        if(number_of_cpu != environment.matrix.size.x * environment.matrix.size.y) {
            fprintf(stderr, "The number of CPU is different than the size of the matrix.\n");
            exit(EXIT_FAILURE);
        }
        target = parse_entry_until_request(&environment, true);
    }

    MPI_Bcast(&environment.p, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast(&environment.t, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&environment.matrix.size.x, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&environment.matrix.size.y, 1, MPI_INT, 0, MPI_COMM_WORLD);
    my_coordinates = get_my_cell_coordinates(environment.matrix.size);

    //broadcast all data to process
    MPI_Scatter(environment.matrix.data, 1, MPI_DOUBLE, &my_value, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    //do computation
    for(int i = 0; i < environment.t; i++) {
        double b, d, f, h;

        b = move_double(
                        cpu_id_from_coordinates_with_mod(my_coordinates.x - 1, my_coordinates.y, environment.matrix.size),
                        cpu_id_from_coordinates_with_mod(my_coordinates.x + 1, my_coordinates.y, environment.matrix.size),
                        my_value
                        );
        d = move_double(
                        cpu_id_from_coordinates_with_mod(my_coordinates.x, my_coordinates.y - 1, environment.matrix.size),
                        cpu_id_from_coordinates_with_mod(my_coordinates.x, my_coordinates.y + 1, environment.matrix.size),
                        my_value
                        );
        f = move_double(
                        cpu_id_from_coordinates_with_mod(my_coordinates.x, my_coordinates.y + 1, environment.matrix.size),
                        cpu_id_from_coordinates_with_mod(my_coordinates.x, my_coordinates.y - 1, environment.matrix.size),
                        my_value
                        );
        h = move_double(
                        cpu_id_from_coordinates_with_mod(my_coordinates.x + 1, my_coordinates.y, environment.matrix.size),
                        cpu_id_from_coordinates_with_mod(my_coordinates.x - 1, my_coordinates.y, environment.matrix.size),
                        my_value
                        );

        my_value = (1 - environment.p) * my_value + environment.p * (b + d + f + h) / 4;

        if(my_id == 0 && i % 100 == 99) {
            printf("Iteration %d\n", i + 1);
        }
    }

    //retrieve back all data
    MPI_Gather(&my_value, 1, MPI_DOUBLE, environment.matrix.data, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    if(my_id == 0) {
        coordinates end_target = coordinates_init(-1, -1);
        while(!coordinates_equals(target, end_target)) {
            printf("Value of case (%d, %d) is %lf.\n", target.x, target.y, matrix_get_case(environment.matrix, target));
            target = parse_entry_until_request(&environment, false);
        };
    }

    MPI_Finalize();
    return EXIT_SUCCESS;
}
