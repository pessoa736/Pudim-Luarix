---
description: "Enforce buffer overflow and memory safety standards for kernel C code in include/ and drives/. Validates buffer sizes, string lengths, argument counts, and bounds checks."
name: "Buffer Overflow & Memory Safety"
applyTo: "include/**/*.{c,h},drives/**/*.{c,h}"
---

# Buffer Overflow & Memory Safety Standards

All code in `include/` and `drives/` must implement explicit bounds checking and overflow prevention. This instruction enforces kernel-level memory safety for freestanding implementations.

## 1. Buffer Size Constants

**Define explicit limits for every buffer:**

```c
/* ✓ CORRECT */
#define KTERM_MAX_BUFFER   256
#define KHEAP_MAX_BLOCKS  1024
#define KFSLUA_MAX_FILENAME 256

typedef struct {
    char buffer[KTERM_MAX_BUFFER];
    unsigned int index;
} TermBuffer;
```

**NEVER use implicit/magic numbers:**

```c
/* ✗ WRONG */
char buf[256];  /* What if we need 512 elsewhere? */
memcpy(buf, data, len);  /* len could overflow buf */
```

## 2. String Length Validation

**Before any string operation, check length:**

```c
/* ✓ CORRECT - kfslua.c pattern */
static const char* kfslua_check_string_arg(lua_State* L, int idx) {
    if (!lua_isstring(L, idx)) {
        return NULL;
    }
    return lua_tostring(L, idx);
}

static int kfslua_write(lua_State* L) {
    const char* name = kfslua_check_string_arg(L, 1);
    
    if (!name || strlen(name) >= KFSLUA_MAX_FILENAME) {
        lua_pushboolean(L, 0);
        return 1;
    }
    
    /* Safe to use 'name' now */
    return kfs_write(name, content);
}

/* ✗ WRONG */
strcpy(buf, user_input);  /* No length check! Buffer overflow */
sprintf(buf, "%s", data);  /* sprintf is inherently unsafe */
```

## 3. Argument Count Validation (Lua Bindings)

**Check `lua_gettop(L)` for exact argument count:**

```c
/* ✓ CORRECT - ksyslua.c pattern */
static int ksyslua_uptime_ms(lua_State* L) {
    /* Implicit: no args needed */
    uint64_t uptime = ksys_uptime_ms();
    lua_pushinteger(L, (lua_Integer)uptime);
    return 1;
}

static int kfslua_write(lua_State* L) {
    /* 2 args: filename, content */
    if (lua_gettop(L) != 2) {
        lua_pushboolean(L, 0);
        return 1;
    }
    /* ... rest of function ... */
}

/* ✗ WRONG */
const char* arg = lua_tostring(L, 1);  /* No gettop check */
```

## 4. Integer Overflow Prevention

**Check arithmetic before execution:**

```c
/* ✓ CORRECT - kfs.c pattern (from security audit) */
int kfs_append(const char* name, const char* content) {
    unsigned int old_size = kfs_size(name);
    unsigned int add_size = strlen(content);
    
    /* CRITICAL: Check overflow before += */
    if (old_size > KFS_MAX_FILE_SIZE - add_size) {
        return 0;  /* Would overflow, reject */
    }
    
    unsigned int new_size = old_size + add_size;
    /* ... safe to write new_size ... */
}

/* ✗ WRONG */
size_t total = old_size + add_size;  /* Could wrap around on 32-bit */
if (total > MAX_SIZE) { ... }  /* Check after overflow happened! */
```

## 5. Pointer Bounds Validation

**For array/buffer access, always validate index:**

```c
/* ✓ CORRECT */
#define KHEAP_MAX_BLOCKS 1024

typedef struct {
    void* blocks[KHEAP_MAX_BLOCKS];
    unsigned int count;
} Heap;

void* kheap_get_block(Heap* heap, unsigned int index) {
    if (index >= KHEAP_MAX_BLOCKS) {
        return NULL;  /* Out of bounds */
    }
    return heap->blocks[index];
}

/* ✗ WRONG */
return heap->blocks[arbitrary_index];  /* No bounds check */
```

## 6. File Size & Content Validation

**Enforce limits on external input:**

```c
/* ✓ CORRECT - klua_cmds.c pattern */
#define KLUA_CMD_MAX_SCRIPT 512

int klua_cmd_run(const char* name) {
    const char* script = klua_cmd_get_script(name);
    
    if (!script || strlen(script) > KLUA_CMD_MAX_SCRIPT) {
        return 0;  /* Script too large */
    }
    
    return klua_run(script);
}

/* ✗ WRONG */
return klua_run(script);  /* No size check on script */
```

## 7. Buffer Initialization & Zero-Termination

**Always initialize buffers and ensure null-termination:**

