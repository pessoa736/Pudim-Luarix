# API sys

Tabela global: `sys`

## Funcoes

| Funcao | Assinatura | Retorno |
|---|---|---|
| `version` | `sys.version()` | `string` da versao do kernel |
| `uptime_ms` | `sys.uptime_ms()` | `integer` (milissegundos) |
| `uptime_us` | `sys.uptime_us()` | `integer` (microssegundos) |
| `memory_total` | `sys.memory_total()` | `integer` (RAM utilizavel em bytes) |
| `memory_free` | `sys.memory_free()` | `integer` (heap livre em bytes) |
| `memory_used` | `sys.memory_used()` | `integer` (heap usada em bytes) |
| `heap_total` | `sys.heap_total()` | `integer` (heap total) |
| `boot_media_total` | `sys.boot_media_total()` | `integer` (tamanho da midia de boot) |
| `rom_total` | `sys.rom_total()` | `integer` (alias de capacidade de boot para saida de terminal) |
| `kernel_image_size` | `sys.kernel_image_size()` | `integer` (bytes da imagem do kernel) |
| `cpu_count` | `sys.cpu_count()` | `integer` (threads logicas) |
| `cpu_physical_cores` | `sys.cpu_physical_cores()` | `integer` (nucleos fisicos estimados) |
| `cpu_vendor` | `sys.cpu_vendor()` | `string` |
| `cpu_brand` | `sys.cpu_brand()` | `string` |
| `memory_map_count` | `sys.memory_map_count()` | `integer` (entradas E820) |
| `memory_map_entry` | `sys.memory_map_entry(index)` | `table` ou `nil` |

## Estrutura de memory_map_entry

Retorna tabela com os campos:

- `base`: inteiro com endereco base.
- `length`: inteiro com tamanho da faixa.
- `type`: inteiro E820.
- `kind`: texto amigavel (`usable`, `reserved`, `acpi_reclaim`, `acpi_nvs`, `bad`, `unknown`).
- `base_hex`: string hexadecimal (`0x...`).
- `length_hex`: string hexadecimal (`0x...`).

## Exemplo

```lua
print(sys.version())
print(sys.cpu_vendor(), sys.cpu_brand())

for i = 1, sys.memory_map_count() do
    local e = sys.memory_map_entry(i)
    if e then
        print(i, e.base_hex, e.length_hex, e.kind)
    end
end
```
