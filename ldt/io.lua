---@meta

--- Interactive console output API.
---@class IOAPI
local io = {}

--- Writes values without newline.
---@param ... any
function io.write(...) end

--- Writes values followed by newline.
---@param ... any
function io.writeln(...) end

--- Clears terminal output surface.
function io.clear() end

--- Global IO API table.
---@type IOAPI
_G.io = io
