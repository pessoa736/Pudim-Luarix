---
description: "Use when designing or modifying kernel architecture, APIs, roadmap items, terminal commands, Lua bindings, or system interfaces. Enforces Lua-first kernel conventions for Pudim-Luarix."
name: "Lua-First Kernel Conventions"
applyTo: "kernel.c,include/**,drives/**,lua/**,ROADMAP.md"
---
# Lua-First Kernel Conventions

- Treat Lua as the primary system interface.
- Prefer exposing kernel capabilities as Lua tables/functions instead of syscall-style interfaces.
- Keep performance-critical logic in C, but expose controlled bindings to Lua.
- Preserve the project direction: kernel services support Lua APIs (process, memory, fs, sync, sys).
- For roadmap/proposals, prioritize features that improve Lua API capabilities and process isolation.
- Keep naming and code patterns ASCII-only in identifiers and filenames.
- For boot and initialization diagnostics, use the `kbootlog_*` API instead of ad-hoc `vga_print`/`serial_print` pairs.
- Keep boot log titles color-coded in VGA (blue title, readable body) while mirroring the same message to serial.
