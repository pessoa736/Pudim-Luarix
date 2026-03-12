# chown — Mudar Dono

Altera o dono (UID) e grupo (GID) de um arquivo.

## Uso

```
chown <nome> <uid> <gid>
```

## Exemplos

```
> chown receita.txt 1 2
> getperm receita.txt
mode=755 uid=1 gid=2
```

## Restrições

Requer ser root (UID 0) ou ter a capability `CAP_CHOWN`.

## Analogia

Como transferir a propriedade de um pote na despensa para outro confeiteiro.
