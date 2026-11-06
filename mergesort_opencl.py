#!/usr/bin/env python3
"""MergeSort paralelo com PyOpenCL.

Entrada: dados.csv
- um vetor por linha;
- valores inteiros nao negativos separados por ponto e virgula (;).

A leitura e feita em Python. As etapas de intercalação do MergeSort sao
executadas paralelamente no dispositivo OpenCL.
"""

import sys
import time
from pathlib import Path

import numpy as np
import pyopencl as cl

ARQUIVO_ENTRADA = "dados.csv"
ARQUIVO_SAIDA = "dados_ordenados.csv"


KERNEL_MERGE = r"""
// Cada work-item escreve exatamente uma posicao do vetor de saida.
// Para manter a estabilidade:
// - elemento da metade esquerda: conta elementos menores na direita;
// - elemento da metade direita: conta elementos menores ou iguais na esquerda.

__kernel void merge_pass(
    __global const uint *entrada,
    __global uint *saida,
    const uint n,
    const uint largura
) {
    const uint gid = get_global_id(0);
    if (gid >= n) return;

    // Identifica o par de blocos que contem este elemento.
    const uint bloco = gid / (2 * largura);
    const uint inicio = bloco * (2 * largura);
    const uint meio = min(inicio + largura, n);
    const uint fim = min(inicio + 2 * largura, n);

    uint destino;

    if (gid < meio) {
        // Elemento pertencente a metade esquerda.
        const uint valor = entrada[gid];
        uint baixo = meio;
        uint alto = fim;

        // lower_bound: primeira posicao cujo valor >= elemento atual.
        while (baixo < alto) {
            const uint m = baixo + (alto - baixo) / 2;
            if (entrada[m] < valor)
                baixo = m + 1;
            else
                alto = m;
        }

        const uint quantidade_antes = baixo - meio;
        destino = inicio + (gid - inicio) + quantidade_antes;
    } else {
        // Elemento pertencente a metade direita.
        const uint valor = entrada[gid];
        uint baixo = inicio;
        uint alto = meio;

        // upper_bound: primeira posicao cujo valor > elemento atual.
        while (baixo < alto) {
            const uint m = baixo + (alto - baixo) / 2;
            if (entrada[m] <= valor)
                baixo = m + 1;
            else
                alto = m;
        }

        const uint quantidade_antes = baixo - inicio;
        destino = inicio + quantidade_antes + (gid - meio);
    }

    saida[destino] = entrada[gid];
}
"""


def ler_vetores(nome_arquivo):
    """Le o arquivo no mesmo formato usado pelo programa C/OpenMP."""
    vetores = []

    with open(nome_arquivo, "r", encoding="utf-8") as arquivo:
        for numero_linha, linha in enumerate(arquivo, start=1):
            linha = linha.strip()

            # Ignora linhas vazias, assim como a versao em C.
            if not linha:
                continue

            valores = []
            for token in linha.split(";"):
                token = token.strip()
                # strtok_r, usado na versao C, ignora campos vazios entre
                # separadores ou no final da linha.
                if not token:
                    continue

                try:
                    valor = int(token, 10)
                except ValueError as erro:
                    raise ValueError(
                        f"Valor invalido na linha {numero_linha}: {token!r}"
                    ) from erro

                # A versao C aceita apenas [0, INT_MAX].
                if valor < 0 or valor > 2_147_483_647:
                    raise ValueError(
                        f"Valor fora do intervalo permitido na linha "
                        f"{numero_linha}: {valor}"
                    )

                valores.append(valor)

            if not valores:
                raise ValueError(f"Vetor vazio na linha {numero_linha}.")

            vetores.append(np.asarray(valores, dtype=np.uint32))

    return vetores


def escolher_dispositivo():
    """Lista os dispositivos OpenCL e permite escolher um."""
    plataformas = cl.get_platforms()
    dispositivos = []

    for plataforma in plataformas:
        for dispositivo in plataforma.get_devices():
            dispositivos.append((plataforma, dispositivo))

    if not dispositivos:
        raise RuntimeError("Nenhum dispositivo OpenCL foi encontrado.")

    print("Dispositivos OpenCL encontrados:")
    for indice, (plataforma, dispositivo) in enumerate(dispositivos):
        tipo = cl.device_type.to_string(dispositivo.type)
        print(f"[{indice}] {dispositivo.name} | {tipo} | {plataforma.name}")

    # Permite escolher pelo primeiro argumento: python mergesort_opencl.py 1
    if len(sys.argv) > 1:
        try:
            escolha = int(sys.argv[1])
        except ValueError as erro:
            raise ValueError("Informe um indice numerico de dispositivo.") from erro
        if escolha < 0 or escolha >= len(dispositivos):
            raise ValueError("Indice de dispositivo invalido.")
    else:
        # Por padrao, usa o primeiro dispositivo listado pelo OpenCL.
        escolha = 0

    plataforma, dispositivo = dispositivos[escolha]
    print(f"\nUsando: {dispositivo.name}\n")
    return dispositivo


def merge_sort_opencl(vetor, contexto, fila, programa):
    """Ordena um vetor uint32 com MergeSort bottom-up no OpenCL."""
    n = int(vetor.size)
    if n <= 1:
        return vetor.copy()

    mf = cl.mem_flags
    buffer_a = cl.Buffer(
        contexto,
        mf.READ_WRITE | mf.COPY_HOST_PTR,
        hostbuf=vetor
    )
    buffer_b = cl.Buffer(contexto, mf.READ_WRITE, vetor.nbytes)

    largura = 1
    origem = buffer_a
    destino = buffer_b

    while largura < n:
        programa.merge_pass(
            fila,
            (n,),
            None,
            origem,
            destino,
            np.uint32(n),
            np.uint32(largura),
        )

        origem, destino = destino, origem
        largura *= 2

    resultado = np.empty_like(vetor)
    cl.enqueue_copy(fila, resultado, origem)
    fila.finish()
    return resultado


def main():
    caminho_entrada = Path(ARQUIVO_ENTRADA)
    if not caminho_entrada.is_file():
        print(f"Erro: arquivo '{ARQUIVO_ENTRADA}' nao encontrado.", file=sys.stderr)
        return 1

    try:
        vetores = ler_vetores(caminho_entrada)
        if not vetores:
            print("Nenhum vetor encontrado no arquivo.", file=sys.stderr)
            return 1

        dispositivo = escolher_dispositivo()
        contexto = cl.Context([dispositivo])
        fila = cl.CommandQueue(contexto)
        programa = cl.Program(contexto, KERNEL_MERGE).build()

        resultados = []
        for indice, vetor in enumerate(vetores, start=1):
            inicio = time.perf_counter()
            ordenado = merge_sort_opencl(vetor, contexto, fila, programa)
            tempo = time.perf_counter() - inicio

            resultados.append(ordenado)
            print(
                f"Vetor {indice}: {vetor.size} elementos | "
                f"tempo: {tempo:.6f} segundos"
            )

        # Mantem a mesma organizacao: um vetor por linha e separador ';'.
        with open(ARQUIVO_SAIDA, "w", encoding="utf-8") as arquivo:
            for vetor in resultados:
                arquivo.write(";".join(str(int(valor)) for valor in vetor))
                arquivo.write("\n")

        print(f"\nOrdenacao concluida. Resultado salvo em '{ARQUIVO_SAIDA}'.")
        return 0

    except (OSError, ValueError, RuntimeError, cl.Error) as erro:
        print(f"Erro: {erro}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
