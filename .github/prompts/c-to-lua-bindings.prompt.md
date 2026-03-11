---
description: "Generate type-safe C→Lua bindings following Pudim-Luarix kernel conventions"
name: "C→Lua Binding Generator"
tags: ["lua", "bindings", "kernel", "code-gen"]
---

# C→Lua Binding Generator

Generate production-ready Lua bindings for kernel C functions. Ensures safety, consistency, and follows Pudim-Luarix conventions.

## Input: C Function Signature

Provide the C function prototype(s) to bind:

```c
// Example:
uint32_t kfs_size(const char* filename);
int kfs_write(const char* name, const char* content);
int kfs_append(const char* name, const char* content);
```

## Output Template

### 1. Header File (k`module`lua.h)

```c
#ifndef K<MODULE>LUA_H
#define K<MODULE>LUA_H

#include "lua.h"

int k<module>lua_register(lua_State* L);

#endif
```

### 2. Implementation (k`module`lua.c)

For each C function, generate:

```c
/* Lua: module.function_name(arg1, arg2, ...) */
static int k<module>lua_function_name(lua_State* L) {
    /* 1. Validate argument count */
    if (lua_gettop(L) != <count>) {
        lua_pushboolean(L, 0);
        return 1;
    }
    
    /* 2. Type-check and extract arguments */
    if (!lua_isstring(L, 1)) {
        lua_pushboolean(L, 0);
        return 1;
    }
    const char* arg1 = lua_tostring(L, 1);
    
    /* 3. Call C function */
    int result = k<module>_function_name(arg1);
    
    /* 4. Push result(s) */
    lua_pushboolean(L, result);
    return 1;  /* Number of return values */
}
```

### 3. Registration Function

```c
int k<module>lua_register(lua_State* L) {
    /* Create module table */
    lua_createtable(L, 0, <field_count>);
    
    lua_pushcfunction(L, k<module>lua_function_name);
    lua_setfield(L, -2, "function_name");
    
    /* ... repeat for each function ... */
    
    lua_setglobal(L, "<module>");
    return 1;
}
```

## Safety Rules (REQUIRED)

1. **ASCII-Only Identifiers**  
   - Use only `[a-zA-Z0-9_]` in function/variable names
   - Reject non-Latin characters (e.g., Cyrillic 'а' instead of 'a')

2. **Argument Validation**
   ```c
   /* Always validate count and types */
   if (lua_gettop(L) != 2) return luaL_error(L, "Expected 2 args");
   if (!lua_isstring(L, 1)) return luaL_error(L, "Arg 1 must be string");
   ```

3. **Buffer Overflow Protection**
   - Check string lengths before operations
   - Define and enforce max sizes: `#define K<MODULE>_MAX_<FIELD> <size>`
   - Example:
   ```c
   #define KFSLUA_MAX_FILENAME 256
   
   const char* filename = lua_tostring(L, 1);
   if (strlen(filename) >= KFSLUA_MAX_FILENAME) {
       lua_pushboolean(L, 0);
       return 1;
   }
   ```

4. **Integer Overflow Prevention**
   - Check before arithmetic operations
   ```c
   if (old_size > KHEAP_MAX_SIZE - add_size) {
       lua_pushboolean(L, 0);
       return 1;
   }
   ```

5. **Return Value Consistency**
   - Boolean functions: `lua_pushboolean(L, result)`
   - Integers: `lua_pushinteger(L, (lua_Integer)value)`
   - Strings: `lua_pushstring(L, string)`
   - Errors: `lua_pushnil(L)` or `luaL_error(L, "msg")`

## Hooks Integration

When adding new bindings:

1. Register in `klua.c` via `k<module>lua_register(L)` call
2. Document in `docs/ROADMAP.md` under "Lua API" section
3. Ensure filenames use ASCII only: `ksyslua.c` ✓, not `ksyslua_α.c` ✗
4. Test with `make run` → interact in serial terminal

## Example Output

For `uint32_t kfs_size(const char* filename)`:

```c
/* Lua: fs.size(filename) returns integer size or -1 on error */
static int kfslua_size(lua_State* L) {
    const char* filename;
    uint32_t size;
    
    if (lua_gettop(L) != 1) {
        lua_pushinteger(L, -1);
        return 1;
    }
    
    if (!lua_isstring(L, 1)) {
        lua_pushinteger(L, -1);
        return 1;
    }
    
    filename = lua_tostring(L, 1);
    kfslua_ensure_init();
    
    size = kfs_size(filename);
    lua_pushinteger(L, (lua_Integer)size);
    return 1;
}
```

## Checklist

- [ ] Function names follow `k<module>lua_<function>` pattern
- [ ] All arguments validated before C call
- [ ] Buffer overflow checks (max sizes defined)
- [ ] Integer overflow checks (if applicable)
- [ ] Return values pushed correctly to Lua stack
- [ ] Header guard present in `.h` file
- [ ] Documentary comment shows Lua usage signature
- [ ] No non-ASCII characters in identifiers or filenames
- [ ] Integrated into `klua.c` registration chain
