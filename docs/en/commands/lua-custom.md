# Dynamic Lua Commands (KCMDS)

Dynamic terminal commands are loaded from commands.lua (stored in KFS) and exposed through the global table KCMDS.

## How it works

- Native lookup fails, then terminal tries KCMDS.
- lcmds prints currently available dynamic command names.
- Default script is embedded in commands_lua.inc and can be replaced in KFS.

## Default commands

| Command | Usage | Purpose |
|---|---|---|
| version | version | Prints Pudim-Luarix version. |
| sysstats | sysstats | Prints a system dashboard (CPU, RAM, heap, ROM, processes, uptime). |
| memmap | memmap | Prints E820 memory map entries. |
| runq | runq | Runs up to 64 scheduler ticks and summarizes run count. |
| test | test proc | Quick process/sync smoke test. |
| test | test page [count] | Allocates 4096-byte pages, reports, and frees them. |

## Invocation rules

- Command name whitelist: [a-zA-Z0-9_]
- Max arguments per line: 8
- Max script size per command: 512 bytes
- Space-containing arguments can use single or double quotes

A good mental model: native commands are baked into the mold; KCMDS lets you pour new flavor on top.
