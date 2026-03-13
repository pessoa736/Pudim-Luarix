# Comandos do Terminal (plterm)

Este guia descreve os comandos nativos registrados em `g_kterm_commands` no terminal do kernel.

Se o kernel fosse uma confeitaria, este terminal seria a bancada principal: aqui voce mede ingredientes (memoria), organiza camadas (processos) e finaliza a cobertura (persistencia).

## Visao geral

- Prompt: `plterm>`
- Buffer maximo de linha: 250 caracteres uteis.
- Comandos desconhecidos: o terminal tenta executar como comando Lua dinamico (`KCMDS`) antes de retornar `unknown command`.

## Comandos nativos

| Comando | Uso | Retorno esperado | Observacoes |
|---|---|---|---|
| `help` | `help` | Lista de comandos | Inclui comandos nativos e nomes de comandos Lua (`KCMDS`). |
| `clear` | `clear` | Limpa tela | Usa o backend de display atual (VGA/serial). |
| `md` | `md [-l|-s|-p]` | Lista arquivos | `-l` mostra modo/uid/gid/tamanho, `-s` mostra tamanho, `-p` mostra permissoes. |
| `ps` | `ps` | Lista processos | Mostra `pid` e estado (`ready`, `running`, `error`, `dead`). |
| `count` | `count` | Numero de arquivos | Total do KFS em memoria. |
| `lcmds` | `lcmds` | Lista comandos Lua | Lidos da tabela global `KCMDS`. |
| `save` | `save` | `saved to disk` ou erro | Persiste KFS se houver disco de persistencia. |
| `whoami` | `whoami` | Nome do usuario atual | Terminal roda como uid `0` (root). |
| `write` | `write <name> <content>` | `ok` ou `fail` | Sobrescreve/cria arquivo no KFS. |
| `read` | `read <name>` | Conteudo ou `not found` | Leitura textual do arquivo. |
| `append` | `append <name> <content>` | `ok` ou `fail` | Adiciona conteudo ao final do arquivo. |
| `rm` | `rm <name>` | `ok` ou `fail` | Remove arquivo do KFS. |
| `size` | `size <name>` | Tamanho em bytes | Retorna `0` para arquivo ausente. |
| `lua` | `lua <code>` | Saida do script | Aceita trecho Lua; aspas externas sao removidas se presentes. |
| `chmod` | `chmod <name> <mode>` | `ok` ou `fail` | Modo em octal (`0` a `777`). |
| `chown` | `chown <name> <uid> <gid>` | `ok` ou `fail` | Ajusta dono e grupo do arquivo. |
| `getperm` | `getperm <name>` | `mode/uid/gid` | Mostra permissoes atuais do arquivo. |
| `history` | `history` | Historico numerado | Exibe as "camadas" de comandos executados. |
| `inspect` | `inspect` | Estatisticas de heap | Blocos, blocos livres, maior bloco livre, bytes livres e totais. |
| `exit` | `exit` | `bye` | Encerra o loop do terminal. |

## Fluxo de resolucao de comando

1. O terminal tenta casar o comando com a tabela nativa.
2. Se nao casar, tenta `klua_cmd_run_line(line)`.
3. Se ainda falhar, retorna `unknown command`.

## Exemplos rapidos

```text
plterm> md -l
plterm> write notes "massa pronta"
plterm> read notes
plterm> chmod notes 644
plterm> history
```
