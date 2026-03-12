# lua — Executar Lua

Executa código Lua diretamente do terminal.

## Uso

```
lua <código>
```

## Exemplos

```
> lua print("ola mundo")
ola mundo

> lua for i=1,5 do print(i) end
1
2
3
4
5

> lua sys.version()
```

## Detalhes

- Primeiro tenta executar como comando KCMDS.
- Se não for um comando registrado, executa como código Lua puro.
- Erros são exibidos no terminal com traceback.

## Analogia

Como mexer na massa do pudim diretamente — acesso à linguagem de extensão do kernel.
