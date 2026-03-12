# inspect — Inspecionar o Heap

Mostra estatísticas do alocador de memória do heap.

## Uso

```
inspect
```

## Saída

```
> inspect
blocks=5 free=251 largest=4096 free_bytes=1044480 total=1048576
```

## Campos

| Campo | Descrição |
|-------|-----------|
| `blocks` | Blocos de memória alocados |
| `free` | Blocos livres disponíveis |
| `largest` | Maior bloco livre contíguo (bytes) |
| `free_bytes` | Total de bytes livres |
| `total` | Total de bytes no heap |

## Analogia

Como inspecionar a qualidade da massa do pudim — ver a consistência do heap.
