# C Libraries Architecture - Performance Bindings

## Overview

Pudim-Luarix usa **C libraries com bindings Lua** para operações críticas onde performance é essencial. Lua é interpretado (lento), então code hot-path vai em C.

**Filosofia**: 
- Sistema é exposto como Lua APIs (tabelas/funções)
- Código-crítico é implementado em C
- Lua é orquestração + scripting

---

## Pattern: Como Criar uma C Library com Bindings Lua

### 1. Estrutura de Arquivos

```
include/
├── klib.h          ← Interface pública (declarations)
├── klib.c          ← Implementação internals (não exposto)
└── klibLua.c/h     ← Bindings para Lua (exports)
```

**Exemplo**: `kfs` (filesystem)
```
include/
├── kfs.h           ← int kfs_write(name, content)
├── kfs.c           ← implementação de kfs_*
└── kfslua.c/h      ← Lua: kfs.write(), kfs.read(), etc
```

### 2. Template: Criar Nova Lib

#### Passo 1: Definir interface C (e.g., `include/kmath.h`)

```c
#ifndef KMATH_H
#define KMATH_H

#include <stdint.h>

/* Fast math operations */
uint32_t kmath_sqrt(uint32_t x);
uint32_t kmath_power(uint32_t base, uint32_t exp);
int32_t kmath_gcd(int32_t a, int32_t b);

#endif
```

#### Passo 2: Implementar C (e.g., `include/kmath.c`)

```c
#include "kmath.h"

uint32_t kmath_sqrt(uint32_t x) {
    if (x == 0) return 0;
    uint32_t root = x;
    uint32_t next = (root + x / root) / 2;
    while (next < root) {
        root = next;
        next = (root + x / root) / 2;
    }
    return root;
}

uint32_t kmath_power(uint32_t base, uint32_t exp) {
    uint32_t result = 1;
    while (exp--) result *= base;
    return result;
}

int32_t kmath_gcd(int32_t a, int32_t b) {
    if (b == 0) return a;
    return kmath_gcd(b, a % b);
}
```

#### Passo 3: Criar Lua Bindings (e.g., `include/kmathlua.c/h`)

**kmathlua.h:**
```c
#ifndef KMATHLUA_H
#define KMATHLUA_H

#include "../lua/src/lua.h"

int kmathlua_register(lua_State* L);

#endif
```

**kmathlua.c:**
```c
#include "kmathlua.h"
#include "kmath.h"

/* Lua: math.sqrt(x) */
static int kmathlua_sqrt(lua_State* L) {
    if (!lua_isnumber(L, 1)) {
        lua_pushstring(L, "math.sqrt: argument must be a number");
        lua_error(L);
        return 0;
    }
    
    uint32_t x = (uint32_t)lua_tointeger(L, 1);
    uint32_t result = kmath_sqrt(x);
    lua_pushinteger(L, result);
    return 1;
}

/* Lua: math.power(base, exp) */
static int kmathlua_power(lua_State* L) {
    uint32_t base = (uint32_t)lua_tointeger(L, 1);
    uint32_t exp = (uint32_t)lua_tointeger(L, 2);
    lua_pushinteger(L, kmath_power(base, exp));
    return 1;
}

/* Lua: math.gcd(a, b) */
static int kmathlua_gcd(lua_State* L) {
    int32_t a = (int32_t)lua_tointeger(L, 1);
    int32_t b = (int32_t)lua_tointeger(L, 2);
    lua_pushinteger(L, kmath_gcd(a, b));
    return 1;
}

int kmathlua_register(lua_State* L) {
    /* Create math table */
    lua_createtable(L, 0, 3);
    
    lua_pushcfunction(L, kmathlua_sqrt);
    lua_setfield(L, -2, "sqrt");
    
    lua_pushcfunction(L, kmathlua_power);
    lua_setfield(L, -2, "power");
    
    lua_pushcfunction(L, kmathlua_gcd);
    lua_setfield(L, -2, "gcd");
    
    lua_setglobal(L, "math");
    return 1;
}
```

#### Passo 4: Registrar em klua.c

Em `include/klua.c` no `klua_init()`:

```c
int klua_init(void) {
    g_lua = lua_newstate(klua_alloc, NULL, 0x50554449u);
    // ... outras coisas ...
    
    if (!kfslua_register(g_lua)) return 0;
    if (!kmathlua_register(g_lua)) return 0;  // !!! Nova lib
    if (!ksyslua_register(g_lua)) return 0;   // !!! Outra nova lib
    
    return 1;
}
```

#### Passo 5: Atualizar Makefile

Adicionar arquivo à compilação:

```makefile
KERNEL_OBJS = \
    $(BUILD)/kernel.o \
    $(BUILD)/kio.o \
    $(BUILD)/kfs.o \
    $(BUILD)/kfslua.o \
    $(BUILD)/kmath.o \      # !!! Nova
    $(BUILD)/kmathlua.o \  # !!!
    # ... resto ...
```

