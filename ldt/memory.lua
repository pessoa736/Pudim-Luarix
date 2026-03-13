---@meta

--- Memory management API for Lua scripts.
---@class MemoryAPI
local memory = {}

--- Allocates a memory block and returns pointer as integer.
---@param size integer
---@return integer
function memory.allocate(size) end

--- Frees a previously allocated pointer.
---@param ptr integer
---@return boolean
function memory.free(ptr) end

--- Applies protection flags to a pointer region.
---@param ptr integer
---@param flags integer
---@return boolean
function memory.protect(ptr, flags) end

--- Returns a textual memory statistics summary.
---@return string
function memory.dump_stats() end

--- Global memory API table.
---@type MemoryAPI
_G.memory = memory
