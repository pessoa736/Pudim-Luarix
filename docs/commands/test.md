# test — Teste de Subsistemas

Executa testes rápidos de subsistemas do kernel.

## Uso

```
test proc
test page [quantidade]
```

## Modos

### `test proc` — Teste de processos

Mostra o estado do escalonador:

```
> test proc
proctest sync start
process active 0 / 16
proctest sync end
```

### `test page [quantidade]` — Teste de alocação de página

Aloca e libera blocos de 4KB como teste de stress do heap:

```
> test page 32
pagetest sync begin 32
pagetest allocated 32
pagetest freed
```

Se a memória acabar antes de alocar tudo:

```
> test page 999
pagetest sync begin 999
pagetest stop 257
pagetest allocated 256
pagetest freed
```

O valor padrão de quantidade é 64.

## Tipo

Comando Lua (tabela `KCMDS` em `commands.lua`).

## Analogia

Como fazer testes de qualidade do pudim — verificar se a massa fica firme (memória) e se o forno funciona (processos).
