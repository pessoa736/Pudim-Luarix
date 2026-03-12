# sysstats — Estatísticas do Sistema

Mostra informações completas sobre o hardware e estado do sistema.

## Uso

```
sysstats
```

## Saída

```
> sysstats
=== System Info ===
Version: Pudim-Luarix x86_64 v0.1.0
CPU vendor: GenuineIntel
CPU brand: Intel(R) Core(TM) i7-...
CPU threads: 4
CPU cores: 2
RAM usable: 2147483648 bytes
Heap total: 1048576 bytes
Heap free: 1044480 bytes
Heap used: 4096 bytes
ROM total: 0 bytes
Kernel image: 32768 bytes
Processes: 0/16
Uptime: 1234 ms
```

## Campos

| Campo | Fonte (API) | Descrição |
|-------|-------------|-----------|
| Version | `sys.version()` | Versão do kernel |
| CPU vendor | `sys.cpu_vendor()` | Fabricante da CPU |
| CPU brand | `sys.cpu_brand()` | Nome do modelo da CPU |
| CPU threads | `sys.cpu_count()` | Threads lógicas |
| CPU cores | `sys.cpu_physical_cores()` | Cores físicos |
| RAM usable | `sys.memory_total()` | RAM detectada (E820) |
| Heap total | `sys.heap_total()` | Heap total do kernel |
| Heap free | `sys.memory_free()` | Heap livre |
| Heap used | `sys.memory_used()` | Heap em uso |
| ROM total | `sys.rom_total()` | Tamanho do ROM |
| Kernel image | `sys.kernel_image_size()` | Tamanho da imagem do kernel |
| Processes | `process.count()` / `process.max()` | Processos ativos / máximo |
| Uptime | `sys.uptime_ms()` | Tempo desde o boot |

## Tipo

Comando Lua (tabela `KCMDS` em `commands.lua`).

## Analogia

Como ler a ficha técnica completa da confeitaria — temperatura do forno, ingredientes disponíveis, quantos pudins em preparo.
