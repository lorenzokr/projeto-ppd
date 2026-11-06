
#!/bin/bash

# Configuracoes
REPETICOES=${1:-10}
QUANTIDADE=${2:-1000000}
ENTRADA=${3:-sudoku.csv}
EXECUTAVEL=${4:-./sudoku_omp_simples}

RESULTADOS="resultados_benchmark.csv"

echo "threads,repeticao,tempo" > "$RESULTADOS"

# Testa 1, 2 e 4 threads
for T in 1 2 4; do
    echo ""
    echo "===== $T THREAD(S) ====="

    for ((R=1; R<=REPETICOES; R++)); do
        echo -n "Execucao $R/$REPETICOES: "

        SAIDA=$("$EXECUTAVEL" "$ENTRADA" /dev/null "$QUANTIDADE" "$T" 2>&1)

        if [ $? -ne 0 ]; then
            echo "ERRO"
            echo "$SAIDA"
            continue
        fi

        TEMPO=$(echo "$SAIDA" | awk '/Tempo de resolucao:/ {print $(NF-1)}')

        if [ -z "$TEMPO" ]; then
            echo "Nao foi possivel obter o tempo"
            continue
        fi

        if ! echo "$SAIDA" | grep -q \
            "Sudokus resolvidos corretamente: $QUANTIDADE/$QUANTIDADE"; then
            echo "Falha na validacao"
            continue
        fi

        echo "$T,$R,$TEMPO" >> "$RESULTADOS"
        echo "$TEMPO segundos"
    done
done

# Calcula estatisticas
python3 - "$RESULTADOS" <<'PY'
import csv
import statistics
import sys

dados = {1: [], 2: [], 4: []}

with open(sys.argv[1], newline="") as arquivo:
    for linha in csv.DictReader(arquivo):
        dados[int(linha["threads"])].append(
            float(linha["tempo"])
        )

if not dados[1]:
    print("Sem resultados validos com 1 thread.")
    sys.exit(1)

media_base = statistics.mean(dados[1])

print("\n========== RESULTADOS ==========")
print(f"{'Threads':<10}{'Media':<14}{'Mediana':<14}{'Speedup':<12}{'Eficiencia'}")

for t in (1, 2, 4):
    tempos = dados[t]

    if not tempos:
        print(f"{t:<10}Sem resultados")
        continue

    media = statistics.mean(tempos)
    mediana = statistics.median(tempos)
    speedup = media_base / media
    eficiencia = speedup / t * 100

    print(
        f"{t:<10}{media:<14.6f}{mediana:<14.6f}"
        f"{speedup:<12.2f}x{eficiencia:.2f}%"
    )

print("\nSpeedup = tempo medio de 1 thread / tempo medio atual")
print("Eficiencia = speedup / numero de threads * 100")
print("\nTempos individuais salvos em resultados_benchmark.csv")
PY
