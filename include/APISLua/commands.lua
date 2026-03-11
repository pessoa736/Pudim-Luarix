---@diagnostic disable: undefined-global, undefined-field


local function outln(msg)
    io.writeln(msg)
    serial.writeln(msg)
end

KCMDS = {
    version = function()
        outln('Pudim-Luarix x86_64 v0.1.0')
    end,
    sysstats = function()
        outln('=== System Info ===')
        outln('Version: ' .. sys.version())
        outln('CPU vendor: ' .. sys.cpu_vendor())
        outln('CPU brand: ' .. sys.cpu_brand())
        outln('CPU threads: ' .. sys.cpu_count())
        outln('CPU cores: ' .. sys.cpu_physical_cores())
        outln('RAM usable: ' .. sys.memory_total() .. ' bytes')
        outln('Heap total: ' .. sys.heap_total() .. ' bytes')
        outln('Heap free: ' .. sys.memory_free() .. ' bytes')
        outln('Heap used: ' .. sys.memory_used() .. ' bytes')
        outln('ROM total: ' .. sys.rom_total() .. ' bytes')
        outln('Kernel image: ' .. sys.kernel_image_size() .. ' bytes')
        outln('Processes: ' .. process.count() .. '/' .. process.max())
        outln('Uptime: ' .. sys.uptime_ms() .. ' ms')
    end,
    memmap = function()
        local total = sys.memory_map_count()
        outln('=== E820 Memory Map ===')
        outln('Entries: ' .. total)
        for i = 1, total do
            local e = sys.memory_map_entry(i)
            if e then
                outln('#' .. i .. ' base=' .. e.base_hex .. ' length=' .. e.length_hex .. ' type=' .. e.kind .. '(' .. e.type .. ')')
            end
        end
    end
}
