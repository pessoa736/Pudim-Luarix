# Comandos Lua Dinamicos (KCMDS)

Comandos Lua dinamicos sao carregados de `commands.lua` (arquivo no KFS) e expostos pela tabela global `KCMDS`.

Pense nesses comandos como receitas da casa: o forno (kernel) ja vem pronto, mas voce troca o sabor do menu sem desmontar a cozinha.

## Como funcionam

- O terminal tenta esses comandos quando a tabela nativa nao reconhece a linha.
- `lcmds` mostra os nomes atualmente disponiveis.
- O arquivo padrao vem embutido em `commands_lua.inc` e pode ser substituido no proprio KFS.

## Comandos padrao

| Comando | Uso | O que faz |
|---|---|---|
| `version` | `version` | Mostra versao do kernel Pudim-Luarix. |
| `sysstats` | `sysstats` | Mostra painel de sistema: CPU, RAM, heap, ROM, processos e uptime. |
| `memmap` | `memmap` | Lista mapa de memoria E820 (`base`, `length`, `type`, `kind`). |
| `runq` | `runq` | Executa ate 64 ticks do escalonador e resume quantos processos rodaram. |
| `test` | `test proc` | Teste simples de processo/sincronizacao. |
| `test` | `test page [count]` | Aloca paginas de 4096 bytes, reporta e libera ao final. |

## Regras de invocacao

- Nome do comando aceita apenas `[a-zA-Z0-9_]`.
- Limite de argumentos por linha: 8.
- Limite de tamanho de script carregado por comando: 512 bytes.
- Argumentos com espacos podem ser envolvidos por aspas simples ou duplas.

## Exemplo

```text
plterm> lcmds
plterm> sysstats
plterm> test page 32
```
