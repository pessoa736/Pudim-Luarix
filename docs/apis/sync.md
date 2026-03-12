# API sync — Sincronização (Spinlocks e Mutexes)

Módulo Lua para primitivas de sincronização entre processos.

> Implementação C: `include/APISLua/ksynclua.c`

## Funções

### `sync.spinlock()`

Cria um novo spinlock.

**Retorno**: `userdata` (objeto spinlock)

```lua
local lock = sync.spinlock()
lock:lock()
-- seção crítica
lock:unlock()
```

---

### `sync.mutex()`

Cria um novo mutex.

**Retorno**: `userdata` (objeto mutex)

```lua
local m = sync.mutex()
m:acquire()
-- seção crítica
m:release()
```

---

## Métodos dos objetos

Tanto spinlock quanto mutex possuem os mesmos métodos:

| Método | Alias | Descrição |
|--------|-------|-----------|
| `:lock()` | `:acquire()` | Adquire o lock (espera ativa no spinlock) |
| `:unlock()` | `:release()` | Libera o lock |

## Diferenças

| Tipo | Mecanismo | Uso ideal |
|------|-----------|-----------|
| Spinlock | Espera ativa (busy-wait) | Seções críticas curtas |
| Mutex | Spinlock interno com controle de estado | Seções críticas mais longas |

## Exemplo

```lua
local lock = sync.spinlock()

process.create(function()
    lock:lock()
    print("processo 1 na seção crítica")
    lock:unlock()
end)

process.create(function()
    lock:lock()
    print("processo 2 na seção crítica")
    lock:unlock()
end)
```
