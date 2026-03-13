# Terminal Commands (plterm)

This guide documents the native terminal commands registered in the kernel command table.

## Overview

- Prompt: plterm>
- Max useful input length: 250 characters.
- Unknown command behavior: the terminal first tries dynamic Lua commands (KCMDS) before returning unknown command.

Think of this terminal as a kitchen bench: native commands are your fixed utensils, and Lua commands are custom molds you can add as needed.

## Native commands

| Command | Usage | Expected output | Notes |
|---|---|---|---|
| help | help | command list | Includes native commands and dynamic Lua command names. |
| clear | clear | clear screen | Uses current display backend (VGA/serial). |
| md | md [-l|-s|-p] | file list | -l mode/uid/gid/size, -s size, -p permissions. |
| ps | ps | process list | Shows pid and state (ready, running, error, dead). |
| count | count | file count | Total files in in-memory KFS. |
| lcmds | lcmds | Lua command list | Reads names from global KCMDS. |
| save | save | saved to disk or error | Persists KFS if persistent storage exists. |
| whoami | whoami | current user | Terminal context runs as uid 0 (root). |
| write | write <name> <content> | ok or fail | Creates or overwrites a KFS file. |
| read | read <name> | content or not found | Text read from KFS. |
| append | append <name> <content> | ok or fail | Appends text to file tail. |
| rm | rm <name> | ok or fail | Deletes a KFS file. |
| size | size <name> | bytes | Returns 0 for missing files. |
| lua | lua <code> | script output | Runs Lua code; strips wrapping quotes when present. |
| chmod | chmod <name> <mode> | ok or fail | Octal mode (0 to 777). |
| chown | chown <name> <uid> <gid> | ok or fail | Changes file owner and group. |
| getperm | getperm <name> | mode/uid/gid | Prints current file permissions. |
| history | history | numbered history | Shows command layers (the pudding stack). |
| inspect | inspect | heap stats | Blocks/free/largest/free bytes/total bytes. |
| exit | exit | bye | Exits terminal loop. |

## Resolution flow

1. Match against native command table.
2. If no match, try klua_cmd_run_line(line).
3. If still unresolved, return unknown command.
