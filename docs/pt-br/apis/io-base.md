# API io, serial e base Lua

Estas funcoes sao registradas em `klua_init` e formam a camada basica de execucao.

## Saida

### `print(...)`

- Escreve valores no output principal (`kio_write`) separados por dois espacos e com quebra de linha final.

### `io`

- `io.write(...)`: escreve sem quebra de linha.
- `io.writeln(...)`: escreve com quebra de linha.
- `io.clear()`: limpa tela/saida atual.

### `serial`

- `serial.write(...)`: escreve apenas argumentos string na serial.
- `serial.writeln(...)`: idem, com `\r\n` ao final.

## Funcoes base registradas

- `tonumber`
- `tostring`
- `type`
- `next`
- `pairs`
- `ipairs`
- `select`
- `error`
- `pcall`
- `rawget`
- `rawset`
- `rawlen`
- `setmetatable`
- `getmetatable`

## Nota

A biblioteca base e propositalmente enxuta para manter o kernel freestanding leve, como uma receita curta com ingredientes bem controlados.
