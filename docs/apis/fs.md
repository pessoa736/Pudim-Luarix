# API fs — Sistema de Arquivos (Despensa)

Módulo Lua para gerenciamento de arquivos no PLFS (Pudim Lua File System).

> Implementação C: `include/APISLua/kfslua.c`
> Registrado como `fs` e `kfs` (ambos acessam a mesma tabela).

## Funções

### `fs.write(nome, conteudo)`

Cria ou sobrescreve um arquivo.

**Parâmetros**:
- `nome` (`string`): nome do arquivo
- `conteudo` (`string`): conteúdo a escrever

**Retorno**: `boolean`

```lua
fs.write("receita.txt", "pudim de leite")
```

---

### `fs.append(nome, conteudo)`

Adiciona conteúdo ao final de um arquivo existente.

**Parâmetros**:
- `nome` (`string`)
- `conteudo` (`string`)

**Retorno**: `boolean`

---

### `fs.read(nome)`

Lê o conteúdo de um arquivo.

**Parâmetros**:
- `nome` (`string`)

**Retorno**: `string` ou `nil` (se não existir)

```lua
local conteudo = fs.read("receita.txt")
if conteudo then print(conteudo) end
```

---

### `fs.delete(nome)`

Deleta um arquivo.

**Parâmetros**:
- `nome` (`string`)

**Retorno**: `boolean`

---

### `fs.exists(nome)`

Verifica se um arquivo existe.

**Parâmetros**:
- `nome` (`string`)

**Retorno**: `boolean`

---

### `fs.size(nome)`

Retorna o tamanho de um arquivo em bytes.

**Parâmetros**:
- `nome` (`string`)

**Retorno**: `integer` (0 se não existir)

---

### `fs.count()`

Retorna o número total de arquivos.

**Retorno**: `integer`

---

### `fs.list()`

Retorna uma lista com os nomes de todos os arquivos.

**Retorno**: `table` (array de strings)

```lua
for _, nome in ipairs(fs.list()) do
    print(nome)
end
```

---

### `fs.clear()`

Deleta todos os arquivos do sistema.

**Retorno**: `boolean` (sempre true)

---

### `fs.save()`

Persiste o sistema de arquivos no disco (formato PLFS).

**Retorno**: `boolean`

---

### `fs.load()`

Carrega o sistema de arquivos do disco (limpa arquivos atuais primeiro).

**Retorno**: `boolean`

---

### `fs.persistent()`

Verifica se armazenamento persistente está disponível (disco ATA detectado).

**Retorno**: `boolean`

---

### `fs.chmod(nome, modo)`

Altera permissões de um arquivo.

**Parâmetros**:
- `nome` (`string`)
- `modo` (`integer`): valor octal 0-777

**Retorno**: `boolean`

Requer root, ser o dono, ou `CAP_CHMOD`.

---

### `fs.chown(nome, uid, gid)`

Altera dono e grupo de um arquivo.

**Parâmetros**:
- `nome` (`string`)
- `uid` (`integer`)
- `gid` (`integer`)

**Retorno**: `boolean`

Requer root ou `CAP_CHOWN`.

---

### `fs.getperm(nome)`

Retorna as permissões de um arquivo.

**Parâmetros**:
- `nome` (`string`)

**Retorno**: `table` ou `nil`

```lua
local p = fs.getperm("receita.txt")
-- p.mode (integer)
-- p.uid  (integer)
-- p.gid  (integer)
```
