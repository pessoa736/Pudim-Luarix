# API process — Processos e Escalonador

Módulo Lua para criação e gerenciamento de processos (coroutines com isolamento).

> Implementação C: `include/APISLua/kprocesslua.c`

## Funções

### `process.create(script)`

Cria um novo processo a partir de código Lua ou uma função.

**Parâmetros**:
- `script` (`string` ou `function`): código Lua para executar

**Retorno**: `integer` (PID do novo processo, 0 se falhou)

```lua
local pid = process.create("for i=1,10 do print(i) end")
print("criado pid " .. pid)

-- Ou com função:
local pid2 = process.create(function()
    print("processo filho")
end)
```

---

### `process.list()`

Lista todos os processos ativos.

**Retorno**: `table` — array de `{pid=integer, state=string}`

```lua
for _, p in ipairs(process.list()) do
    print("pid=" .. p.pid .. " state=" .. p.state)
end
```

Estados: `"ready"`, `"running"`, `"error"`, `"dead"`

---

### `process.kill(pid)`

Mata um processo pelo PID.

**Parâmetros**:
- `pid` (`integer`)

**Retorno**: `boolean`

---

### `process.yield()`

Cede o controle ao escalonador (usado dentro de processos).

**Retorno**: `boolean` (false se não está em um processo)

---

### `process.get_id()`

Retorna o PID do processo atual.

**Retorno**: `integer` (0 para o terminal)

---

### `process.tick()`

Executa um tick do escalonador — dá uma fatia de tempo ao próximo processo pronto.

**Retorno**: `boolean` (true se um processo foi executado)

---

### `process.count()`

Retorna o número de processos ativos.

**Retorno**: `integer`

---

### `process.max()`

Retorna o número máximo de slots de processos.

**Retorno**: `integer`

---

### `process.getuid()`

Retorna o UID do processo atual.

**Retorno**: `integer` (0 = root no terminal)

---

### `process.setuid(uid)`

Define o UID do processo atual.

**Parâmetros**:
- `uid` (`integer`)

**Retorno**: `boolean`

Requer ser root (UID 0) ou ter capability `CAP_SETUID`.

---

### `process.getgid()`

Retorna o GID do processo atual.

**Retorno**: `integer`

---

### `process.setgid(gid)`

Define o GID do processo atual.

**Parâmetros**:
- `gid` (`integer`)

**Retorno**: `boolean`

Requer ser root (UID 0) ou ter capability `CAP_SETUID`.

---

### `process.get_capabilities()`

Retorna as capabilities do UID atual.

**Retorno**: `table`

```lua
local caps = process.get_capabilities()
-- caps.setuid   (boolean)
-- caps.chmod    (boolean)
-- caps.chown    (boolean)
-- caps.kill     (boolean)
-- caps.fs_admin (boolean)
-- caps.process_admin (boolean)
```
