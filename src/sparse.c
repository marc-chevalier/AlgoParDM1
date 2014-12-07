#include <stdlib.h>
#include <stdio.h>
#include <mpi.h>
#include <stdbool.h>
#include <math.h>
#include "shared.c"
#include "gfx.c"

/* Quelques structures utiles */
typedef enum request_type {
    VALUE = 0,
    CONSTANT = 1,
    GET = 2
} request_type;

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

#define GUI_SCALE_FACTOR 30
void gui_set_color_from_value(double value) {
    int color = (int) (255 * sqrt(value));
    gfx_color(color, color, color);
}

void gui_draw_matrix(matrix matrix) {
    coordinates coord;
    
    gfx_clear();
    for(coord.x = 0; coord.x < matrix.size.x; coord.x++) {
        for(coord.y = 0; coord.y < matrix.size.y; coord.y++) {
            gui_set_color_from_value(matrix.data[matrix.size.y * coord.x + coord.y]);
            for(int k = 0; k < GUI_SCALE_FACTOR; k++) {
                gfx_line(coord.x * GUI_SCALE_FACTOR, coord.y * GUI_SCALE_FACTOR + k, (coord.x + 1) * GUI_SCALE_FACTOR, coord.y * GUI_SCALE_FACTOR + k);
            }
        }
    }
    gfx_flush();
}

void gui_open(environment environment) {
    coordinates window_size = coordinates_init(environment.matrix.size.x * GUI_SCALE_FACTOR, environment.matrix.size.y * GUI_SCALE_FACTOR);
    
    gfx_open(window_size.x, window_size.y, "constants");
    
    gui_draw_matrix(environment.matrix);
}

/* Main code */
int main(int argc, char* argv[])
{
    MPI_Init(&argc, &argv);
    
    int my_id = get_my_id();
    environment environment;
    int number_of_cpu = get_number_of_cpu();
    coordinates my_coordinates;
    
    if(my_id == 0) {
        environment = parse_file_header();
        if(number_of_cpu != environment.matrix.size.x * environment.matrix.size.y) {
            fprintf(stderr, "The number of CPU is different than the size of the matrix.\n");
            exit(EXIT_FAILURE);
        }
    }

    MPI_Bcast(&environment.p, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast(&environment.t, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&environment.matrix.size.x, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&environment.matrix.size.y, 1, MPI_INT, 0, MPI_COMM_WORLD);
    my_coordinates = get_my_cell_coordinates(environment.matrix.size);

    //compute Z^t
    //We don't use here the algorithm of the question 4 but the one given at question 7
    double zt_value = (my_id == 0);
    for(int i = environment.t; i > 0; i /= 2) {
        //do z^t -> z^{2t}
        coordinates added_case;
        double new_zt_value = 0;
        for(added_case.x = 0; added_case.x < environment.matrix.size.x; added_case.x++) {
            for(added_case.y = 0; added_case.y < environment.matrix.size.y; added_case.y++) {
                double added_value = zt_value;
                MPI_Bcast(&added_value, 1, MPI_DOUBLE, cpu_id_from_coordinates_with_mod(added_case.x, added_case.y, environment.matrix.size), MPI_COMM_WORLD);
                double tmp_zt = move_double(
                                            cpu_id_from_coordinates_with_mod(my_coordinates.x - added_case.x, my_coordinates.y - added_case.y, environment.matrix.size),
                                            cpu_id_from_coordinates_with_mod(my_coordinates.x + added_case.x, my_coordinates.y + added_case.y, environment.matrix.size),
                                            zt_value
                                            );
                new_zt_value += added_value * tmp_zt;
            }
        }
        zt_value = new_zt_value;

        //compute a step if needed
        if(i % 2 == 1) {
            double b = move_double(
                        cpu_id_from_coordinates_with_mod(my_coordinates.x - 1, my_coordinates.y, environment.matrix.size),
                        cpu_id_from_coordinates_with_mod(my_coordinates.x + 1, my_coordinates.y, environment.matrix.size),
                        zt_value
                        );
            double d = move_double(
                        cpu_id_from_coordinates_with_mod(my_coordinates.x, my_coordinates.y - 1, environment.matrix.size),
                        cpu_id_from_coordinates_with_mod(my_coordinates.x, my_coordinates.y + 1, environment.matrix.size),
                        zt_value
                        );
            double f = move_double(
                        cpu_id_from_coordinates_with_mod(my_coordinates.x, my_coordinates.y + 1, environment.matrix.size),
                        cpu_id_from_coordinates_with_mod(my_coordinates.x, my_coordinates.y - 1, environment.matrix.size),
                        zt_value
                        );
            double h = move_double(
                        cpu_id_from_coordinates_with_mod(my_coordinates.x + 1, my_coordinates.y, environment.matrix.size),
                        cpu_id_from_coordinates_with_mod(my_coordinates.x - 1, my_coordinates.y, environment.matrix.size),
                        zt_value
                        );
        
            zt_value = (1 - environment.p) * zt_value + environment.p * (b + d + f + h) / 4;
        }
    }
    
    if(my_id == 0 && with_gui(argc, argv)) {
        gui_open(environment);
    }

    //Do parsing and output
    double my_value = 0;
    coordinates end_target = coordinates_init(-1, -1);
    while(42) {
        request_type type;
        coordinates target;
        double x;

        if(my_id == 0) {
            if(with_gui(argc, argv)) {
                gui_draw_matrix(environment.matrix);
                while(gfx_wait() != 1) {}
                target.x = gfx_xpos() / GUI_SCALE_FACTOR;
                target.y = gfx_xpos() / GUI_SCALE_FACTOR;
                x = 1;
                type = VALUE;
            } else {
                if(scanf("%d %d %d %lf", &type, &target.x, &target.y, &x) != 4) {
                    fprintf(stderr, "Bad input\n");
                    break;
                }
                if(coordinates_equals(target, end_target)) {
                    break;
                }
            }
        }
        MPI_Bcast(&type, 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&target.x, 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&target.y, 1, MPI_INT, 0, MPI_COMM_WORLD);

        if(type == GET) {
            if(my_id == 0) {
                 printf("Value of case (%d, %d) is %lf.\n", target.x, target.y, matrix_get_case(environment.matrix, target));
            }
        } else if(type == VALUE || type == CONSTANT) {
            MPI_Bcast(&x, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);

            //We move the cooeficent of zt
            //If the (0, 0) should become (target.x, target.y)
            double tmp_zt = move_double(
                                 cpu_id_from_coordinates_with_mod(my_coordinates.x - target.x, my_coordinates.y - target.y, environment.matrix.size),
                                 cpu_id_from_coordinates_with_mod(my_coordinates.x + target.x, my_coordinates.y + target.y, environment.matrix.size),
                                 zt_value
                                 );
            my_value += x * tmp_zt;

            //We get back values
            MPI_Gather(&my_value, 1, MPI_DOUBLE, environment.matrix.data, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        }
    }

    MPI_Finalize();
    return EXIT_SUCCESS;
}
