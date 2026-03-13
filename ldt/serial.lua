---@meta

--- Serial port output API.
---@class SerialAPI
local serial = {}

--- Writes string arguments to serial output.
---@param ... string
function serial.write(...) end

--- Writes string arguments to serial output and appends CRLF.
---@param ... string
function serial.writeln(...) end

--- Global serial API table.
---@type SerialAPI
_G.serial = serial
