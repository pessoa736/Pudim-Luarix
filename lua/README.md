# Lua Runtime - Kernel Integration

## Overview

This directory contains **Lua 5.5.0**, a lightweight, embeddable scripting language designed to serve as the primary system interface for Pudim-Luarix.

**Attribution:** Lua is developed by the Lua team at PUC-Rio (Pontifical Catholic University of Rio de Janeiro) and is distributed under the MIT License.

- **Official Website:** https://www.lua.org
- **License:** MIT (see `LICENSE_LUA` in this directory)
- **Version:** 5.5.0 (released December 15, 2025)

## Status in Pudim-Luarix

- **Policy:** Vendored dependency (stable branch)
- **Integration:** Runs in freestanding environment (no libc)
- **Memory:** Uses kernel heap allocator (kheap.c via libc_shim)
- **Build:** Integrated with main kernel makefile

## Structure

```
lua/
├── README.md                    ← This file
├── LICENSE_LUA                  ← Lua MIT License
├── src/                         ← Lua 5.5.0 source (unmodified)
│   ├── lapi.c/.h              ← C API
│   ├── lvm.c/.h               ← Virtual machine
│   ├── lobject.c/.h           ← Object system
│   ├── lmem.c/.h              ← Memory management
│   ├── lua.h                  ← Main header
│   └── ...                    ← Other core modules
├── doc/                         ← Lua documentation
│   ├── manual.html            ← Language reference
│   ├── readme.html            ← Installation guide
│   └── ...                    ← Man pages, CSS
├── commands.lua                 ← Kernel command registry
└── Makefile                     ← Build configuration
```

## Memory Model

Lua runs in a **freestanding environment**:

- **No libc:** Uses `libc_shim.c` for minimal C functions
- **No printf:** Output via kernel serial/VGA drivers
- **No filesystem:** Kernel filesystem API (kfs.c)
- **Allocator:** All `malloc`/`free` redirected to kernel `kheap_alloc()`

This allows Lua code to query/control kernel resources safely.

## C API Integration

Kernel modules expose Lua bindings via:

```c
/* Example: filesystem binding */
int kfslua_register(lua_State* L);

/* Lua code can now use: */
fs.write("file.txt", "data")
fs.read("file.txt")
```

Pattern follows [.github/prompts/c-to-lua-bindings.prompt.md](../.github/prompts/c-to-lua-bindings.prompt.md)

## Customizations

If kernel-specific patches are applied to Lua:

```
lua/patches/
├── freestanding.patch         ← Any adaptations
└── README.md                  ← Explain changes
```

Currently: Lua 5.5.0 is used unmodified; compatibility maintained via `libc_shim.c`.

## Compilation

```bash
# Build with kernel (from root)
make

# Or standalone (if patching Lua)
cd lua
make posix MYCFLAGS="-I../include -ffreestanding"
```

## License

```
Lua 5.5.0
Copyright (C) 1994-2024 Lua.org, PUC-Rio.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
```

[Full license in `LICENSE_LUA`]

## When to Update Lua

Update Lua version only for:

1. **Critical security patches**
2. **Performance improvements**
3. **Freestanding compatibility fixes**

DO NOT update for:

- New language features (maintains binary stability)
- Non-critical bugfixes
- Changes to stdlib (use kernel APIs instead)

### Rationale

A kernel's scripting language should remain stable to ensure:

- **Reproducible builds** (same Lua version always)
- **Stable ABI** for user scripts
- **Predictable behavior** across kernel versions

## Related Kernel Documentation

- [docs/ROADMAP.md](../docs/ROADMAP.md) — Lua API expansion roadmap
- [docs/SECURITY.md](../docs/SECURITY.md) — Memory safety guidelines
- [.github/prompts/c-to-lua-bindings.prompt.md](../.github/prompts/c-to-lua-bindings.prompt.md) — Binding generation template
