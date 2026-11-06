#define _POSIX_C_SOURCE 200809L

#include <omp.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Dimensoes do Sudoku: 9 linhas, 9 colunas e 81 celulas. */
#define SUDOKU_N 9
#define CELL_COUNT 81

/*
 * Mascara dos numeros permitidos: bits 1 ate 9 ligados.
 * O bit zero fica desligado porque zero representa uma celula vazia.
 */
#define FULL_MASK 0x3FEu

/*
 * Estado usado durante a busca de uma solucao.
 * As mascaras guardam quais numeros ja aparecem em cada linha,
 * coluna e bloco 3x3. Isso evita percorrer essas regioes toda vez
 * que precisamos descobrir se um numero pode ser colocado.
 */
typedef struct {
    unsigned char cell[CELL_COUNT];
    uint16_t row_mask[SUDOKU_N];
    uint16_t col_mask[SUDOKU_N];
    uint16_t box_mask[SUDOKU_N];
} SearchState;

/*
 * Guarda os dados de cada Sudoku:
 * input    = tabuleiro recebido no CSV (zeros sao casas vazias);
 * expected = solucao oficial do dataset, usada para conferir o resultado;
 * solution = solucao encontrada pelo nosso algoritmo;
 * status   = indica se foi resolvido ou se houve algum problema.
 */
typedef struct {
    unsigned char input[CELL_COUNT];
    unsigned char expected[CELL_COUNT];
    unsigned char solution[CELL_COUNT];
    int status;
} Puzzle;

enum {
    PUZZLE_PENDING = 0,
    PUZZLE_SOLVED,
    PUZZLE_INVALID,
    PUZZLE_UNSOLVABLE,
    PUZZLE_INCORRECT
};

/* Retorna o indice do bloco 3x3 da posicao. */
static int box_index(int row, int col)
{
    return (row / 3) * 3 + (col / 3);
}

/* Copia o tabuleiro e monta as mascaras. Retorna 0 se houver conflito. */
static int initialize_state(SearchState *state,
                            const unsigned char input[CELL_COUNT])
{
    memset(state, 0, sizeof(*state));
    memcpy(state->cell, input, CELL_COUNT);

    for (int pos = 0; pos < CELL_COUNT; ++pos) {
        int value = state->cell[pos];

        if (value == 0)
            continue;

        int row = pos / SUDOKU_N;
        int col = pos % SUDOKU_N;
        int box = box_index(row, col);
        /*
         * Cada numero e representado por um bit.
         * Exemplo: o numero 3 liga o bit 3 (1 << 3).
         * Se esse bit ja estiver na linha, coluna ou bloco, ha conflito.
         */
        uint16_t bit = (uint16_t)(1u << value);

        if ((state->row_mask[row] & bit) ||
            (state->col_mask[col] & bit) ||
            (state->box_mask[box] & bit))
            return 0;

        /* Registra que esse numero ja existe nas tres regioes. */
        state->row_mask[row] |= bit;
        state->col_mask[col] |= bit;
        state->box_mask[box] |= bit;
    }

    return 1;
}

/*
 * Escolhe a celula vazia com menos candidatos (heuristica MRV).
 * Retorna:
 *   >= 0: posicao escolhida;
 *   -1: tabuleiro completo;
 *   -2: nao existe candidato para alguma celula.
 */
static int choose_mrv(const SearchState *state, unsigned int *candidates_out)
{
    int best_pos = -1;
    int best_count = 10;
    unsigned int best_candidates = 0;

    for (int pos = 0; pos < CELL_COUNT; ++pos) {
        if (state->cell[pos] != 0)
            continue;

        int row = pos / SUDOKU_N;
        int col = pos % SUDOKU_N;
        int box = box_index(row, col);

        unsigned int used = (unsigned int)state->row_mask[row] |
                            (unsigned int)state->col_mask[col] |
                            (unsigned int)state->box_mask[box];

        /*
         * used tem os bits dos numeros que ja aparecem nas regioes.
         * ~used inverte os bits; AND com FULL_MASK deixa somente 1..9
         * que ainda podem ser usados nessa celula.
         */
        unsigned int candidates = FULL_MASK & (~used);
        int count = __builtin_popcount(candidates);

        /* Nenhum numero cabe nesta celula: este caminho nao tem solucao. */
        if (count == 0) {
            *candidates_out = 0;
            return -2;
        }

        /*
         * MRV (Minimum Remaining Values): escolhe a casa com menos
         * candidatos. Isso reduz as tentativas e costuma acelerar a busca.
         */
        if (count < best_count) {
            best_count = count;
            best_pos = pos;
            best_candidates = candidates;

            if (count == 1)
                break;
        }
    }

    *candidates_out = best_candidates;
    return best_pos;
}

