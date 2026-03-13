# API sys

Global table: sys

Core calls include:

- sys.version()
- sys.uptime_ms()
- sys.uptime_us()
- sys.memory_total()
- sys.memory_free()
- sys.memory_used()
- sys.heap_total()
- sys.boot_media_total()
- sys.rom_total()
- sys.kernel_image_size()
- sys.cpu_count()
- sys.cpu_physical_cores()
- sys.cpu_vendor()
- sys.cpu_brand()
- sys.memory_map_count()
- sys.memory_map_entry(index)

memory_map_entry returns fields base, length, type, kind, base_hex, and length_hex.
