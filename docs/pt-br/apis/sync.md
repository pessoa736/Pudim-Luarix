# API sync

Tabela global: `sync`

## Construtores

| Funcao | Assinatura | Retorno |
|---|---|---|
| `spinlock` | `sync.spinlock()` | objeto lock |
| `mutex` | `sync.mutex()` | objeto mutex |

## Metodos dos objetos

Tanto `spinlock` quanto `mutex` expõem os mesmos metodos:

- `lock()`
- `unlock()`
- `acquire()` (alias de `lock`)
- `release()` (alias de `unlock`)

## Observacoes

- Uso invalido de lock gera erro Lua.
- Objetos sao implementados com userdata contendo ponteiro C.

## Exemplo

```lua
local m = sync.mutex()
m:lock()
-- secao critica
m:unlock()
```
