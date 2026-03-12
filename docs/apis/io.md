# API io — Saída de Texto

Módulo Lua para escrita no display (VGA ou serial, conforme detecção automática).

> Implementação C: `include/APISLua/klua.c`

## Funções

### `io.write(...)`

Escreve valores no display sem quebra de linha.

**Parâmetros**: variádicos (strings)

```lua
io.write("temperatura: ")
io.write("180°C")
-- saída: "temperatura: 180°C"
```

---

### `io.writeln(...)`

Escreve valores no display com quebra de linha ao final.

**Parâmetros**: variádicos (strings)

```lua
io.writeln("pudim pronto!")
```

---

### `io.clear()`

Limpa o display.

```lua
io.clear()
```