/* Resolve um tabuleiro usando backtracking recursivo. */
static int solve_sudoku(SearchState *state)
{
    /* Primeiro escolhe a proxima casa usando a heuristica MRV. */
    unsigned int candidates;
    int pos = choose_mrv(state, &candidates);

    if (pos == -2)
        return 0; /* Beco sem saida. */

    if (pos == -1)
        return 1; /* Todas as celulas foram preenchidas. */

    int row = pos / SUDOKU_N;
    int col = pos % SUDOKU_N;
    int box = box_index(row, col);

    /* Tenta, um por vez, todos os valores permitidos nessa celula. */
    while (candidates != 0) {
        /*
         * Extrai o bit ligado mais a direita, ou seja, um candidato.
         * Depois remove esse bit da lista para testar o proximo candidato.
         * __builtin_ctz informa a posicao do bit, que e o numero Sudoku.
         */
        unsigned int bit = candidates & (~candidates + 1u);
        candidates &= candidates - 1u;

        int value = __builtin_ctz(bit);

        /*
         * Faz a tentativa: coloca o numero na celula e atualiza as
         * mascaras para que as chamadas recursivas vejam essa escolha.
         */
        state->cell[pos] = (unsigned char)value;
        state->row_mask[row] |= (uint16_t)bit;
        state->col_mask[col] |= (uint16_t)bit;
        state->box_mask[box] |= (uint16_t)bit;

        if (solve_sudoku(state))
            return 1; /* Mantem o tabuleiro resolvido. */

        /* A tentativa falhou: desfaz a jogada antes de tentar outro valor. */
        /*
         * BACKTRACKING: se a tentativa nao levou a uma solucao,
         * desfaz exatamente as alteracoes feitas acima e tenta outro numero.
         */
        state->cell[pos] = 0;
        state->row_mask[row] &= (uint16_t)~bit;
        state->col_mask[col] &= (uint16_t)~bit;
        state->box_mask[box] &= (uint16_t)~bit;
    }

    return 0;
}

/* Prepara e resolve um Sudoku. Cada chamada trabalha em seu proprio tabuleiro. */
static void solve_one_puzzle(Puzzle *puzzle)
{
    SearchState state;

    if (!initialize_state(&state, puzzle->input)) {
        puzzle->status = PUZZLE_INVALID;
        return;
    }

    if (solve_sudoku(&state)) {
        memcpy(puzzle->solution, state.cell, CELL_COUNT);
        puzzle->status = PUZZLE_SOLVED;
    } else {
        puzzle->status = PUZZLE_UNSOLVABLE;
    }
}

/* Le uma linha do Kaggle: 81 digitos do quiz, virgula, 81 da solucao. */
static int parse_kaggle_line(char *line,
                             unsigned char input[CELL_COUNT],
                             unsigned char expected[CELL_COUNT])
{
    line[strcspn(line, "\r\n")] = '\0';

    /* Separa os dois campos do CSV usando a virgula como delimitador. */
    char *comma = strchr(line, ',');
    if (comma == NULL || strchr(comma + 1, ',') != NULL)
        return 0;

    *comma = '\0';

    if (strlen(line) != CELL_COUNT || strlen(comma + 1) != CELL_COUNT)
        return 0;

    /* Converte os caracteres '0'..'9' para valores inteiros de 0 a 9. */
    for (int i = 0; i < CELL_COUNT; ++i) {
        unsigned char quiz_digit = (unsigned char)line[i];
        unsigned char solution_digit = (unsigned char)comma[1 + i];

        if (!isdigit(quiz_digit) || !isdigit(solution_digit))
            return 0;

        input[i] = (unsigned char)(quiz_digit - '0');
        expected[i] = (unsigned char)(solution_digit - '0');

        if (expected[i] == 0 ||
            (input[i] != 0 && input[i] != expected[i]))
            return 0;
    }

    return 1;
}

