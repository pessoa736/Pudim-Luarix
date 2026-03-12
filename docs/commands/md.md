# md — Menu de Despensa (Pantry Menu)

Lista os arquivos armazenados no sistema de arquivos em memória (PLFS).
Como espiar a despensa para ver quais ingredientes de pudim estão na prateleira.

## Uso

```
md [-l|-s|-p]
```

## Modos de visualização

| Flag | Descrição | Saída |
|------|-----------|-------|
| *(nenhuma)* | Mostra apenas os nomes dos arquivos | `receita.txt` |
| `-l` | Visão detalhada — permissões, uid, gid, tamanho e nome | `rwxr-xr--  0 0  42  receita.txt` |
| `-s` | Visão de tamanho — tamanho e nome | `42  receita.txt` |
| `-p` | Visão de permissões — permissões e nome | `rwxr-xr--  receita.txt` |

## Exemplos

```
> md
receita.txt
boot.log
config.lua

> md -l
rwxr-xr--  0 0  42  receita.txt
rwxr-xr--  0 0  1024  boot.log
rwxr-xr--  0 0  88  config.lua

> md -s
42  receita.txt
1024  boot.log
88  config.lua

> md -p
rwxr-xr--  receita.txt
rwxr-xr--  boot.log
rwxr-xr--  config.lua
```

## Analogia

| Modo | Analogia com pudim |
|------|--------------------|
| `md` | Ler os rótulos da despensa — só os nomes |
| `md -l` | Inspecionar cada pote em detalhe |
| `md -s` | Conferir o peso dos ingredientes |
| `md -p` | Ler as permissões da receita |

## Notas

- Se não houver arquivos, exibe `(empty)`.
- Flags inválidas exibem a mensagem de uso: `usage: md [-l|-s|-p]`.
