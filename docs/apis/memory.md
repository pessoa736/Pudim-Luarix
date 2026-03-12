# API memory — Gerenciamento de Memória

Módulo Lua para alocação e proteção de memória do kernel.

> Implementação C: `include/APISLua/kmemorylua.c`

## Funções

### `memory.allocate(size)`

Aloca um bloco de memória alinhado a 4KB.

**Parâmetros**:
- `size` (`integer`): tamanho em bytes

**Retorno**: `integer` (endereço do bloco, 0 se falhou)

```lua
local ptr = memory.allocate(4096)
if ptr ~= 0 then
    print("alocado em " .. ptr)
end
```

---

### `memory.free(ptr)`

Libera um bloco de memória previamente alocado.

**Parâmetros**:
- `ptr` (`integer`): endereço retornado por `memory.allocate`

**Retorno**: `boolean` (true se liberou com sucesso)

```lua
memory.free(ptr)
```

---

### `memory.protect(ptr, flags)`

Define flags de proteção em um bloco de memória.

**Parâmetros**:
- `ptr` (`integer`): endereço do bloco
- `flags` (`integer`): combinação de flags

**Flags**:
| Valor | Constante | Descrição |
|-------|-----------|-----------|
| 1 | READ | Leitura permitida |
| 2 | WRITE | Escrita permitida |
| 4 | EXEC | Execução permitida |

**Retorno**: `boolean`

```lua
memory.protect(ptr, 1 + 2) -- leitura + escrita
```

---

### `memory.dump_stats()`

Retorna uma string com estatísticas do alocador de memória.

**Retorno**: `string`

```lua
print(memory.dump_stats())
-- "allocated=4096 bytes, blocks=1, mmu_rwx=4k"
```
