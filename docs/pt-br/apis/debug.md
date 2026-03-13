# API debug

Tabela global: `debug`

Esta camada e a bancada de confeitaria para inspeção: ajuda a olhar camadas de pilha, memoria e historico do terminal.

## Funcoes

| Funcao | Assinatura | Retorno |
|---|---|---|
| `locals` | `debug.locals([level])` | `table` de locais (ou mensagem/nil em falha) |
| `stack` | `debug.stack()` | `string` com traceback |
| `globals` | `debug.globals()` | `table` (`nome_global -> tipo`) |
| `timer_start` | `debug.timer_start()` | sem retorno |
| `timer_stop` | `debug.timer_stop()` | `integer` ms |
| `timer_mark` | `debug.timer_mark([label])` | `boolean` |
| `timer_reset` | `debug.timer_reset()` | sem retorno |
| `timer_report` | `debug.timer_report()` | `table` de marcos `{ label, ms }` |
| `heap` | `debug.heap()` | `table` com estatisticas do heap |
| `lua_mem` | `debug.lua_mem()` | `table` com uso da VM Lua |
| `fragmentation` | `debug.fragmentation()` | `number` (0.0 a 1.0) |
| `history` | `debug.history()` | `table` com historico de comandos |

## Estruturas de retorno

`debug.heap()`:

- `blocks`
- `free_blocks`
- `largest_free`
- `free_bytes`
- `total_bytes`

`debug.lua_mem()`:

- `kb`
- `bytes_remainder`

## Exemplo

```lua
debug.timer_start()
-- trecho em analise
debug.timer_mark("massa pronta")
print(debug.timer_stop(), "ms")

local h = debug.heap()
print(h.free_bytes, "/", h.total_bytes)
```
