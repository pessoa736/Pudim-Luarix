---@meta

--- Persistent disk format metadata.
---@class FormatDiskInfo
---@field magic string Format signature.
---@field version integer Format version number.
---@field files integer Number of stored files.
---@field available boolean Persistence availability state.

--- Format and serialization API.
---@class FormatAPI
---@field HEADER string Binary/text header marker.
---@field VERSION integer Version constant.
local format = {}

--- Encodes a Lua table to PLFS text representation.
---@param value table
---@return string
function format.encode(value) end

--- Decodes PLFS text representation back to table.
---@param blob string
---@return table
function format.decode(blob) end

--- Checks whether a blob matches expected format header.
---@param blob string
---@return boolean
function format.is_valid(blob) end

--- Initializes persistent disk with empty format.
---@return boolean
function format.disk_init() end

--- Returns current disk format information.
---@return FormatDiskInfo
function format.disk_info() end

--- Global format API table.
---@type FormatAPI
_G.format = format
