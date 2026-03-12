# runq — Executar Fila de Processos

Executa ticks do escalonador de processos manualmente.

## Uso

```
runq
```

## Saída

```
> runq
runq ran 3 active 1
```

## Detalhes

- Executa até 64 ticks do escalonador.
- Cada tick dá uma fatia de tempo a um processo pronto.
- Para quando não há mais processos ou esgota o budget de 64 ticks.
- Mostra quantos ticks efetivamente executaram processos e quantos processos permanecem ativos.

## Tipo

Comando Lua (tabela `KCMDS` em `commands.lua`).

## Analogia

Como colocar os pudins no forno e rodar o timer — cada tick é um minuto de cozimento.