---

## Roadmap: C Libraries Prioritárias

### **Priority 1 (v0.1.1) - Crítico para Performance**

#### `ksys` - System Information
```lua
-- Lua API:
sys.version()          -- "Pudim-Luarix v0.1.0"
sys.uptime()           -- ms desde boot
sys.get_time()         -- clock atual (para PIT timer)
sys.memory_total()     -- RAM total
sys.memory_free()      -- RAM livre
```
**Por quê**: Scheduler + timers vs performance
**Esforço**: ~2h

---

#### `ksync` - Synchronization Primitives
```lua
-- Lua API:
lock = sync.mutex()
lock:lock()
lock:unlock()

spin = sync.spinlock()
spin:lock()
spin:unlock()
```
**Por quê**: Multi-process safety sem overhead
**Esforço**: ~3h

---

#### `kbuffer` - Efficient Buffer Handling
```lua
-- Lua API:
buf = buffer.create(size)
buffer.write(buf, data)
buffer.read(buf, size)
buffer.size(buf)
buffer.clear(buf)
```
**Por quê**: File I/O + serial I/O otimizado
**Esforço**: ~2h

---

### **Priority 2 (v0.2) - Otimizações Gerais**

#### `kmath` - Fast Math (uint32/int32)
```lua
math.sqrt(x)
math.power(base, exp)
math.gcd(a, b)
math.abs(x)
math.min(a, b)
math.max(a, b)
```
**Por quê**: Scripts Lua com muita matemática
**Esforço**: ~1.5h

---

#### `kstr` - String Operations (C-speed)
```lua
str.length(s)
str.concat(s1, s2)
str.substr(s, pos, len)
str.find(haystack, needle)
str.split(s, delim)
str.upper(s)
str.lower(s)
```
**Por quê**: String manipulation sem GC overhead
**Esforço**: ~2.5h

---

#### `khash` - Hash Functions
```lua
hash.crc32(data)
hash.djb2(data)
hash.sdbm(data)
```
**Por quê**: Checksums + tabelas hash customizadas
**Esforço**: ~1.5h

---

### **Priority 3 (v0.3) - Advanced**

#### `kcrypto` - Crypto Primitives
```lua
crypto.sha256(data)
crypto.md5(data)
```
**Por quê**: Security + signatures
**Esforço**: ~4h

---

---

## Best Practices

### 1. **Error Handling em Lua**

```c
static int kmath_sqrt(lua_State* L) {
    if (lua_gettop(L) != 1) {
        lua_pushstring(L, "math.sqrt: expected 1 argument");
        lua_error(L);  // Throw para Lua
        return 0;
    }
    
    if (!lua_isnumber(L, 1)) {
        lua_pushstring(L, "math.sqrt: argument must be a number");
        lua_error(L);
        return 0;
    }
    
    lua_pushinteger(L, result);
    return 1;  // 1 return value
}
```

### 2. **Memory Management**

- **Allocation**: Use `kmalloc()` (kernel heap)
- **Freeing**: Sempre free se você allocou; Lua GC só coleta Lua objects
- **Safety**: Check para NULL após kmalloc

```c
void* buf = kmalloc(size);
if (!buf) {
    lua_pushstring(L, "kmalloc failed");
    lua_error(L);
    return 0;
}
// ... use buf ...
kfree(buf);
```

### 3. **Performance Tips**

- Minimize lua_*() calls em loop
- Use `lua_raw*` (sem metamethods) se possível
- Cache `lua_State*` globalmente (não passa em stack)
- Prefira inteiros a floats quando possível

---

## Testing Strategy

1. **Unit tests em Lua**:
```lua
-- test_math.lua
assert(math.sqrt(16) == 4, "sqrt(16) failed")
assert(math.gcd(12, 8) == 4, "gcd failed")
print("math tests passed")
```

2. **Integração com kterm**:
```bash
# Em qemu serial terminal:
kterm> lrun test_math
kterm> math.sqrt(100)
10
```

3. **Benchmark**:
```lua
-- time_math.lua
local start = sys.uptime()
for i = 1, 1000000 do
    math.sqrt(i)
end
local elapsed = sys.uptime() - start
print("1M sqrt in " .. elapsed .. "ms")
```

---

## Próximos Passos

1. **Implementar `ksys`** (system info → necessário para scheduler)
2. **Implementar `ksync`** (mutexes → process isolation)
3. **Implementar `kbuffer`** (I/O otimizado → terminal responsivo)
4. Continuar com `kmath`, `kstr`, `khash` conforme necessário

**Estimativa total (Priority 1)**: ~7h de dev  
**Impacto**: Kernel 2-3x mais rápido em operações críticas
