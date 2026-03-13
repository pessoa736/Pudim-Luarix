# API memory

Global table: memory

Calls:

- memory.allocate(size)
- memory.free(ptr)
- memory.protect(ptr, flags)
- memory.dump_stats()

Pointers are passed to Lua as integers.
