# API format — Serialização PLF (Pudim Lua Format)

Módulo Lua para serialização/desserialização de tabelas e gerenciamento do formato no disco.

> Implementação C: `include/APISLua/kformatlua.c`

## Constantes

| Nome | Valor | Descrição |
|------|-------|-----------|
| `format.HEADER` | `"PLF:1\n"` | Cabeçalho do formato PLF |
| `format.VERSION` | `1` | Versão do formato |

## Funções

### `format.encode(tabela)`

Serializa uma tabela Lua para string no formato PLF.

**Parâmetros**:
- `tabela` (`table`)

**Retorno**: `string` ou `nil, string` (erro)

```lua
local dados = { nome = "pudim", calorias = 300, gostoso = true }
local plf = format.encode(dados)
print(plf)
```

Suporta:
- Strings, números, booleanos
- Tabelas aninhadas (profundidade máxima: 8)

---

### `format.decode(texto)`

Desserializa uma string PLF de volta para tabela Lua.

**Parâmetros**:
- `texto` (`string`): dados no formato PLF

**Retorno**: `table` ou `nil, string` (erro)

```lua
local tabela = format.decode(plf)
print(tabela.nome) -- "pudim"
```

---

### `format.is_valid(texto)`

Verifica se uma string possui um cabeçalho PLF válido.

**Parâmetros**:
- `texto` (`string`)

**Retorno**: `boolean`

```lua
if format.is_valid(dados) then
    local t = format.decode(dados)
end
```

---

### `format.disk_init()`

Formata o disco com um PLFS vazio (apaga todos os arquivos e salva).

**Retorno**: `boolean`

```lua
format.disk_init() -- cuidado: apaga tudo!
```

---

### `format.disk_info()`

Retorna informações sobre o formato do disco.

**Retorno**: `table`

```lua
local info = format.disk_info()
-- info.magic     (string)
-- info.version   (integer)
-- info.files     (integer) — número de arquivos
-- info.available (boolean) — disco disponível
```
