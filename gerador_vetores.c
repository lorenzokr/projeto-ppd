#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <limits.h>

int main(int argc, char *argv[]) {
    int n, i, j;
    FILE *arquivo;
    long tamanho;
    // Verifica se o numero de vetores foi informado
    if (argc != 2) {
        printf("Uso: %s <numero_de_vetores>\n", argv[0]);
        return 1;
    }
    // Converte o argumento para inteiro
    char *fim;
    errno = 0;
    long quantidade = strtol(argv[1], &fim, 10);
    if (errno != 0 || *fim != '\0' ||
        quantidade <= 0 || quantidade > INT_MAX) {
        printf("Erro: informe um numero inteiro positivo.\n");
        return 1;
    }
    n = (int)quantidade;
    // Abre o arquivo em modo append (adicionar ao final)
    arquivo = fopen("dados.csv", "a");
    if (arquivo == NULL) {
        perror("Erro ao abrir dados.csv");
        return 1;
    }
    // Inicializa o gerador de numeros aleatorios
    srand((unsigned int)time(NULL));
    for (i = 0; i < n; i++) {
        // Gera tamanhos grandes e diferentes
        // Entre 1 milhao e 10 milhoes de inteiros
        tamanho = 1000000L + (long)i * 1000000L;
        printf("Gerando vetor %d de %d (%ld inteiros)...\n",
               i + 1, n, tamanho);
        for (j = 0; j < tamanho; j++) {
            int r = rand();
            fprintf(arquivo, "%d", r);
            if (j < tamanho - 1) {
                fprintf(arquivo, ";");
            }
        }
        // Cada vetor ocupa uma linha
        fprintf(arquivo, "\n");
    }
    fclose(arquivo);
    printf("Concluido! %d vetores adicionados a dados.csv.\n", n);
    return 0;
}

