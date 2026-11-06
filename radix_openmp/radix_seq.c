#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

typedef struct {
    int *dados;
    size_t tamanho;
} Vetor;

// Counting Sort sequencial
void countingSort(int *v, int *c, int n, int exp) {
    int contagem[10] = {0};

    // Conta a ocorrencia de cada digito
    for (int i = 0; i < n; i++) {
        int digito = (v[i] / exp) % 10;
        contagem[digito]++;
    }

    // Calcula as posicoes acumuladas
    for (int i = 1; i < 10; i++) {
        contagem[i] += contagem[i - 1];
    }

    // Distribui os elementos de tras para frente
    // para preservar a estabilidade
    for (int i = n - 1; i >= 0; i--) {
        int digito = (v[i] / exp) % 10;
        c[contagem[digito] - 1] = v[i];
        contagem[digito]--;
    }

    // Copia o resultado para o vetor original
    for (int i = 0; i < n; i++) {
        v[i] = c[i];
    }
}

// Radix Sort sequencial
void radixSort(int *v, int *c, int n) {
    if (n <= 1) {
        return;
    }

    int maior = v[0];

    // Encontra o maior valor
    for (int i = 1; i < n; i++) {
        if (v[i] > maior) {
            maior = v[i];
        }
    }

    // Ordena por unidades, dezenas, centenas etc.
    for (int exp = 1; maior / exp > 0; exp *= 10) {
        countingSort(v, c, n, exp);

        // Evita overflow ao multiplicar exp por 10
        if (exp > maior / 10) {
            break;
        }
    }
}

// Libera a memoria de todos os vetores
void liberar_vetores(Vetor *vetores, size_t quantidade) {
    if (vetores == NULL) {
        return;
    }

    for (size_t i = 0; i < quantidade; i++) {
        free(vetores[i].dados);
    }

    free(vetores);
}

// Le cada linha do CSV como um vetor independente
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

        char *p = linha;

        // Ignora linhas vazias
        while (*p == ' ' || *p == '\t' ||
               *p == '\n' || *p == '\r') {
            p++;
        }

        if (*p == '\0') {
            continue;
        }

        // Aumenta o espaco para armazenar mais vetores
        if (quantidade == capacidade_vetores) {
            size_t nova_capacidade =
                capacidade_vetores == 0
                ? 4 : capacidade_vetores * 2;

            Vetor *temp = realloc(
                vetores,
                nova_capacidade * sizeof(Vetor)
            );

            if (temp == NULL) {
                perror("Erro de alocacao");

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
                    perror("Erro de alocacao");

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

    if (criar_vetores("dados.csv",
                      &vetores,
                      &quantidade) != 0) {
        return 1;
    }

    for (size_t i = 0; i < quantidade; i++) {
        if (vetores[i].tamanho > INT_MAX) {
            fprintf(stderr,
                    "Vetor grande demais para radixSort.\n");

            liberar_vetores(vetores, quantidade);
            return 1;
        }

        int tamanho = (int)vetores[i].tamanho;
        int *v = vetores[i].dados;

        int *c = malloc((size_t)tamanho * sizeof(int));

        if (c == NULL) {
            perror("Erro de alocacao de memoria");
            liberar_vetores(vetores, quantidade);
            return 1;
        }

        radixSort(v, c, tamanho);
        free(c);
    }

    liberar_vetores(vetores, quantidade);

    return 0;
}
