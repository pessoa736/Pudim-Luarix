---@diagnostic disable: undefined-global, undefined-field


local function outln(msg)
    io.writeln(msg)
    serial.writeln(msg)
end

KCMDS = {
    version = function(...)
        outln('Pudim-Luarix x86_64 v0.1.0')
    end,
    sysstats = function(...)
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
    memmap = function(...)
        local total = sys.memory_map_count()
        outln('=== E820 Memory Map ===')
        outln('Entries: ' .. total)
        for i = 1, total do
            local e = sys.memory_map_entry(i)
            if e then
                outln('#' .. i .. ' base=' .. e.base_hex .. ' length=' .. e.length_hex .. ' type=' .. e.kind .. '(' .. e.type .. ')')
            end
        end
    end,
    runq = function(...)
        local budget = 64
        local ran = 0

        for i = 1, budget do
            if process.tick() then
                ran = ran + 1
            end

            if process.count() == 0 then
                break
            end
        end

        print('runq ran', ran, 'active', process.count())
    end,
    test = function(...)
        local args = {...}
        local mode = args[1] or 'status'

        if mode == 'proc' or mode == 'proctest' then
            print('proctest sync start')
            print('process active', process.count(), '/', process.max())
            print('proctest sync end')
            return
        end

        if mode == 'page' or mode == 'pagetest' then
            local blocks = {}
            local count = tonumber(args[2]) or 64

            if count < 1 then
                count = 1
            end

            print('pagetest sync begin', count)

            for i = 1, count do
                local p = memory.allocate(4096)
                if p == 0 then
                    print('pagetest stop', i)
                    break
                end
                blocks[#blocks + 1] = p
            end

            print('pagetest allocated', #blocks)

            for i = 1, #blocks do
                memory.free(blocks[i])
            end

            print('pagetest freed')
            return
        end

        print('usage: test proc | test page [count]')
    end
}
