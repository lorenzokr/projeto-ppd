

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <quantidade_de_linhas>\n", argv[0]);
        return 1;
    }
    int quantidade_linhas = atoi(argv[1]);
    if (quantidade_linhas <= 0) {
        printf("Erro: informe uma quantidade de linhas positiva.\n");
        return 1;
    }
    FILE *arquivo = fopen("dados.csv", "w");
    if (arquivo == NULL) {
        perror("Erro ao criar dados.csv");
        return 1;
    }
    srand((unsigned int) time(NULL));
    for (int i = 0; i < quantidade_linhas; i++) {
        int tamanho = 100 + rand() % 9901;
        for (int j = 0; j < tamanho; j++) {
            int numero = rand() % 1000000;
            fprintf(arquivo, "%d", numero);
            if (j < tamanho - 1) {
                fprintf(arquivo, ";");
            }
        }
        fprintf(arquivo, "\n");
    }
    fclose(arquivo);
    printf("Arquivo dados.csv criado com %d linhas.\n", quantidade_linhas);
    return 0;
}

