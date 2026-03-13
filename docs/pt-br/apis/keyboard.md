# API keyboard

Tabela global: `keyboard`

## Funcoes

| Funcao | Assinatura | Retorno |
|---|---|---|
| `getkey` | `keyboard.getkey()` | `string` (tecla), nome especial (`"up"`, `"left"`, etc.) ou `nil` |
| `getchar` | `keyboard.getchar()` | `string` de 1 caractere ou `nil` |
| `available` | `keyboard.available()` | `boolean` |

## Notas

- Leitura e nao bloqueante.
- Teclas especiais mapeadas incluem: `up`, `down`, `left`, `right`, `home`, `end`, `delete`.

## Exemplo

```lua
if keyboard.available() then
    local k = keyboard.getkey()
    if k then
        print("tecla:", k)
    end
end
```