/* Carrega os primeiros N Sudokus do arquivo CSV. */
static int read_puzzles(const char *path,
                        size_t requested_count,
                        Puzzle **puzzles_out,
                        size_t *count_out)
{
    FILE *file = fopen(path, "r");

    if (file == NULL) {
        perror("Erro ao abrir o arquivo de entrada");
        return 0;
    }

    char *line = NULL;
    size_t line_capacity = 0;
    ssize_t line_length = getline(&line, &line_capacity, file);

    if (line_length == -1) {
        fprintf(stderr, "Erro: arquivo CSV vazio.\n");
        free(line);
        fclose(file);
        return 0;
    }

    line[strcspn(line, "\r\n")] = '\0';

    if (strcmp(line, "quizzes,solutions") != 0) {
        fprintf(stderr,
                "Erro: cabecalho inesperado. Esperado: quizzes,solutions\n");
        free(line);
        fclose(file);
        return 0;
    }

    /*
     * Reserva memoria para todos os tabuleiros solicitados.
     * calloc inicializa os campos com zero.
     */
    Puzzle *puzzles = calloc(requested_count, sizeof(*puzzles));

    if (puzzles == NULL) {
        fprintf(stderr, "Erro: memoria insuficiente para os Sudokus.\n");
        free(line);
        fclose(file);
        return 0;
    }

    size_t count = 0;
    int ok = 1;

    /* Le cada registro ate atingir a quantidade solicitada ou o fim do arquivo. */
    while (count < requested_count &&
           (line_length = getline(&line, &line_capacity, file)) != -1) {
        (void)line_length;

        if (line[0] == '\n' || line[0] == '\r' || line[0] == '\0')
            continue;

        if (!parse_kaggle_line(line, puzzles[count].input,
                               puzzles[count].expected)) {
            fprintf(stderr,
                    "Erro: registro %zu do CSV possui formato invalido.\n",
                    count + 1);
            ok = 0;
            break;
        }

        puzzles[count].status = PUZZLE_PENDING;
        ++count;
    }

    if (ok && ferror(file)) {
        perror("Erro durante a leitura do CSV");
        ok = 0;
    }

    if (ok && count < requested_count) {
        fprintf(stderr,
                "Erro: foram solicitados %zu Sudokus, mas o arquivo contem somente %zu registros validos.\n",
                requested_count, count);
        ok = 0;
    }

    free(line);
    fclose(file);

    if (!ok) {
        free(puzzles);
        return 0;
    }

    *puzzles_out = puzzles;
    *count_out = count;
    return 1;
}

/* Grava os Sudokus resolvidos no mesmo formato do arquivo Kaggle. */
static int write_solutions(const char *path,
                           const Puzzle *puzzles,
                           size_t count)
{
    FILE *file = fopen(path, "w");

    if (file == NULL) {
        perror("Erro ao abrir o arquivo de saida");
        return 0;
    }

    fprintf(file, "quizzes,solutions\n");

    /* Cada linha de saida contem o tabuleiro original e sua solucao. */
    for (size_t i = 0; i < count; ++i) {
        for (int j = 0; j < CELL_COUNT; ++j)
            fputc('0' + puzzles[i].input[j], file);

        fputc(',', file);

        for (int j = 0; j < CELL_COUNT; ++j)
            fputc('0' + puzzles[i].solution[j], file);

        fputc('\n', file);
    }

    int ok = !ferror(file);

    if (fclose(file) != 0)
        ok = 0;

    if (!ok)
        fprintf(stderr, "Erro ao gravar o arquivo de saida.\n");

    return ok;
}

/* Converte a quantidade de Sudokus informada na linha de comando. */
static int parse_positive_size(const char *text, size_t *value_out)
{
    if (text == NULL || text[0] == '\0' || text[0] == '-')
        return 0;

    errno = 0;
    char *end = NULL;
    unsigned long long value = strtoull(text, &end, 10);

    if (end == text || *end != '\0' || errno == ERANGE || value == 0 ||
        value > (unsigned long long)(SIZE_MAX / sizeof(Puzzle)))
        return 0;

    *value_out = (size_t)value;
    return 1;
}

/* Converte a quantidade de threads informada na linha de comando. */
static int parse_positive_int(const char *text, int *value_out)
{
    errno = 0;
    char *end = NULL;
    long value = strtol(text, &end, 10);

    if (end == text || *end != '\0' || errno == ERANGE ||
        value < 1 || value > INT_MAX)
        return 0;

    *value_out = (int)value;
    return 1;
}

