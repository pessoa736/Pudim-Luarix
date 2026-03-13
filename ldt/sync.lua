---@meta

--- Synchronization object interface shared by lock types.
---@class SyncLock
local SyncLock = {}

--- Acquires the lock.
function SyncLock:lock() end

--- Releases the lock.
function SyncLock:unlock() end

--- Alias for lock().
function SyncLock:acquire() end

--- Alias for unlock().
function SyncLock:release() end

--- Synchronization factory API.
---@class SyncAPI
local sync = {}

--- Creates a spinlock object.
---@return SyncLock
function sync.spinlock() end

--- Creates a mutex object.
---@return SyncLock
function sync.mutex() end

--- Global synchronization API table.
---@type SyncAPI
_G.sync = sync
