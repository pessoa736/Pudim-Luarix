# API event

Tabela global: `event`

## Funcoes

| Funcao | Assinatura | Retorno |
|---|---|---|
| `subscribe` | `event.subscribe(name, callback)` | `integer` id de inscricao |
| `unsubscribe` | `event.unsubscribe(id)` | `boolean` |
| `emit` | `event.emit(name[, data])` | `boolean` |
| `pending` | `event.pending()` | `integer` |
| `poll` | `event.poll()` | `name, data` ou `nil` |
| `clear` | `event.clear()` | sem retorno |
| `subscribers` | `event.subscribers([name])` | `integer` |
| `timer_interval` | `event.timer_interval([ticks])` | `integer` (intervalo atual) |

## Tipos aceitos em emit

- `string`
- `integer`
- `number` (convertido para inteiro)
- `boolean`
- `nil` (evento sem payload)

## Notas

- Callbacks sao referenciados no registro da VM Lua.
- O dispatch de callbacks ocorre no loop do terminal, em lotes limitados por chamada.

## Exemplo

```lua
local id = event.subscribe("timer", function(v)
    print("tick", v)
end)

event.emit("timer", 1)
print(event.pending())

local name, data = event.poll()
if name then
    print(name, data)
end

event.unsubscribe(id)
```
