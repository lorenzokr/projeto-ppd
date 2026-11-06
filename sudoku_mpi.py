import os
import subprocess
import time
import zipfile
import numpy as np
from mpi4py import MPI

# ==========================================
# CONFIGURAÇÃO: Quantidade de Sudokus
# ==========================================
NUM_SUDOKUS = 10000  # Número total de sudokus para resolver


# 1. Função de Backtracking para resolver o Sudoku
def eh_valido(tabuleiro, lin, col, num):
    if num in tabuleiro[lin, :] or num in tabuleiro[:, col]:
        return False
    inicio_lin, inicio_col = 3 * (lin // 3), 3 * (col // 3)
    if num in tabuleiro[inicio_lin : inicio_lin + 3, inicio_col : inicio_col + 3]:
        return False
    return True


def resolver_sudoku(tabuleiro):
    vazios = np.argwhere(tabuleiro == 0)
    if len(vazios) == 0:
        return True  # Todos os quadros preenchidos

    lin, col = vazios[0]
    for num in range(1, 10):
        if eh_valido(tabuleiro, lin, col, num):
            tabuleiro[lin, col] = num
            if resolver_sudoku(tabuleiro):
                return True
            tabuleiro[lin, col] = 0  # Backtrack
    return False


# 2. Função para baixar e carregar os dados
def obter_linhas_sudoku():
    zip_path = os.path.expanduser("~/Downloads/sudoku.zip")
    extract_dir = os.path.expanduser("~/Downloads")
    csv_path = os.path.join(extract_dir, "sudoku.csv")

    url = "https://www.kaggle.com/api/v1/datasets/download/bryanpark/sudoku"
    command = ["curl", "-L", "-o", zip_path, url]

    # Download via curl se o arquivo ainda não existir no disco
    if not os.path.exists(csv_path):
        print(f"[Rank 0] Downloading dataset to {zip_path}...")
        subprocess.run(command, check=True)
        print("[Rank 0] Download complete!")

        print(f"[Rank 0] Extracting {zip_path}...")
        with zipfile.ZipFile(zip_path, "r") as zip_ref:
            zip_ref.extractall(extract_dir)
        print(f"[Rank 0] File extracted to {csv_path}")

    # Leitura do arquivo CSV local
    print(f"[Rank 0] Lendo dados de {csv_path}...")
    with open(csv_path, "r", encoding="utf-8") as f:
        conteudo = f.read().splitlines()

    return conteudo


# INICIALIZAÇÃO DO AMBIENTE MPI
comm = MPI.COMM_WORLD
rank = comm.Get_rank()
size = comm.Get_size()

quizzes_divididos = None
solutions_divididas = None

# Apenas o processo principal (Rank 0) carrega os dados e prepara a divisão
if rank == 0:
    print("\n" + "=" * 50)
    print(f" PARALELIZAÇÃO COM OPENMPI ({size} PROCESSOS)")
    print(f" RESOLVENDO OS {NUM_SUDOKUS} PRIMEIROS SUDOKUS")
    print("=" * 50)

    content = obter_linhas_sudoku()
    linhas = content[1 : NUM_SUDOKUS + 1]

    quizzes = np.zeros((NUM_SUDOKUS, 81), dtype=np.int32)
    solutions = np.zeros((NUM_SUDOKUS, 81), dtype=np.int32)

    for i, line in enumerate(linhas):
        quiz, solution = line.split(",")
        for j, (q, s) in enumerate(zip(quiz, solution)):
            quizzes[i, j] = int(q)
            solutions[i, j] = int(s)

    quizzes = quizzes.reshape((-1, 9, 9))
    solutions = solutions.reshape((-1, 9, 9))

    # Divide os arrays igualmente entre o número de processos disponíveis (size)
    quizzes_divididos = np.array_split(quizzes, size)
    solutions_divididas = np.array_split(solutions, size)

# Distribui (Scatter) as fatias de dados para cada processo
meus_quizzes = comm.scatter(quizzes_divididos, root=0)
minhas_solutions = comm.scatter(solutions_divididas, root=0)

# Sincroniza todos os nós antes de iniciar a medição de tempo
comm.Barrier()
tempo_inicio = time.perf_counter()

# Cada processo resolve a sua fração dos Sudokus
corretos_locais = 0
total_local = len(meus_quizzes)

for idx in range(total_local):
    tabuleiro = meus_quizzes[idx].copy()
    solucao_esperada = minhas_solutions[idx]

    if resolver_sudoku(tabuleiro):
        if np.array_equal(tabuleiro, solucao_esperada):
            corretos_locais += 1

tempo_fim = time.perf_counter()
tempo_local = tempo_fim - tempo_inicio

print(f"[Nó {rank}] Processou {total_local} sudokus em {tempo_local:.2f}s ({corretos_locais} corretos).")

# Agrupa e reduz as estatísticas no processo principal (Rank 0)
total_corretos = comm.reduce(corretos_locais, op=MPI.SUM, root=0)
tempo_maximo = comm.reduce(tempo_local, op=MPI.MAX, root=0)

if rank == 0:
    print("\n" + "=" * 50)
    print(f" RESUMO DA EXECUÇÃO PARALELA")
    print("=" * 50)
    print(f" Total de Sudokus resolvidos com sucesso: {total_corretos}/{NUM_SUDOKUS}")
    print(f" Tempo total decorrido (maior tempo entre nós): {tempo_maximo:.2f} segundos")
    print("=" * 50 + "\n")
