
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include<omp.h>

typedef struct {
    int *dados;
    size_t tamanho;
} Vetor;

void countingSort(int *v, int *c, int n, int exp) {
    if (n <= 1) {
        return;
    }
    int threads = omp_get_max_threads();
    // Cada thread logica tera 10 contadores privados.
    int *contagem = calloc((size_t)threads * 10, sizeof(int));
    // Posicao inicial de cada digito para cada thread.
    size_t *posicoes = malloc((size_t)threads * 10 * sizeof(size_t));
    if (contagem == NULL || posicoes == NULL) {
        fprintf(stderr, "Erro de alocacao no Counting Sort.\n");
        free(contagem);
        free(posicoes);
        exit(EXIT_FAILURE);
    }
    // 1. Conta os digitos em paralelo.
    // Cada iteracao processa uma parte fixa do vetor.
    #pragma omp parallel for schedule(static)
    for (int t = 0; t < threads; t++) {
        int inicio = (int)((long long)n * t / threads);
        int fim = (int)((long long)n * (t + 1) / threads);
        int *local = &contagem[t * 10];
        for (int i = inicio; i < fim; i++) {
            int digito = (v[i] / exp) % 10;
            local[digito]++;
        }
    }
    // 2. Calcula as posicoes iniciais de cada digito.
    size_t inicio_digito[10];
    size_t total = 0;
    for (int d = 0; d < 10; d++) {
        inicio_digito[d] = total;
        for (int t = 0; t < threads; t++) {
            total += (size_t)contagem[t * 10 + d];
        }
    }
    for (int d = 0; d < 10; d++) {
        size_t pos = inicio_digito[d];
        for (int t = 0; t < threads; t++) {
            posicoes[t * 10 + d] = pos;
            pos += (size_t)contagem[t * 10 + d];
        }
    }
    // 3. Distribui os elementos em paralelo.
    #pragma omp parallel for schedule(static)
    for (int t = 0; t < threads; t++) {
        int inicio = (int)((long long)n * t / threads);
        int fim = (int)((long long)n * (t + 1) / threads);
        size_t local_pos[10];
        for (int d = 0; d < 10; d++) {
            local_pos[d] = posicoes[t * 10 + d];
        }
        for (int i = inicio; i < fim; i++) {
            int digito = (v[i] / exp) % 10;
            c[local_pos[digito]++] = v[i];
        }
    }
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < n; i++) {
        v[i] = c[i];
    }
    free(contagem);
    free(posicoes);
}


void radixSort(int *v, int *c, int n) {
    if (n <= 1) {
        return;
    }
    int maior = v[0];
    // Encontra o maior valor do vetor
    #pragma omp parallel for 
    for (int i = 1; i < n; i++) {
        if (v[i] > maior) {
            maior = v[i];
        }
    }
    // Ordena pelas unidades, dezenas, centenas etc.
    for (int exp = 1; maior / exp > 0; exp *= 10) {
        countingSort(v, c, n, exp);
        // Evita overflow ao multiplicar exp por 10
        if (exp > maior / 10) {
            break;
        }
    }
}

// Lê os dados do arquivo dados.csv

void liberar_vetores(Vetor *vetores, size_t quantidade) {
    if (vetores == NULL) {
        return;
    }

    for (size_t i = 0; i < quantidade; i++) {
        free(vetores[i].dados);
    }

    free(vetores);
}

