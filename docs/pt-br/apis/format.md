# API format

Tabela global: `format`

API para serializacao PLFS e inicializacao do disco persistente.

## Funcoes

| Funcao | Assinatura | Retorno |
|---|---|---|
| `encode` | `format.encode(table)` | `string` no formato PLFS |
| `decode` | `format.decode(string)` | `table` |
| `is_valid` | `format.is_valid(string)` | `boolean` |
| `disk_init` | `format.disk_init()` | `boolean` |
| `disk_info` | `format.disk_info()` | `table` |

## Constantes

- `format.HEADER` = `"PLF:1\n"`
- `format.VERSION` = `1`

## Estrutura de disk_info

- `magic` (esperado: `PLFS`)
- `version` (esperado: `1`)
- `files` (contagem no KFS)
- `available` (persistencia disponivel)

## Exemplo

```lua
local s = format.encode({ nome = "pudim", fatias = 8 })
if format.is_valid(s) then
    local t = format.decode(s)
    print(t.nome, t.fatias)
end
```
