#define _POSIX_C_SOURCE 200809L

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "merge_sort.h"

typedef struct {
    int *values;
    int length;
} Vector;

typedef struct {
    int original_index;
    int length;
    int *values;
} Result;

static void free_vectors(Vector *vectors, int count) {
    if (!vectors) return;
    for (int i = 0; i < count; ++i) free(vectors[i].values);
    free(vectors);
}

static int append_int(int **values, int *length, int *capacity, int value) {
    if (*length == *capacity) {
        int new_capacity = (*capacity == 0) ? 8 : (*capacity * 2);
        int *tmp = realloc(*values, (size_t)new_capacity * sizeof(int));
        if (!tmp) return 0;
        *values = tmp;
        *capacity = new_capacity;
    }
    (*values)[(*length)++] = value;
    return 1;
}

static int append_vector(Vector **vectors, int *count, int *capacity,
                         int *values, int length) {
    if (*count == *capacity) {
        int new_capacity = (*capacity == 0) ? 16 : (*capacity * 2);
        Vector *tmp = realloc(*vectors, (size_t)new_capacity * sizeof(Vector));
        if (!tmp) return 0;
        *vectors = tmp;
        *capacity = new_capacity;
    }
    (*vectors)[*count].values = values;
    (*vectors)[*count].length = length;
    ++(*count);
    return 1;
}

static int parse_line(char *line, int line_number,
                      int **values_out, int *length_out) {
    int *values = NULL;
    int length = 0;
    int capacity = 0;
    char *cursor = line;

    while (*cursor != '\0') {
        /* Ignora espaços no início do campo. */
        while (*cursor == ' ' || *cursor == '\t' ||
               *cursor == '\r' || *cursor == '\n') {
            cursor++;
        }

        /* Ignora linhas vazias. */
        if (*cursor == '\0') {
            break;
        }

        /* Converte o próximo campo para inteiro. */
        errno = 0;
        char *end = NULL;
        long parsed = strtol(cursor, &end, 10);

        if (cursor == end || errno == ERANGE ||
            parsed < INT_MIN || parsed > INT_MAX) {
            fprintf(stderr,
                    "Erro: inteiro inválido na linha %d.\n",
                    line_number);
            free(values);
            return 0;
        }

        if (!append_int(&values, &length, &capacity, (int)parsed)) {
            fprintf(stderr,
                    "Erro: sem memória na linha %d.\n",
                    line_number);
            free(values);
            return 0;
        }

        cursor = end;

        /* Ignora espaços depois do número. */
        while (*cursor == ' ' || *cursor == '\t' ||
               *cursor == '\r') {
            cursor++;
        }

        /* Espera ponto e vírgula ou fim de linha. */
        if (*cursor == ';') {
            cursor++;

            /* Não permite campo vazio no final. */
            char *next = cursor;
            while (*next == ' ' || *next == '\t' ||
                   *next == '\r') {
                next++;
            }

            if (*next == '\0' || *next == '\n') {
                fprintf(stderr,
                        "Erro: campo vazio na linha %d.\n",
                        line_number);
                free(values);
                return 0;
            }
        } else if (*cursor != '\0' && *cursor != '\n') {
            fprintf(stderr,
                    "Erro: esperado ';' na linha %d.\n",
                    line_number);
            free(values);
            return 0;
        }
    }

    *values_out = values;
    *length_out = length;

    return 1;
}

static int read_csv(const char *path, Vector **vectors_out, int *count_out) {
    FILE *file = fopen(path, "r");
    if (!file) {
        perror(path);
        return 0;
    }

    Vector *vectors = NULL;
    int count = 0, capacity = 0;
    char *line = NULL;
    size_t line_capacity = 0;
    ssize_t nread;
    int line_number = 0;

    while ((nread = getline(&line, &line_capacity, file)) != -1) {
        (void)nread;
        ++line_number;

        int *values = NULL, length = 0;
        if (!parse_line(line, line_number, &values, &length)) {
            free(line);
            fclose(file);
            free_vectors(vectors, count);
            return 0;
        }

        if (length == 0) continue; /* Ignora linhas vazias. */

        if (!append_vector(&vectors, &count, &capacity, values, length)) {
            fprintf(stderr, "Erro: sem memória ao armazenar vetores.\n");
            free(values);
            free(line);
            fclose(file);
            free_vectors(vectors, count);
            return 0;
        }
    }

    free(line);
    fclose(file);
    *vectors_out = vectors;
    *count_out = count;
    return 1;
}

static int write_csv(const char *path, Result *results, int count) {
    FILE *file = fopen(path, "w");
    if (!file) {
        perror(path);
        return 0;
    }

    for (int i = 0; i < count; ++i) {
        if (i > 0 && results[i].original_index < results[i - 1].original_index) {
            /* A ordenação por índice é feita antes de chamar esta função. */
            fprintf(stderr, "Aviso: resultados fora de ordem.\n");
        }
        for (int j = 0; j < results[i].length; ++j) {
            if (j > 0 && fputc(',', file) == EOF) {
                fclose(file);
                return 0;
            }
            if (fprintf(file, "%d", results[i].values[j]) < 0) {
                fclose(file);
                return 0;
            }
        }
        if (fputc('\n', file) == EOF) {
            fclose(file);
            return 0;
        }
    }

    if (fclose(file) != 0) {
        perror("Erro ao fechar arquivo de saída");
        return 0;
    }
    return 1;
}

static int compare_results(const void *a, const void *b) {
    const Result *ra = a;
    const Result *rb = b;
    if (ra->original_index < rb->original_index) return -1;
    if (ra->original_index > rb->original_index) return 1;
    return 0;
}

