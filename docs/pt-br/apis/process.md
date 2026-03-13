# API process

Tabela global: `process`

## Funcoes

| Funcao | Assinatura | Retorno |
|---|---|---|
| `create` | `process.create(script_ou_func)` | `integer` pid ou `0` |
| `list` | `process.list()` | `table` com `{ pid, state }` |
| `kill` | `process.kill(pid)` | `boolean` |
| `yield` | `process.yield()` | cede execucao (corrotina) |
| `get_id` | `process.get_id()` | `integer` pid atual |
| `tick` | `process.tick()` | `boolean` (rodou algo no tick) |
| `count` | `process.count()` | `integer` |
| `max` | `process.max()` | `integer` |
| `getuid` | `process.getuid()` | `integer` |
| `setuid` | `process.setuid(uid)` | `boolean` |
| `getgid` | `process.getgid()` | `integer` |
| `setgid` | `process.setgid(gid)` | `boolean` |
| `get_capabilities` | `process.get_capabilities()` | `table` de capacidades |

## Estados na listagem

- `ready`
- `running`
- `error`
- `dead`

## Estrutura de get_capabilities

Campos booleanos:

- `setuid`
- `chmod`
- `chown`
- `kill`
- `fs_admin`
- `process_admin`

## Notas

- O terminal interativo usa `pid = 0` (contexto root).
- `setuid` e `setgid` exigem validacao de capacidade.

## Exemplo

```lua
local pid = process.create("print('forno ligado')")
print("pid", pid)
print("ativos", process.count(), "/", process.max())
```
