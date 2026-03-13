---@meta

--- Describes permission metadata for one file.
---@class FsPerm
---@field mode integer Octal-style mode bits.
---@field uid integer File owner id.
---@field gid integer File group id.

--- File system API exposed as fs and kfs.
---@class FsAPI
local fs = {}

--- Writes content to a file, creating or replacing it.
---@param name string
---@param content string
---@return boolean
function fs.write(name, content) end

--- Appends content to the end of a file.
---@param name string
---@param content string
---@return boolean
function fs.append(name, content) end

--- Reads full text content from a file.
---@param name string
---@return string|nil
function fs.read(name) end

--- Deletes a file by name.
---@param name string
---@return boolean
function fs.delete(name) end

--- Checks if a file exists.
---@param name string
---@return boolean
function fs.exists(name) end

--- Returns file size in bytes.
---@param name string
---@return integer
function fs.size(name) end

--- Returns number of files currently stored.
---@return integer
function fs.count() end

--- Returns all file names as an array.
---@return string[]
function fs.list() end

--- Clears all files from in-memory storage.
---@return boolean
function fs.clear() end

--- Persists file system state to backing storage.
---@return boolean
function fs.save() end

--- Loads file system state from backing storage.
---@return boolean
function fs.load() end

--- Returns whether persistent storage is available.
---@return boolean
function fs.persistent() end

--- Changes permission mode of a file.
---@param name string
---@param mode integer
---@return boolean
function fs.chmod(name, mode) end

--- Changes file owner and group.
---@param name string
---@param uid integer
---@param gid integer
---@return boolean
function fs.chown(name, uid, gid) end

--- Returns permission metadata for a file.
---@param name string
---@return FsPerm|nil
function fs.getperm(name) end

--- Global file system API table.
---@type FsAPI
_G.fs = fs

--- Alias for global file system API table.
---@type FsAPI
_G.kfs = fs
