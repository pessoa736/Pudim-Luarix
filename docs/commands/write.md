# write — Escrever Receita

Cria ou sobrescreve um arquivo no sistema de arquivos em memória.

## Uso

```
write <nome> <conteúdo>
```

## Exemplos

```
> write receita.txt pudim de leite condensado
> read receita.txt
pudim de leite condensado
```

## Detalhes

- Se o arquivo já existir, o conteúdo é substituído.
- Se não existir, o arquivo é criado.
- Todo conteúdo após o nome do arquivo é tratado como o conteúdo.

## Analogia

Como escrever uma receita nova e guardar na despensa.
