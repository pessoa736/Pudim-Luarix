# save — Guardar na Despensa

Persiste o sistema de arquivos em memória (PLFS) no disco ATA.

## Uso

```
save
```

## Saída

```
> save
fs saved
```

Ou, se não houver disco disponível:

```
> save
fs save failed
```

## Detalhes

O PLFS (Pudim Lua File System) reside em memória. O comando `save` serializa todos os arquivos para o disco secundário em formato PLFS. Sem `save`, os dados são perdidos ao reiniciar.

## Analogia

Como colocar o pudim na geladeira — se não guardar, derrete e perde tudo.
