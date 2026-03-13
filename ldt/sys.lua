---@meta

--- Represents a firmware memory map entry exposed by the kernel.
---@class SysMemoryMapEntry
---@field base integer Physical base address.
---@field length integer Region length in bytes.
---@field type integer Raw E820 type identifier.
---@field kind string Human-readable memory kind.
---@field base_hex string Base address as hexadecimal string.
---@field length_hex string Length as hexadecimal string.

--- System information API exposed in the global table.
---@class SysAPI
local sys = {}

--- Returns the kernel version string.
---@return string
function sys.version() end

--- Returns uptime in milliseconds.
---@return integer
function sys.uptime_ms() end

--- Returns uptime in microseconds.
---@return integer
function sys.uptime_us() end

--- Returns total usable RAM in bytes.
---@return integer
function sys.memory_total() end

--- Returns free heap memory in bytes.
---@return integer
function sys.memory_free() end

--- Returns used heap memory in bytes.
---@return integer
function sys.memory_used() end

--- Returns total heap capacity in bytes.
---@return integer
function sys.heap_total() end

--- Returns boot media capacity in bytes.
---@return integer
function sys.boot_media_total() end

--- Returns ROM-related capacity shown by terminal stats.
---@return integer
function sys.rom_total() end

--- Returns loaded kernel image size in bytes.
---@return integer
function sys.kernel_image_size() end

--- Returns logical CPU thread count.
---@return integer
function sys.cpu_count() end

--- Returns estimated physical core count.
---@return integer
function sys.cpu_physical_cores() end

--- Returns CPU vendor string.
---@return string
function sys.cpu_vendor() end

--- Returns CPU brand/model string.
---@return string
function sys.cpu_brand() end

--- Returns number of entries in firmware memory map.
---@return integer
function sys.memory_map_count() end

--- Returns one memory map entry by 1-based index.
---@param index integer
---@return SysMemoryMapEntry|nil
function sys.memory_map_entry(index) end

--- Global system API table.
---@type SysAPI
_G.sys = sys
