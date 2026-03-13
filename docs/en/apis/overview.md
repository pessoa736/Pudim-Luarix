# Kernel Lua APIs Overview

These modules are the kernel tasting menu exposed to Lua.

- sys: platform and runtime metrics
- fs (alias kfs): file operations, persistence, permissions
- process: process lifecycle and identity
- memory: allocation/free/protection
- event: queue, subscriptions, timer interval
- sync: spinlock and mutex primitives
- debug: stack/locals/history/profiler helpers
- keyboard: non-blocking input
- format: PLFS serialization and disk helpers
- io and serial + base functions: runtime foundation