int main(int argc, char **argv) {
    int rank, world_size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    if (argc != 3) {
        if (rank == 0) {
            fprintf(stderr, "Uso: %s entrada.csv saida.csv\n", argv[0]);
        }
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    double total_start = MPI_Wtime();
    Vector *vectors = NULL;
    int vector_count = 0;
    int input_ok = 1;

    if (rank == 0) {
        input_ok = read_csv(argv[1], &vectors, &vector_count);
    }

    MPI_Bcast(&input_ok, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (!input_ok) {
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    MPI_Bcast(&vector_count, 1, MPI_INT, 0, MPI_COMM_WORLD);

    Result *results = NULL;
    int local_result_count = 0;
    int local_capacity = 0;
    int local_ok = 1;

    if (rank == 0) {
        for (int i = 0; i < vector_count; ++i) {
            int destination = i % world_size;
            int length = vectors[i].length;

            if (destination == 0) {
                if (!append_vector((Vector **)&results, &local_result_count,
                                   &local_capacity, NULL, 0)) {
                    local_ok = 0;
                    break;
                }
                results[local_result_count - 1].original_index = i;
                results[local_result_count - 1].length = length;
                results[local_result_count - 1].values = vectors[i].values;
                vectors[i].values = NULL;
            } else {
                MPI_Send(&i, 1, MPI_INT, destination, 10, MPI_COMM_WORLD);
                MPI_Send(&length, 1, MPI_INT, destination, 11, MPI_COMM_WORLD);
                MPI_Send(vectors[i].values, length, MPI_INT, destination, 12, MPI_COMM_WORLD);
            }
        }
    } else {
        for (int i = rank; i < vector_count; i += world_size) {
            int index = -1, length = 0;
            MPI_Recv(&index, 1, MPI_INT, 0, 10, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(&length, 1, MPI_INT, 0, 11, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            int *values = NULL;
            if (length > 0) {
                values = malloc((size_t)length * sizeof(int));
                if (!values) {
                    local_ok = 0;
                    /* Ainda precisamos receber a mensagem de dados para não travar o rank 0. */
                    int *discard = malloc((size_t)length * sizeof(int));
                    if (discard) {
                        MPI_Recv(discard, length, MPI_INT, 0, 12, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                        free(discard);
                    } else {
                        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                    }
                    continue;
                }
            }
            MPI_Recv(values, length, MPI_INT, 0, 12, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            merge_sort(values, length);

            if (local_result_count == local_capacity) {
                int new_capacity = local_capacity == 0 ? 8 : local_capacity * 2;
                Result *tmp = realloc(results, (size_t)new_capacity * sizeof(Result));
                if (!tmp) {
                    free(values);
                    local_ok = 0;
                    continue;
                }
                results = tmp;
                local_capacity = new_capacity;
            }
            results[local_result_count].original_index = index;
            results[local_result_count].length = length;
            results[local_result_count].values = values;
            ++local_result_count;
        }
    }

    if (rank == 0) {
        for (int i = 0; i < local_result_count; ++i) {
            merge_sort(results[i].values, results[i].length);
        }
    }
    if (rank != 0) {
        for (int i = 0; i < local_result_count; ++i) {
            MPI_Send(&results[i].original_index, 1, MPI_INT, 0, 20, MPI_COMM_WORLD);
            MPI_Send(&results[i].length, 1, MPI_INT, 0, 21, MPI_COMM_WORLD);
            MPI_Send(results[i].values, results[i].length, MPI_INT, 0, 22, MPI_COMM_WORLD);
        }
    } else {
        int expected_remote = vector_count - local_result_count;
        for (int k = 0; k < expected_remote; ++k) {
            int index = -1, length = 0;
            MPI_Status status;
            MPI_Probe(MPI_ANY_SOURCE, 20, MPI_COMM_WORLD, &status);
            int source = status.MPI_SOURCE;
            MPI_Recv(&index, 1, MPI_INT, source, 20, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(&length, 1, MPI_INT, source, 21, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            int *values = NULL;
            if (length > 0) {
                values = malloc((size_t)length * sizeof(int));
                if (!values) {
                    fprintf(stderr, "Erro: sem memória ao receber resultados.\n");
                    MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                }
            }
            MPI_Recv(values, length, MPI_INT, source, 22, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            if (local_result_count == local_capacity) {
                int new_capacity = local_capacity == 0 ? 8 : local_capacity * 2;
                Result *tmp = realloc(results, (size_t)new_capacity * sizeof(Result));
                if (!tmp) {
                    free(values);
                    fprintf(stderr, "Erro: sem memória ao armazenar resultados.\n");
                    MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                }
                results = tmp;
                local_capacity = new_capacity;
            }
            results[local_result_count].original_index = index;
            results[local_result_count].length = length;
            results[local_result_count].values = values;
            ++local_result_count;
        }
        qsort(results, (size_t)local_result_count, sizeof(Result), compare_results);
        if (!write_csv(argv[2], results, local_result_count)) {
            fprintf(stderr, "Erro ao gravar arquivo de saída.\n");
            local_ok = 0;
        }
    }

    int all_ok = 0;
    MPI_Allreduce(&local_ok, &all_ok, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);

    if (rank == 0 && all_ok) {
        printf("Ordenados %d vetores usando %d processo(s).\n",
               vector_count, world_size);
        printf("Tempo total: %.6f segundos\n", MPI_Wtime() - total_start);
    }

    for (int i = 0; i < local_result_count; ++i) free(results[i].values);
    free(results);
    free_vectors(vectors, vector_count);

    MPI_Finalize();
    return all_ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
