# Developer Tooling & Standards

This directory contains development guidelines, code generation templates, and automation hooks for Pudim-Luarix.

## Structure

```
.github/
├── hooks/              # Git hooks (pre-commit, etc.)
├── instructions/       # Code standards & conventions
├── prompts/           # Reusable AI prompts for code generation
└── scripts/           # Setup and utility scripts
```

## Quick Start

### 1. Install Development Hooks

```bash
bash .github/scripts/setup-hooks.sh
```

This installs:
- **Pre-commit hook**: Validates ASCII filenames, docs location, and memory safety

### 2. Generate C→Lua Bindings

When creating new kernel bindings:

```bash
# Use the binding generator prompt
cat .github/prompts/c-to-lua-bindings.prompt.md

# Follow the pattern provided to ensure:
# - Consistent naming (k<module>lua_<function>)
# - Proper argument validation
# - Buffer overflow protection
# - Return value consistency
```

### 3. Code Review Checklist

All `include/` changes must pass:

```bash
cat .github/instructions/buffer-overflow-safety.instructions.md
```

**Key requirements:**
- ✓ Every buffer has `#define <NAME>_MAX_SIZE`
- ✓ String operations check lengths before use
- ✓ Lua bindings validate `lua_gettop(L)` and types
- ✓ Arithmetic checks for overflow
- ✓ Identifiers are ASCII-only `[a-zA-Z0-9_]`

Note: driver implementations now live in `drives/` (for example `drives/serial.c`, `drives/vga.c`, `drives/keyboard.c`).
Follow the same safety checklist for `drives/` code, even when hook rules are focused on `include/`.

### 4. Documentation Rules

```bash
cat .github/instructions/docs-location.instructions.md
```

**Summary:**
- `.md` files go in `docs/` or `.github/`
- Root exceptions currently used by hooks: `README.md`, `ATTRIBUTIONS.md`
- `lua/README.md` is also allowed
- Update internal links when moving docs

---

## Available Resources

### Prompts (Code Generation)

| Prompt | Purpose |
|--------|---------|
| [c-to-lua-bindings.prompt.md](prompts/c-to-lua-bindings.prompt.md) | Generate C→Lua bindings following kernel conventions |

### Instructions (Code Standards)

| Standard | Applies To | Purpose |
|----------|-----------|---------|
| [buffer-overflow-safety.instructions.md](instructions/buffer-overflow-safety.instructions.md) | `include/**/*.{c,h}` | Memory safety, overflow prevention, Lua binding patterns |
| [lua-first-kernel.instructions.md](instructions/lua-first-kernel.instructions.md) | `kernel.c`, `include/`, `drives/`, `lua/`, `ROADMAP.md` | Lua-first kernel design principles |
| [docs-location.instructions.md](instructions/docs-location.instructions.md) | `**/*.md` | Documentation organization rules |

### Scripts

| Script | Purpose |
|--------|---------|
| [setup-hooks.sh](scripts/setup-hooks.sh) | Install git hooks and initialize dev environment |

### Hooks

| Hook | Validation | Action |
|------|-----------|--------|
| **pre-commit** | ASCII filenames, doc locations, code safety | Prevents commit if validation fails |

---

## Example: Adding a New Binding

### Step 1: Plan the binding

You have a C function:
```c
int kfs_size(const char* filename);
```

### Step 2: Generate binding code

Reference [c-to-lua-bindings.prompt.md](prompts/c-to-lua-bindings.prompt.md):

```c
/* Lua: fs.size(filename) */
static int kfslua_size(lua_State* L) {
    const char* filename;
    int size;
    
    if (lua_gettop(L) != 1) {
        lua_pushinteger(L, -1);
        return 1;
    }
    
    if (!lua_isstring(L, 1)) {
        lua_pushinteger(L, -1);
        return 1;
    }
    
    filename = lua_tostring(L, 1);
    size = kfs_size(filename);
    lua_pushinteger(L, (lua_Integer)size);
    return 1;
}
```

### Step 3: Register in kernel

In `klua.c`:
```c
kfslua_register(L);  // Register fs table
```

### Step 4: Validate with pre-commit

```bash
git add include/kfslua.c include/kfslua.h
git commit -m "Add fs.size() binding"
# Pre-commit hook validates automatically
```

---

## Memory Safety Checklist

For any `include/**/*.c` or `include/**/*.h` changes:

- [ ] Every buffer has `#define <NAME>_MAX_SIZE`
- [ ] String lengths checked: `strlen(str) < MAX_SIZE`
- [ ] Lua args validated: `lua_gettop(L) == N`
- [ ] Type checks before `lua_to*()`: `lua_isstring(L, 1)`
- [ ] Arithmetic overflow checked: `if (a > MAX - b) return 0`
- [ ] Array access validated: `idx < MAX_SIZE`
- [ ] Identifiers are ASCII: `[a-zA-Z0-9_]` only
- [ ] No unsafe functions: no `strcpy`, `sprintf`, `gets`

---

## Pre-commit Hook Details

The hook validates:

### 1. **ASCII-Only Filenames**
```
✓ valid: kfslua.c, kterm_buffer.h, process-manager.c
✗ invalid: kfslua_α.c, proc_менеджер.c, 进程.c
```

### 2. **Documentation Location**
```
✓ valid: docs/ROADMAP.md, .github/instructions/safety.md, README.md, ATTRIBUTIONS.md, lua/README.md
✗ invalid: ROADMAP.md (root), doc/api.md (wrong folder)
```

### 3. **Non-ASCII in C Code**
Detects and rejects:
```c
✗ char ваша_переменная;  // Cyrillic
✗ int α_value;            // Greek
✗ void 函数();            // CJK
```

### 4. **Unsafe Functions in include/**
Warns about:
```c
strcpy()   // Use: memcpy() + bounds check
sprintf()  // Use: lua_pushstring() or manual formatting
gets()     // Never use this function
```

---

## Testing the Hook

```bash
# Test that hook passes for valid code
git add .github/
git commit -m "Initial tooling setup"

# Simulate failure (create file with invalid name)
touch avaрiable_файл.c
git add avaрiable_файл.c
git commit -m "Test"  # Hook will REJECT this

# Clean up
git reset HEAD
rm avaрiable_файл.c
```

---

## Integration with CI/CD

The pre-commit hook can be extended for CI:

```bash
# In GitHub Actions or similar:
bash .github/scripts/setup-hooks.sh
.git/hooks/pre-commit  # Run validation
```

---

## Related Documentation

- [SECURITY.md](/SECURITY.md) - Full security audit
- [ROADMAP.md](/ROADMAP.md) - Kernel architecture & Lua APIs
- [docs/C_LIBS_ARCHITECTURE.md](/docs/C_LIBS_ARCHITECTURE.md) - Library design
