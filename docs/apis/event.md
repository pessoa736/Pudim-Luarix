# API event — Sistema de Eventos

Módulo Lua para publicação e assinatura de eventos assíncronos.

> Implementação C: `include/APISLua/keventlua.c`

## Funções

### `event.subscribe(nome, callback)`

Registra uma função callback para um evento nomeado.

**Parâmetros**:
- `nome` (`string`): nome do evento
- `callback` (`function`): chamada quando o evento for disparado

**Retorno**: `integer` (ID da assinatura, 0 se falhou)

```lua
local id = event.subscribe("timer", function(data)
    print("timer disparou: " .. tostring(data))
end)
```

---

### `event.unsubscribe(id)`

Remove uma assinatura de evento.

**Parâmetros**:
- `id` (`integer`): ID retornado por `subscribe`

**Retorno**: `boolean`

---

### `event.emit(nome [, dados])`

Emite um evento com dados opcionais.

**Parâmetros**:
- `nome` (`string`): nome do evento
- `dados` (opcional): `string`, `integer`, `number`, ou `boolean`

**Retorno**: `boolean`

```lua
event.emit("meu_evento", 42)
event.emit("alerta", "pudim queimou!")
event.emit("flag", true)
```

---

### `event.pending()`

Retorna o número de eventos pendentes na fila.

**Retorno**: `integer`

---

### `event.poll()`

Consome e retorna o próximo evento da fila.

**Retorno**: `string, valor` ou `nil` (se fila vazia)

```lua
local nome, dados = event.poll()
if nome then
    print("evento: " .. nome)
end
```

---

### `event.clear()`

Limpa todos os eventos pendentes e libera referências Lua dos callbacks.

---

### `event.subscribers([nome])`

Retorna o número de assinantes.

**Parâmetros**:
- `nome` (`string`, opcional): filtrar por nome do evento

**Retorno**: `integer`

```lua
print(event.subscribers())        -- total
print(event.subscribers("timer")) -- apenas do evento "timer"
```

---

### `event.timer_interval([ticks])`

Consulta ou define o intervalo do evento de timer em ticks.

**Parâmetros**:
- `ticks` (`integer`, opcional): novo intervalo (1 a 100000)

**Retorno**: `integer` (intervalo atual)

```lua
event.timer_interval(1000) -- disparar a cada 1000 ticks
print(event.timer_interval()) -- consultar atual
```
