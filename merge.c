#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include<unistd.h>


#define TAM_LINHA 1000000
#define TAM_VETOR 10000000

int v[TAM_VETOR]; 

// Função que intercala (merge) duas metades ordenadas do vetor
void merge(int *v, int *c, int i, int m, int f) {
    int z, iv = i, ic = m + 1;

    for (z = i; z <= f; z++) {
        c[z] = v[z];
    }

    z = i;

    while (iv <= m && ic <= f) {
        if (c[iv] <= c[ic]) {
            v[z++] = c[iv++];
        } else {
            v[z++] = c[ic++];
        }
    }

    while (iv <= m) {
        v[z++] = c[iv++];
    }

    while (ic <= f) {
        v[z++] = c[ic++];
    }
}

// Função recursiva do Merge Sort
void mergeSort(int *v, int *c, int i, int f) {
    if (i >= f) return;

    int m = (i + f) / 2;

    mergeSort(v, c, i, m);
    mergeSort(v, c, m + 1, f);

    // Otimização: se já estiver ordenado, pula o merge
    if (v[m] <= v[m + 1]) return;

    merge(v, c, i, m, f);
}

int criar_vetor(int *v) {
    int i=0;

    //printf("Criar_vetor\n");

    FILE *arquivo = fopen("dados.csv", "r");
    if (arquivo == NULL) {
        printf("Erro ao abrir o arquivo.\n");
        return 1;
    }

    char linha[TAM_LINHA];

    // Lê linha por linha do arquivo
    while (fgets(linha, sizeof(linha), arquivo) != NULL) {
        // Remove a quebra de linha do final, se existir
        linha[strcspn(linha, "\n")] = 0;

        // Tokeniza a linha usando a vírgula como delimitador
        char *token = strtok(linha, ";");
        while (token != NULL) {
            //printf("Campo [%d]: %s\n", i, token);

	    v[i]=atoi(token);
	    i++;

            token = strtok(NULL, ";");
        }
        //printf("------------------------\n");
    }

    fclose(arquivo);
    return i;
}


int main() {
//    int v[TAM_VETOR]; 
    int i, j, aux, retorno,tamanho;
    int n = sizeof(v) / sizeof(v[0]);
    int *c; // = (int *) malloc(n * sizeof(int)); // Vetor auxiliar

    tamanho=criar_vetor(v);

    n = tamanho;
    c = (int *) malloc(n * sizeof(int)); // Vetor auxiliar

    if (c == NULL) {
        printf("Erro de alocação de memória!\n");
        return 1;
    }

/*
    printf("Vetor original:\n");
    for (i = 0; i < (n-1); i++) {
        printf("%d;", v[i]);
    }
    printf("%d\n", v[i]);

    sleep(5);
*/

    mergeSort(v, c, 0, n - 1);

 //   printf("Vetor ordenado:\n");

    for (i = 0; i < (n-1); i++) {
        printf("%d;", v[i]);
    }
    printf("%d\n", v[i]);

    free(c); // Libera a memória do vetor auxiliar
    return 0;
}
