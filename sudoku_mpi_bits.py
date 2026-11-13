import os
import time
import numpy as np
from mpi4py import MPI


# ==========================================
# CONFIGURACAO: Quantidade de Sudokus
# ==========================================
NUM_SUDOKUS = 100000  # Numero total de sudokus para resolver

SUDOKU_N = 9
CELL_COUNT = 81
FULL_MASK = 0x3FE  # Bits 1 ate 9 ligados; bit 0 representa casa vazia.


# ==========================================
# 1. Resolucao de Sudoku usando mascaras de bits
# ==========================================

def box_index(lin, col):
    """Retorna o indice do bloco 3x3 da posicao."""
    return (lin // 3) * 3 + (col // 3)


def inicializar_estado(tabuleiro):
    """
    Cria as mascaras de linhas, colunas e blocos.
    Retorna None se as pistas iniciais tiverem algum conflito.
    """
    celulas = [int(valor) for valor in tabuleiro]
    mascara_linhas = [0] * SUDOKU_N
    mascara_colunas = [0] * SUDOKU_N
    mascara_blocos = [0] * SUDOKU_N

    for pos, valor in enumerate(celulas):
        if valor == 0:
            continue

        if valor < 1 or valor > 9:
            return None

        lin, col = divmod(pos, SUDOKU_N)
        bloco = box_index(lin, col)
        bit = 1 << valor

        # Se o bit ja estiver ligado, existe um numero repetido.
        if (mascara_linhas[lin] & bit or
                mascara_colunas[col] & bit or
                mascara_blocos[bloco] & bit):
            return None

        mascara_linhas[lin] |= bit
        mascara_colunas[col] |= bit
        mascara_blocos[bloco] |= bit

    return celulas, mascara_linhas, mascara_colunas, mascara_blocos


def escolher_mrv(celulas, mascara_linhas, mascara_colunas, mascara_blocos):
    """
    Escolhe a casa vazia com menos candidatos (MRV).
    Retorna (posicao, candidatos).
    posicao = -1 indica tabuleiro completo.
    posicao = -2 indica que alguma casa nao possui candidatos.
    """
    melhor_pos = -1
    menor_quantidade = 10
    melhores_candidatos = 0

    for pos in range(CELL_COUNT):
        if celulas[pos] != 0:
            continue

        lin, col = divmod(pos, SUDOKU_N)
        bloco = box_index(lin, col)

        usados = (mascara_linhas[lin] |
                  mascara_colunas[col] |
                  mascara_blocos[bloco])

        candidatos = FULL_MASK & (~usados)
        quantidade = candidatos.bit_count()

        if quantidade == 0:
            return -2, 0

        if quantidade < menor_quantidade:
            menor_quantidade = quantidade
            melhor_pos = pos
            melhores_candidatos = candidatos

            # Nao existe escolha melhor que uma casa com apenas um candidato.
            if quantidade == 1:
                break

    return melhor_pos, melhores_candidatos


def backtracking(celulas, mascara_linhas, mascara_colunas, mascara_blocos):
    """Resolve o Sudoku utilizando backtracking e operacoes de bits."""
    pos, candidatos = escolher_mrv(
        celulas, mascara_linhas, mascara_colunas, mascara_blocos
    )

    if pos == -2:
        return False

    if pos == -1:
        return True

    lin, col = divmod(pos, SUDOKU_N)
    bloco = box_index(lin, col)

    while candidatos:
        # Isola o bit ligado mais a direita (um dos candidatos).
        bit = candidatos & -candidatos
        candidatos &= candidatos - 1

        # A posicao do bit corresponde ao numero que sera colocado.
        valor = bit.bit_length() - 1

        celulas[pos] = valor
        mascara_linhas[lin] |= bit
        mascara_colunas[col] |= bit
        mascara_blocos[bloco] |= bit

        if backtracking(celulas, mascara_linhas, mascara_colunas, mascara_blocos):
            return True

        # Desfaz a tentativa antes de testar o proximo candidato.
        celulas[pos] = 0
        mascara_linhas[lin] &= ~bit
        mascara_colunas[col] &= ~bit
        mascara_blocos[bloco] &= ~bit

    return False


def resolver_sudoku(tabuleiro):
    """Prepara o estado e resolve um unico tabuleiro."""
    estado = inicializar_estado(tabuleiro)

    if estado is None:
        return False

    celulas, mascara_linhas, mascara_colunas, mascara_blocos = estado

    if backtracking(celulas, mascara_linhas, mascara_colunas, mascara_blocos):
        return celulas

    return None


# ==========================================
# 2. Leitura do arquivo CSV local
# ==========================================

def obter_linhas_sudoku():
    # O CSV deve estar na mesma pasta deste script.
    pasta_script = os.path.dirname(os.path.abspath(__file__))
    csv_path = os.path.join(pasta_script, "sudoku.csv")

    print(f"[Rank 0] Lendo dados de {csv_path}...")

    with open(csv_path, "r", encoding="utf-8") as arquivo:
        return arquivo.read().splitlines()


# ==========================================
# 3. Inicializacao do ambiente MPI
# ==========================================

comm = MPI.COMM_WORLD
rank = comm.Get_rank()
size = comm.Get_size()

quizzes_divididos = None
solutions_divididas = None

# Somente o processo principal carrega e prepara os dados.
if rank == 0:
    print("\n" + "=" * 50)
    print(f" PARALELIZACAO COM OPENMPI ({size} PROCESSOS)")
    print(f" RESOLVENDO OS {NUM_SUDOKUS} PRIMEIROS SUDOKUS")
    print("=" * 50)

    content = obter_linhas_sudoku()
    linhas = content[1:NUM_SUDOKUS + 1]

    quantidade_lida = len(linhas)
    if quantidade_lida != NUM_SUDOKUS:
        raise ValueError(
            f"Foram solicitados {NUM_SUDOKUS} Sudokus, "
            f"mas o arquivo possui somente {quantidade_lida} registros."
        )

    quizzes = np.zeros((NUM_SUDOKUS, 81), dtype=np.int32)
    solutions = np.zeros((NUM_SUDOKUS, 81), dtype=np.int32)

    for i, line in enumerate(linhas):
        quiz, solution = line.strip().split(",")

        if len(quiz) != 81 or len(solution) != 81:
            raise ValueError(f"Registro {i + 1} possui tamanho invalido.")

        quizzes[i] = [int(digito) for digito in quiz]
        solutions[i] = [int(digito) for digito in solution]

    quizzes = quizzes.reshape((-1, 9, 9))
    solutions = solutions.reshape((-1, 9, 9))

    # Divide os arrays entre os processos disponiveis.
    quizzes_divididos = np.array_split(quizzes, size)
    solutions_divididas = np.array_split(solutions, size)


# Cada processo recebe sua parte dos dados.
meus_quizzes = comm.scatter(quizzes_divididos, root=0)
minhas_solutions = comm.scatter(solutions_divididas, root=0)


# ==========================================
# 4. Resolucao paralela dos Sudokus
# ==========================================

# Sincroniza os processos antes de iniciar a medicao.
comm.Barrier()
tempo_inicio = time.perf_counter()

corretos_locais = 0
total_local = len(meus_quizzes)

for idx in range(total_local):
    tabuleiro = meus_quizzes[idx].reshape(CELL_COUNT)
    solucao_esperada = minhas_solutions[idx].reshape(CELL_COUNT)

    solucao_encontrada = resolver_sudoku(tabuleiro)

    if (solucao_encontrada is not None and
            np.array_equal(solucao_encontrada, solucao_esperada)):
        corretos_locais += 1

tempo_fim = time.perf_counter()
tempo_local = tempo_fim - tempo_inicio

print(
    f"[No {rank}] Processou {total_local} sudokus em "
    f"{tempo_local:.2f}s ({corretos_locais} corretos)."
)


# ==========================================
# 5. Agregacao dos resultados no Rank 0
# ==========================================

total_corretos = comm.reduce(corretos_locais, op=MPI.SUM, root=0)
tempo_maximo = comm.reduce(tempo_local, op=MPI.MAX, root=0)

if rank == 0:
    print("\n" + "=" * 50)
    print(" RESUMO DA EXECUCAO PARALELA")
    print("=" * 50)
    print(
        f" Total de Sudokus resolvidos com sucesso: "
        f"{total_corretos}/{NUM_SUDOKUS}"
    )
    print(
        f" Tempo total decorrido (maior tempo entre nos): "
        f"{tempo_maximo:.2f} segundos"
    )
    print("=" * 50 + "\n")
