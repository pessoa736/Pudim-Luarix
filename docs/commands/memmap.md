# memmap — Mapa de Memória

Exibe o mapa de memória E820 fornecido pelo BIOS/UEFI.

## Uso

```
memmap
```

## Saída

```
> memmap
=== E820 Memory Map ===
Entries: 6
#1 base=0x0 length=0x9FC00 type=usable(1)
#2 base=0x9FC00 length=0x400 type=reserved(2)
#3 base=0xF0000 length=0x10000 type=reserved(2)
#4 base=0x100000 length=0x7EE0000 type=usable(1)
#5 base=0xFFFC0000 length=0x40000 type=reserved(2)
```

## Tipos de região

| Tipo | Código | Descrição |
|------|--------|-----------|
| `usable` | 1 | RAM disponível para uso |
| `reserved` | 2 | Reservada pelo hardware |
| `acpi_reclaim` | 3 | ACPI reclaimable |
| `acpi_nvs` | 4 | ACPI Non-Volatile Storage |
| `bad` | 5 | Memória defeituosa |

## Tipo

Comando Lua (tabela `KCMDS` em `commands.lua`).

## Nota

No aarch64, o mapa de memória E820 não está disponível (retorna 0 entries).

## Analogia

Como ver a planta da confeitaria — quais áreas estão livres, quais estão ocupadas por equipamentos.
