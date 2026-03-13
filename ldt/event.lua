---@meta

--- Allowed event payload scalar types.
---@alias EventData string|integer|number|boolean|nil

--- Event queue and subscription API.
---@class EventAPI
local event = {}

--- Subscribes a callback to an event name.
---@param name string
---@param callback fun(data: EventData)
---@return integer
function event.subscribe(name, callback) end

--- Removes one subscription by id.
---@param id integer
---@return boolean
function event.unsubscribe(id) end

--- Emits an event with optional payload.
---@param name string
---@param data? EventData
---@return boolean
function event.emit(name, data) end

--- Returns pending event count in queue.
---@return integer
function event.pending() end

--- Pops one event from queue.
---@return string|nil, EventData
function event.poll() end

--- Clears queued events and subscribers.
function event.clear() end

--- Returns subscriber count, optionally filtered by name.
---@param name? string
---@return integer
function event.subscribers(name) end

--- Gets or sets timer event interval in ticks.
---@param ticks? integer
---@return integer
function event.timer_interval(ticks) end

--- Global event API table.
---@type EventAPI
_G.event = event
