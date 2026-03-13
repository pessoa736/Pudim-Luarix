# API memory

Tabela global: `memory`

## Funcoes

| Funcao | Assinatura | Retorno |
|---|---|---|
| `allocate` | `memory.allocate(size)` | `integer` endereco (ponteiro) ou `0` |
| `free` | `memory.free(ptr)` | `boolean` |
| `protect` | `memory.protect(ptr, flags)` | `boolean` |
| `dump_stats` | `memory.dump_stats()` | `string` com resumo |

## Observacoes

- Ponteiros sao transportados para Lua como inteiros.
- `protect` delega para a camada de memoria do kernel (`kmemory_protect`).
- `allocate` retorna `0` em falha/entrada invalida.

## Exemplo

```lua
local p = memory.allocate(4096)
if p ~= 0 then
    print("pagina", p)
    memory.free(p)
end
print(memory.dump_stats())
```
