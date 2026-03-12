# API keyboard — Entrada de Teclado

Módulo Lua para leitura de teclas do teclado (PS/2 no x86, serial no aarch64).

> Implementação C: `include/APISLua/kkeyboardlua.c`

## Funções

### `keyboard.getkey()`

Lê uma tecla de forma não-bloqueante. Retorna caracteres ASCII ou nomes de teclas especiais.

**Retorno**: `string` ou `nil`

```lua
local key = keyboard.getkey()
if key then
    if key == "up" then
        print("seta para cima")
    else
        print("tecla: " .. key)
    end
end
```

**Teclas especiais** retornadas como string:

| Retorno | Tecla |
|---------|-------|
| `"up"` | Seta ↑ |
| `"down"` | Seta ↓ |
| `"left"` | Seta ← |
| `"right"` | Seta → |
| `"home"` | Home |
| `"end"` | End |
| `"delete"` | Delete |

---

### `keyboard.getchar()`

Lê um caractere ASCII de forma não-bloqueante (ignora teclas especiais).

**Retorno**: `string` (1 caractere) ou `nil`

```lua
local ch = keyboard.getchar()
if ch then print("char: " .. ch) end
```

---

### `keyboard.available()`

Verifica se há uma tecla disponível no buffer.

**Retorno**: `boolean`

```lua
if keyboard.available() then
    local key = keyboard.getkey()
end
```
