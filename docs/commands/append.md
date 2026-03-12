# append — Adicionar Ingrediente

Adiciona conteúdo ao final de um arquivo existente.

## Uso

```
append <nome> <conteúdo>
```

## Exemplos

```
> write notas.txt ingrediente 1
> append notas.txt ingrediente 2
> read notas.txt
ingrediente 1ingrediente 2
```

## Detalhes

- O conteúdo é adicionado ao final sem separador automático.
- Se o arquivo não existir, o append falha silenciosamente.

## Analogia

Como adicionar mais ingredientes em um pote que já tem conteúdo.
