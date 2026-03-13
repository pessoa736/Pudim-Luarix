---@meta

--- One profiler marker entry.
---@class DebugTimerEntry
---@field label string Marker label.
---@field ms integer Elapsed milliseconds.

--- Heap snapshot metrics.
---@class DebugHeap
---@field blocks integer Total heap blocks.
---@field free_blocks integer Number of free blocks.
---@field largest_free integer Largest free block size.
---@field free_bytes integer Total free bytes.
---@field total_bytes integer Total managed bytes.

--- Lua VM memory counters.
---@class DebugLuaMem
---@field kb integer Used kibibytes.
---@field bytes_remainder integer Remaining bytes below next KiB.

--- Debug and inspection API.
---@class DebugAPI
local debug = {}

--- Returns locals at a given stack level.
---@param level? integer
---@return table|string|nil
function debug.locals(level) end

--- Returns formatted stack trace text.
---@return string
function debug.stack() end

--- Returns map of global names to Lua type names.
---@return table<string, string>
function debug.globals() end

--- Starts profiling timer.
function debug.timer_start() end

--- Stops timer and returns elapsed milliseconds.
---@return integer
function debug.timer_stop() end

--- Adds a labeled marker to active timer.
---@param label? string
---@return boolean
function debug.timer_mark(label) end

--- Resets profiler timer state.
function debug.timer_reset() end

--- Returns all timer markers.
---@return DebugTimerEntry[]
function debug.timer_report() end

--- Returns heap statistics snapshot.
---@return DebugHeap
function debug.heap() end

--- Returns Lua VM memory usage counters.
---@return DebugLuaMem
function debug.lua_mem() end

--- Returns fragmentation ratio from 0.0 to 1.0.
---@return number
function debug.fragmentation() end

--- Returns command history entries.
---@return string[]
function debug.history() end

--- Global debug API table.
---@type DebugAPI
_G.debug = debug