```c
/* ✓ CORRECT */
char buffer[256] = {0};  /* Zero-initialized */
if (strlen(input) >= sizeof(buffer) - 1) return 0;
strcpy(buffer, input);
buffer[255] = '\0';  /* Explicit terminator */

/* ✗ WRONG */
char buffer[256];  /* Uninitialized garbage */
strcpy(buffer, input);  /* strlen(input) unknown */
```

## 8. ASCII-Only Validation (Kernel Identifiers)

**Reject non-Latin characters in filenames/identifiers:**

```c
/* ✓ CORRECT - klua_cmds.c pattern */
static int klua_cmd_is_valid_name(const char* name) {
    unsigned int i = 0;
    
    if (!name || !name[0]) return 0;
    
    while (name[i]) {
        char c = name[i];
        /* Whitelist [a-zA-Z0-9_] only */
        if (!((c >= 'a' && c <= 'z') || 
              (c >= 'A' && c <= 'Z') || 
              (c >= '0' && c <= '9') || 
              c == '_')) {
            return 0;  /* Reject non-ASCII/symbols */
        }
        i++;
        
        if (i > KLUA_CMD_MAX_NAME) {
            return 0;  /* Name too long */
        }
    }
    
    return i > 0 && i < KLUA_CMD_MAX_NAME;
}

/* ✗ WRONG */
if (name[0] >= 'a' && name[0] <= 'z') { ... }  /* No full validation */
strcpy(filename, user_name);  /* Non-ASCII could cause symbol issues */
```

## 9. Type Safety in Lua Bindings

**Always type-check before lua_to* calls:**

```c
/* ✓ CORRECT - kfslua.c pattern */
static int kfslua_append(lua_State* L) {
    const char* name;
    const char* content;
    
    name = kfslua_check_string_arg(L, 1);
    content = kfslua_check_string_arg(L, 2);
    
    if (!name || !content) {
        lua_pushboolean(L, 0);
        return 1;
    }
    
    /* Safe: name and content are non-NULL, validated strings */
    return kfs_append(name, content);
}

/* ✗ WRONG */
const char* name = lua_tostring(L, 1);  /* Could be NULL if not string */
kfs_write(name, content);  /* Might crash on NULL pointer */
```

## 10. Stack Overflow Prevention (Recursion)

**Kernel code must be non-recursive or depth-limited:**

```c
/* ✓ CORRECT */
int kfs_read_depth = 0;
#define KFS_MAX_DEPTH 16

int kfs_read_recursive(const char* path) {
    if (kfs_read_depth >= KFS_MAX_DEPTH) {
        return -1;  /* Depth limit exceeded */
    }
    
    kfs_read_depth++;
    int result = /* ... recursive logic ... */;
    kfs_read_depth--;
    
    return result;
}

/* ✗ WRONG - Unbounded recursion */
int kfs_read(const char* path) {
    return kfs_read_internal(path);  /* No depth tracking */
}
```

## Checklist for Code Review

When submitting code in `include/`:

- [ ] **Every buffer** has a `#define <NAME>_MAX_SIZE` constant
- [ ] **All string operations** check `strlen() < MAX_SIZE` first
- [ ] **Lua bindings** validate `lua_gettop(L)` and argument types
- [ ] **Arithmetic operations** check for overflow before execution
- [ ] **Array accesses** validate index `< ARRAY_MAX_SIZE`
- [ ] **External input** (files, Lua args) has size limits enforced
- [ ] **Buffers** are zero-initialized and null-terminated
- [ ] **Identifiers/filenames** use ASCII-only `[a-zA-Z0-9_]`
- [ ] **Type checks** happen before `lua_to*()` calls
- [ ] **Recursion** is depth-limited with counters
- [ ] **No use of unsafe functions**: `strcpy`, `sprintf`, `gets`
- [ ] **Documentation** explains buffer limits in comments

## Related Files

- [docs/SECURITY.md](/home/davi/Documents/GitHub/Pudim-Luarix/docs/SECURITY.md) - Full security audit
- [.github/prompts/c-to-lua-bindings.prompt.md](.github/prompts/c-to-lua-bindings.prompt.md) - Binding generator template
- Example implementations:
  - [include/kfslua.c](include/kfslua.c) - String validation pattern
  - [include/klua_cmds.c](include/klua_cmds.c) - Name validation + size limits
  - [include/kfs.c](include/kfs.c) - Overflow prevention

## Freestanding Constraint

Kernel code has **no libc** (freestanding). This means:

- `strlen()` works (provided in libc_shim.c)
- `strcpy()` prohibited (no bounds checking)
- `memcpy(dst, src, size)` requires explicit size validation
- **You must validate manually** - no compiler warnings for buffer overflows
