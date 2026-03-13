# API fs (kfs)

Tabelas globais: `fs` e `kfs` (alias da mesma API)

## Funcoes

| Funcao | Assinatura | Retorno |
|---|---|---|
| `write` | `fs.write(name, content)` | `boolean` |
| `append` | `fs.append(name, content)` | `boolean` |
| `read` | `fs.read(name)` | `string` ou `nil` |
| `delete` | `fs.delete(name)` | `boolean` |
| `exists` | `fs.exists(name)` | `boolean` |
| `size` | `fs.size(name)` | `integer` |
| `count` | `fs.count()` | `integer` |
| `list` | `fs.list()` | `table` array de nomes |
| `clear` | `fs.clear()` | `boolean` |
| `save` | `fs.save()` | `boolean` |
| `load` | `fs.load()` | `boolean` |
| `persistent` | `fs.persistent()` | `boolean` |
| `chmod` | `fs.chmod(name, mode_octal)` | `boolean` |
| `chown` | `fs.chown(name, uid, gid)` | `boolean` |
| `getperm` | `fs.getperm(name)` | `table` ou `nil` |

## Regras de permissao (resumo)

- `chmod`: permitido para root, dono do arquivo, ou quem possui capacidade `CAP_CHMOD`.
- `chown`: permitido para root ou capacidade `CAP_CHOWN`.

## Estrutura de getperm

- `mode`: inteiro octal (`0..0777`).
- `uid`: dono do arquivo.
- `gid`: grupo do arquivo.

## Exemplo

```lua
fs.write("bolo", "camada1")
fs.append("bolo", "\ncamada2")
print(fs.read("bolo"))

local p = fs.getperm("bolo")
if p then
    print("mode", p.mode, "uid", p.uid, "gid", p.gid)
end
```
