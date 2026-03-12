# chmod — Mudar Permissões

Altera as permissões de um arquivo.

## Uso

```
chmod <nome> <modo>
```

O modo é um valor octal de 0 a 777 (semelhante ao chmod do Linux).

## Exemplos

```
> chmod receita.txt 755
> getperm receita.txt
mode=755 uid=0 gid=0
```

## Permissões

| Dígito | Significado |
|--------|-------------|
| 7 | leitura + escrita + execução (rwx) |
| 6 | leitura + escrita (rw-) |
| 5 | leitura + execução (r-x) |
| 4 | somente leitura (r--) |
| 0 | nenhuma permissão (---) |

Formato: `[dono][grupo][outros]` — ex: `754` = dono rwx, grupo r-x, outros r--.

## Restrições

Requer ser root (UID 0), dono do arquivo, ou ter a capability `CAP_CHMOD`.

## Analogia

Como colocar cadeados nos potes da despensa — definir quem pode abrir cada um.
