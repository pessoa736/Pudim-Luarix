# API event

Global table: event

Calls:

- event.subscribe(name, callback)
- event.unsubscribe(id)
- event.emit(name[, data])
- event.pending()
- event.poll()
- event.clear()
- event.subscribers([name])
- event.timer_interval([ticks])

Event dispatch runs during the terminal loop in bounded batches.