int criar_vetores(const char *nome_arquivo,
                  Vetor **vetores_saida,
                  size_t *quantidade_saida) {
    FILE *arquivo = fopen(nome_arquivo, "r");

    if (arquivo == NULL) {
        perror("Erro ao abrir o arquivo");
        return -1;
    }

    Vetor *vetores = NULL;
    size_t quantidade = 0;
    size_t capacidade_vetores = 0;

    char *linha = NULL;
    size_t capacidade_linha = 0;
    ssize_t comprimento;

    while ((comprimento = getline(&linha,
                                  &capacidade_linha,
                                  arquivo)) != -1) {
        (void)comprimento;

        // Ignora linhas vazias
        char *p = linha;

        while (*p == ' ' || *p == '\t' ||
               *p == '\n' || *p == '\r') {
            p++;
        }

        if (*p == '\0') {
            continue;
        }

        // Aumenta o espaço para armazenar os vetores
        if (quantidade == capacidade_vetores) {
            size_t nova_capacidade =
                capacidade_vetores == 0
                ? 4 : capacidade_vetores * 2;

            Vetor *temp = realloc(
                vetores,
                nova_capacidade * sizeof(Vetor)
            );

            if (temp == NULL) {
                free(linha);
                fclose(arquivo);
                liberar_vetores(vetores, quantidade);
                return -1;
            }

            vetores = temp;
            capacidade_vetores = nova_capacidade;
        }

        vetores[quantidade].dados = NULL;
        vetores[quantidade].tamanho = 0;

        size_t capacidade_dados = 0;
        char *salvar;
        char *token = strtok_r(p, ";", &salvar);

        while (token != NULL) {
            errno = 0;
            char *fim;
            long valor = strtol(token, &fim, 10);

            while (*fim == ' ' || *fim == '\t' ||
                   *fim == '\r' || *fim == '\n') {
                fim++;
            }

            if (token == fim || *fim != '\0' ||
                errno == ERANGE ||
                valor < 0 || valor > INT_MAX) {
                fprintf(stderr,
                        "Valor invalido no arquivo dados.csv.\n");

                free(linha);
                fclose(arquivo);
                liberar_vetores(vetores, quantidade + 1);
                return -1;
            }

            // Aumenta a capacidade do vetor atual
            if (vetores[quantidade].tamanho ==
                capacidade_dados) {

                size_t nova_capacidade =
                    capacidade_dados == 0
                    ? 1024 : capacidade_dados * 2;

                int *temp = realloc(
                    vetores[quantidade].dados,
                    nova_capacidade * sizeof(int)
                );

                if (temp == NULL) {
                    free(linha);
                    fclose(arquivo);
                    liberar_vetores(vetores, quantidade + 1);
                    return -1;
                }

                vetores[quantidade].dados = temp;
                capacidade_dados = nova_capacidade;
            }

            vetores[quantidade].dados[
                vetores[quantidade].tamanho
            ] = (int)valor;

            vetores[quantidade].tamanho++;

            token = strtok_r(NULL, ";", &salvar);
        }

        if (vetores[quantidade].tamanho == 0) {
            fprintf(stderr, "Vetor vazio encontrado.\n");

            free(linha);
            fclose(arquivo);
            liberar_vetores(vetores, quantidade + 1);
            return -1;
        }

        quantidade++;
    }

    if (ferror(arquivo)) {
        perror("Erro durante a leitura");
        free(linha);
        fclose(arquivo);
        liberar_vetores(vetores, quantidade);
        return -1;
    }

    free(linha);
    fclose(arquivo);

    *vetores_saida = vetores;
    *quantidade_saida = quantidade;

    return 0;
}


int main(void) {
    Vetor *vetores = NULL;
    size_t quantidade = 0;
    if (criar_vetores("dados.csv", &vetores, &quantidade) != 0) {
        return 1;
    }
    for (size_t i = 0; i < quantidade; i++) {
        int tamanho = (int)vetores[i].tamanho;
        int *v = vetores[i].dados;
        int *c = malloc((size_t)tamanho * sizeof(int));
        if (c == NULL) {
            printf("Erro de alocacao de memoria!\n");
            liberar_vetores(vetores, quantidade);
            return 1;
        }
        // Ordena o vetor atual
        radixSort(v, c, tamanho);
        free(c);
    }
    liberar_vetores(vetores, quantidade);
    return 0;
}

