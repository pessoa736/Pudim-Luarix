# API debug — Depuração e Profiling (Palito de Pudim)

Módulo Lua para introspecção, profiling e diagnóstico do kernel.

> Implementação C: `include/APISLua/kdebuglua.c`

## Introspecção

### `debug.locals([nivel])`

Inspeciona variáveis locais em um nível da pilha de chamadas ("teste do palito").

**Parâmetros**:
- `nivel` (`integer`, opcional): nível na pilha (padrão: 1)

**Retorno**: `table` — `{nome = valor, ...}`

```lua
local x = 42
local info = debug.locals()
-- info.x == 42
```

---

### `debug.stack()`

Retorna o traceback da pilha de chamadas ("camadas do pudim").

**Retorno**: `string`

```lua
print(debug.stack())
```

---

### `debug.globals()`

Lista todas as variáveis globais com seus tipos.

**Retorno**: `table` — `{nome = tipo_string, ...}`

```lua
local g = debug.globals()
print(g.print) -- "function"
print(g.sys)   -- "table"
```

---

### `debug.history()`

Retorna o histórico de comandos do terminal.

**Retorno**: `table` (array de strings)

```lua
for i, cmd in ipairs(debug.history()) do
    print(i, cmd)
end
```

---

## Profiling (Forno)

### `debug.timer_start()`

Inicia o cronômetro do profiler ("pré-aquecer o forno").

```lua
debug.timer_start()
```

---

### `debug.timer_stop()`

Para o cronômetro e retorna o tempo decorrido.

**Retorno**: `integer` (milissegundos)

```lua
debug.timer_start()
-- código a medir
local ms = debug.timer_stop()
print("demorou " .. ms .. " ms")
```

---

### `debug.timer_mark([label])`

Registra um checkpoint no timeline do profiler.

**Parâmetros**:
- `label` (`string`, opcional): rótulo (padrão: `"mark"`)

**Retorno**: `boolean`

```lua
debug.timer_start()
debug.timer_mark("inicio")
-- fase 1
debug.timer_mark("fase1")
-- fase 2
debug.timer_mark("fase2")
```

---

### `debug.timer_reset()`

Reseta todo o estado do profiler (cronômetro e checkpoints).

---

### `debug.timer_report()`

Retorna todos os checkpoints registrados.

**Retorno**: `table` — array de `{label=string, ms=integer}`

```lua
local report = debug.timer_report()
for _, r in ipairs(report) do
    print(r.label .. ": " .. r.ms .. " ms")
end
```

---

## Diagnóstico de Memória

### `debug.heap()`

Retorna estatísticas do heap do kernel.

**Retorno**: `table`

```lua
local h = debug.heap()
-- h.blocks       (integer) — blocos alocados
-- h.free_blocks  (integer) — blocos livres
-- h.largest_free (integer) — maior bloco livre (bytes)
-- h.free_bytes   (integer) — total de bytes livres
-- h.total_bytes  (integer) — total de bytes no heap
```

---

### `debug.lua_mem()`

Retorna o uso de memória da VM Lua.

**Retorno**: `table`

```lua
local m = debug.lua_mem()
-- m.kb              (integer) — kilobytes usados
-- m.bytes_remainder (integer) — bytes restantes
```

---

### `debug.fragmentation()`

Retorna a taxa de fragmentação do heap (0.0 = sem fragmentação, 1.0 = totalmente fragmentado).

**Retorno**: `number` (0.0 a 1.0)

```lua
local frag = debug.fragmentation()
print("fragmentação: " .. (frag * 100) .. "%")
```
