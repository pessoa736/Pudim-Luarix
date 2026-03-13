---@meta

--- Process information returned by process.list().
---@class ProcessInfo
---@field pid integer Process id.
---@field state 'ready'|'running'|'error'|'dead' Current scheduler state.

--- Capability flags visible to the current process identity.
---@class ProcessCapabilities
---@field setuid boolean Can change uid.
---@field chmod boolean Can change file mode.
---@field chown boolean Can change file owner/group.
---@field kill boolean Can terminate other processes.
---@field fs_admin boolean Has file system admin privileges.
---@field process_admin boolean Has process admin privileges.

--- Process scheduler and identity API.
---@class ProcessAPI
local process = {}

--- Creates a new process from Lua source text or function.
---@param script_or_function string|fun(...: any)
---@return integer
function process.create(script_or_function) end

--- Lists active process entries.
---@return ProcessInfo[]
function process.list() end

--- Terminates a process by pid.
---@param pid integer
---@return boolean
function process.kill(pid) end

--- Yields current process execution.
function process.yield() end

--- Returns current process id.
---@return integer
function process.get_id() end

--- Runs one scheduler tick.
---@return boolean
function process.tick() end

--- Returns number of active process slots.
---@return integer
function process.count() end

--- Returns max process capacity.
---@return integer
function process.max() end

--- Returns current process user id.
---@return integer
function process.getuid() end

--- Attempts to change current process user id.
---@param uid integer
---@return boolean
function process.setuid(uid) end

--- Returns current process group id.
---@return integer
function process.getgid() end

--- Attempts to change current process group id.
---@param gid integer
---@return boolean
function process.setgid(gid) end

--- Returns capability booleans for current identity.
---@return ProcessCapabilities
function process.get_capabilities() end

--- Global process API table.
---@type ProcessAPI
_G.process = process
