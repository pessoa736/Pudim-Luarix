# API serial — Porta Serial

Módulo Lua para escrita direta na porta serial (COM1 no x86, PL011 UART no aarch64).

> Implementação C: `include/APISLua/klua.c`

## Funções

### `serial.write(...)`

Escreve strings na porta serial.

**Parâmetros**: variádicos (strings)

```lua
serial.write("debug: valor = 42")
```

---

### `serial.writeln(...)`

Escreve strings na porta serial seguidas de `\r\n`.

**Parâmetros**: variádicos (strings)

```lua
serial.writeln("log de boot iniciado")
```

## Uso principal

Útil para depuração quando conectado via terminal serial (Termux, minicom, picocom). No modo serial-only (aarch64 ou x86 sem monitor), `io.write` já envia para o serial automaticamente.
