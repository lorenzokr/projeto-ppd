
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

// Combina duas partes já ordenadas do vetor.
void merge(int *v, int *aux, int inicio, int meio, int fim) {
    int i = inicio;
    int j = meio;
    int k = inicio;

    while (i < meio && j < fim) {
        if (v[i] <= v[j]) {
            aux[k++] = v[i++];
        } else {
            aux[k++] = v[j++];
        }
    }

    while (i < meio) {
        aux[k++] = v[i++];
    }

    while (j < fim) {
        aux[k++] = v[j++];
    }

    for (k = inicio; k < fim; k++) {
        v[k] = aux[k];
    }
}

// Merge Sort paralelo usando tarefas OpenMP.
// O intervalo tratado e [inicio, fim), ou seja, fim nao e incluido.
void mergeSortParalelo(int *v, int *aux, int inicio, int fim) {
    int tamanho = fim - inicio;

    if (tamanho <= 1) {
        return;
    }

    int meio = inicio + tamanho / 2;

    // Para partes pequenas, evita o custo de criar tarefas.
    if (tamanho <= 3072) {
        mergeSortParalelo(v, aux, inicio, meio);
        mergeSortParalelo(v, aux, meio, fim);
    } else {
        #pragma omp task shared(v, aux) firstprivate(inicio, meio)
        mergeSortParalelo(v, aux, inicio, meio);

        #pragma omp task shared(v, aux) firstprivate(meio, fim)
        mergeSortParalelo(v, aux, meio, fim);

        #pragma omp taskwait
    }

    merge(v, aux, inicio, meio, fim);
}

void mergeSort(int *v, int *aux, int n) {
    if (n <= 1) {
        return;
    }

    #pragma omp parallel
    {
        #pragma omp single
        mergeSortParalelo(v, aux, 0, n);
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
        mergeSort(v, c, tamanho);
        free(c);
    }
    liberar_vetores(vetores, quantidade);
    return 0;
}

