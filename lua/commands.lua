KCMDS = {
    version = "serial.writeln('Pudim-Luarix x86_64 v0.1.0')",
    
    sysstats = [[
        serial.writeln('=== System Info ===')
        serial.writeln('Version: ' .. sys.version())
        serial.writeln('Memory: ' .. sys.memory_total() .. ' bytes')
        serial.writeln('Memory free: ' .. sys.memory_free() .. ' bytes')
        serial.writeln('Memory used: ' .. sys.memory_used() .. ' bytes')
        serial.writeln('CPUs: ' .. sys.cpu_count())
        serial.writeln('Uptime: ' .. sys.uptime_ms() .. ' ms')
    ]]
}
