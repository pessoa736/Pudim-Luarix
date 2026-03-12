# Funções Globais Lua

Funções disponíveis globalmente sem prefixo de módulo.

> Implementação C: `include/APISLua/klua.c`

## Funções

### `print(...)`

Imprime valores separados por espaço, seguidos de quebra de linha.

```lua
print("olá", "mundo", 42)
-- saída: "olá    mundo    42\n"
```

---

### `tonumber(valor)`

Converte um valor para número.

**Retorno**: `number` ou `nil`

---

### `tostring(valor)`

Converte um valor para string.

**Retorno**: `string`

---

### `type(valor)`

Retorna o nome do tipo de um valor.

**Retorno**: `string` (`"nil"`, `"boolean"`, `"number"`, `"string"`, `"table"`, `"function"`, `"userdata"`)

---

### `next(tabela, chave)`

Iterador de tabela — retorna o próximo par chave/valor.

**Retorno**: `chave, valor` ou `nil`

---

### `pairs(tabela)`

Retorna o iterador genérico para tabelas.

```lua
for k, v in pairs(t) do print(k, v) end
```

---

### `ipairs(tabela)`

Retorna o iterador sequencial para arrays (chaves inteiras 1-based).

```lua
for i, v in ipairs(t) do print(i, v) end
```

---

### `select(indice, ...)`

Seleciona valores dos varargs. `"#"` retorna a contagem.

```lua
print(select("#", 1, 2, 3)) -- 3
print(select(2, "a", "b", "c")) -- "b"  "c"
```

---

### `error(msg)`

Levanta um erro Lua (nunca retorna).

---

### `pcall(fn, ...)`

Chamada protegida — executa uma função capturando erros.

**Retorno**: `boolean, ...` (sucesso + resultados ou mensagem de erro)

```lua
local ok, err = pcall(function() error("ops") end)
print(ok, err) -- false  "ops"
```

---

### `rawget(tabela, chave)`

Acesso direto à tabela (ignora metamétodos).

---

### `rawset(tabela, chave, valor)`

Escrita direta na tabela (ignora metamétodos).

---

### `rawlen(valor)`

Comprimento direto (ignora metamétodos `__len`).

---

### `setmetatable(tabela, mt)`

Define a metatabela de uma tabela.

**Retorno**: a própria tabela

---

### `getmetatable(valor)`

Retorna a metatabela de um valor, ou `nil`.