int main(int argc, char **argv)
{
    if (argc != 5) {
        fprintf(stderr,
                "Uso: %s entrada.csv saida.csv quantidade_sudokus threads\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    size_t requested_count;
    int threads;

    if (!parse_positive_size(argv[3], &requested_count)) {
        fprintf(stderr,
                "Erro: quantidade de Sudokus deve ser um inteiro positivo.\n");
        return EXIT_FAILURE;
    }

    if (!parse_positive_int(argv[4], &threads)) {
        fprintf(stderr,
                "Erro: quantidade de threads deve ser um inteiro positivo.\n");
        return EXIT_FAILURE;
    }

    Puzzle *puzzles = NULL;
    size_t puzzle_count = 0;

    /* A leitura do CSV fica fora da medicao do tempo de resolucao. */
    if (!read_puzzles(argv[1], requested_count, &puzzles, &puzzle_count))
        return EXIT_FAILURE;

    /*
     * Desativa o ajuste automatico de threads e solicita a quantidade
     * escolhida pelo usuario. Assim os testes ficam mais controlados.
     */
    omp_set_dynamic(0);
    omp_set_num_threads(threads);

    printf("Sudokus: %zu | threads solicitadas: %d\n",
           puzzle_count, threads);

    /* Paraleliza os tabuleiros: cada iteracao resolve um Sudoku diferente. */
    double start_time = omp_get_wtime();

    /*
     * PARALELISMO PRINCIPAL:
     * cada iteracao resolve um Sudoku independente.
     * schedule(dynamic, 10) entrega 10 tabuleiros por vez a cada thread;
     * quando uma thread termina seu lote, recebe outro.
     * Isso ajuda a equilibrar a carga entre as threads.
     */
    #pragma omp parallel for schedule(dynamic, 10) default(none) \
        shared(puzzles, puzzle_count)
    for (size_t i = 0; i < puzzle_count; ++i)
        solve_one_puzzle(&puzzles[i]);

    double elapsed = omp_get_wtime() - start_time;

    /*
     * VALIDACAO:
     * compara a solucao calculada com a solucao oficial do CSV.
     * Essa etapa fica fora da medicao de tempo do algoritmo.
     */
    /* Confere cada resultado com a solucao fornecida pelo dataset. */
    size_t correct = 0;
    size_t invalid = 0;
    size_t unsolvable = 0;
    size_t different = 0;

    for (size_t i = 0; i < puzzle_count; ++i) {
        if (puzzles[i].status == PUZZLE_SOLVED) {
            if (memcmp(puzzles[i].solution, puzzles[i].expected,
                       CELL_COUNT) == 0) {
                ++correct;
            } else {
                puzzles[i].status = PUZZLE_INCORRECT;
                ++different;
                fprintf(stderr,
                        "Sudoku %zu: solucao diferente da solucao do dataset.\n",
                        i + 1);
            }
        } else if (puzzles[i].status == PUZZLE_INVALID) {
            ++invalid;
            fprintf(stderr,
                    "Sudoku %zu: pistas iniciais inconsistentes.\n", i + 1);
        } else {
            ++unsolvable;
            fprintf(stderr,
                    "Sudoku %zu: nenhuma solucao encontrada.\n", i + 1);
        }
    }

    printf("Sudokus resolvidos corretamente: %zu/%zu\n",
           correct, puzzle_count);
    printf("Tempo de resolucao: %.6f segundos\n", elapsed);
    printf("Threads solicitadas: %d\n", threads);

    /* Nao gera o CSV final se algum tabuleiro falhou na validacao. */
    if (correct != puzzle_count) {
        fprintf(stderr,
                "Falha: invalidos=%zu, sem solucao=%zu, divergentes=%zu.\n",
                invalid, unsolvable, different);
        fprintf(stderr,
                "Arquivo de saida nao foi gerado porque houve falha na validacao.\n");
        free(puzzles);
        return EXIT_FAILURE;
    }

    if (!write_solutions(argv[2], puzzles, puzzle_count)) {
        free(puzzles);
        return EXIT_FAILURE;
    }

    printf("Resultados gravados em: %s\n", argv[2]);

    free(puzzles);
    return EXIT_SUCCESS;
}
