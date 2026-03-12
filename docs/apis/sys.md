# API sys — Informações do Sistema

Módulo Lua para consultar informações sobre o hardware e estado do kernel.

> Implementação C: `include/APISLua/ksyslua.c`

## Funções

### `sys.version()`

Retorna a string de versão do kernel.

```lua
print(sys.version())
-- "Pudim-Luarix x86_64 v0.1.0"
```

**Retorno**: `string`

---

### `sys.uptime_ms()`

Retorna o tempo desde o boot em milissegundos.

```lua
print(sys.uptime_ms() .. " ms")
```

**Retorno**: `integer`

---

### `sys.uptime_us()`

Retorna o tempo desde o boot em microssegundos.

```lua
print(sys.uptime_us() .. " us")
```

**Retorno**: `integer`

---

### `sys.memory_total()`

Retorna a RAM total disponível em bytes (detectada via E820 no x86 ou valor padrão no aarch64).

**Retorno**: `integer`

---

### `sys.memory_free()`

Retorna a memória livre do heap em bytes.

**Retorno**: `integer`

---

### `sys.memory_used()`

Retorna a memória em uso do heap em bytes.

**Retorno**: `integer`

---

### `sys.heap_total()`

Retorna o tamanho total do heap em bytes.

**Retorno**: `integer`

---

### `sys.boot_media_total()`

Retorna o tamanho total da mídia de boot em bytes.

**Retorno**: `integer`

---

### `sys.rom_total()`

Retorna o tamanho total do ROM em bytes.

**Retorno**: `integer`

---

### `sys.kernel_image_size()`

Retorna o tamanho da imagem do kernel em bytes.

**Retorno**: `integer`

---

### `sys.cpu_count()`

Retorna o número de threads lógicas da CPU.

**Retorno**: `integer`

---

### `sys.cpu_physical_cores()`

Retorna o número de cores físicos da CPU.

**Retorno**: `integer`

---

### `sys.cpu_vendor()`

Retorna a string do fabricante da CPU.

```lua
print(sys.cpu_vendor())
-- "GenuineIntel" ou "AuthenticAMD" (x86)
-- "ARM" ou "Qualcomm" (aarch64)
```

**Retorno**: `string`

---

### `sys.cpu_brand()`

Retorna a string do modelo da CPU.

```lua
print(sys.cpu_brand())
-- "Intel(R) Core(TM) i7-..." (x86)
-- "Cortex-A53" (aarch64)
```

**Retorno**: `string`

---

### `sys.memory_map_count()`

Retorna o número de entradas no mapa de memória E820.

**Retorno**: `integer` (0 no aarch64)

---

### `sys.memory_map_entry(index)`

Retorna uma entrada do mapa de memória.

**Parâmetros**:
- `index` (`integer`): índice 1-based

**Retorno**: `table` ou `nil`

```lua
local e = sys.memory_map_entry(1)
-- e.base       (integer)
-- e.length     (integer)
-- e.type       (integer: 1-5)
-- e.kind       (string: "usable", "reserved", "acpi_reclaim", "acpi_nvs", "bad", "unknown")
-- e.base_hex   (string: "0x...")
-- e.length_hex (string: "0x...")
```
