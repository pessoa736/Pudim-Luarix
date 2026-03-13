---@meta

--- Keyboard input API.
---@class KeyboardAPI
local keyboard = {}

--- Returns one key name or character without blocking.
---@return string|nil
function keyboard.getkey() end

--- Returns one printable character without blocking.
---@return string|nil
function keyboard.getchar() end

--- Returns true when input is available.
---@return boolean
function keyboard.available() end

--- Global keyboard API table.
---@type KeyboardAPI
_G.keyboard = keyboard
