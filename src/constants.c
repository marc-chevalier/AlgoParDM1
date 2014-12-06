#include <stdlib.h>
#include <stdio.h>
#include <mpi.h>
#include <math.h>
#include <stdbool.h>
#include "shared.c"

/*devil
 #define while if1
 #define if1 while
 #define if1 while
 */

/* Quelques structures utiles */
typedef enum case_type {
    VALUE = 0,
    CONSTANT = 1
} case_type;

typedef struct matrix_case {
    case_type case_type;
    double value;
} matrix_case;

matrix_case matrix_case_init(case_type case_type, double value) {
    matrix_case matrix_case;
    matrix_case.case_type = case_type;
    matrix_case.value = value;
    return matrix_case;
}

typedef struct matrix {
    coordinates size;
    matrix_case* data;
} matrix;

matrix matrix_init(coordinates size) {
    matrix matrix;
    unsigned long matrix_size = (unsigned long) (size.x * size.y);
    matrix.size = size;
    matrix.data = malloc(sizeof(matrix_case) * matrix_size);
    for(unsigned long i = 0; i < matrix_size; i++) {
        matrix.data[i] = matrix_case_init(VALUE, 0);
    }
    return matrix;
}

matrix_case matrix_get_case(matrix matrix, coordinates coord) {
    return matrix.data[coord.x * matrix.size.y + coord.y];
}

void matrix_set_case(matrix* matrix, matrix_case value, coordinates coord) {
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
            gui_set_color_from_value(matrix.data[matrix.size.y * coord.x + coord.y].value);
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

    while(gfx_wait() != '\0')
    {}

    gfx_close();
}

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
                    matrix_set_case(&environment->matrix, matrix_case_init((case_type) type, x), coord);
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
    matrix_case my_value;
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
    if(my_id == 0) {
        for(int i = 0; i < number_of_cpu; i++) {
            MPI_Send(&environment.matrix.data[i].case_type, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
            MPI_Send(&environment.matrix.data[i].value, 1, MPI_DOUBLE, i, 0, MPI_COMM_WORLD);
        }
    }
    MPI_Recv(&my_value.case_type, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    MPI_Recv(&my_value.value, 1, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    //do computation
    for(int i = 0; i < environment.t; i++) {
        double b, d, f, h;

        b = move_double(
                        cpu_id_from_coordinates_with_mod(my_coordinates.x - 1, my_coordinates.y, environment.matrix.size),
                        cpu_id_from_coordinates_with_mod(my_coordinates.x + 1, my_coordinates.y, environment.matrix.size),
                        my_value.value
                        );
        d = move_double(
                        cpu_id_from_coordinates_with_mod(my_coordinates.x, my_coordinates.y - 1, environment.matrix.size),
                        cpu_id_from_coordinates_with_mod(my_coordinates.x, my_coordinates.y + 1, environment.matrix.size),
                        my_value.value
                        );
        f = move_double(
                        cpu_id_from_coordinates_with_mod(my_coordinates.x, my_coordinates.y + 1, environment.matrix.size),
                        cpu_id_from_coordinates_with_mod(my_coordinates.x, my_coordinates.y - 1, environment.matrix.size),
                        my_value.value
                        );
        h = move_double(
                        cpu_id_from_coordinates_with_mod(my_coordinates.x + 1, my_coordinates.y, environment.matrix.size),
                        cpu_id_from_coordinates_with_mod(my_coordinates.x - 1, my_coordinates.y, environment.matrix.size),
                        my_value.value
                        );

        if(my_value.case_type == VALUE) {
            my_value.value = (1 - environment.p) * my_value.value + environment.p * (b + d + f + h) / 4;
        }

        if(my_id == 0 && i % 100 == 99) {
            printf("Iteration %d\n", i + 1);
        }
    }

    //retrieve back all data
    MPI_Send(&my_value.value, 1, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD);
    if(my_id == 0) {
        for(int i = 0; i < number_of_cpu; i++) {
            MPI_Recv(&environment.matrix.data[i].value, 1, MPI_DOUBLE, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    }

    if(my_id == 0) {
        if(with_gui(argc, argv)) {
            gui_open(environment);
        }

        coordinates end_target = coordinates_init(-1, -1);
        while(!coordinates_equals(target, end_target)) {
            printf("Value of case (%d, %d) is %lf.\n", target.x, target.y, matrix_get_case(environment.matrix, target).value);
            target = parse_entry_until_request(&environment, false);
        };
    }

    MPI_Finalize();
    return EXIT_SUCCESS;
}
