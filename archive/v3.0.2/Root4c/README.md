# root4c — EMTYPYIE CLI v3.0.2 (C port)

C-based port of the emtypyie CLI. Single portable binary, zero dependencies.

**Release:** Baking Bread
**Status:** In development — structure only, no source files yet.

## Build

```sh
cmake -B build && cmake --build build
```

Requires MinGW-w64 or MSVC on Windows, GCC/Clang on POSIX.

## Commands

| Command | Description |
|---------|-------------|
| `/help` | Show help |
| `/about` | About emtypyie |
| `/list` | List available projects |
| `/get <project>` | Install a project |
| `/get gcc` | Auto-install GCC/G++ compiler |
| `/info <project>` | Show project details |
| `/flash <project>` | Re-download latest version |
| `/rm <project>` | Remove project |
| `/theme <name>` | Change color theme |
| `/bf` | System info (bakafetch) |
| `/docs <project>` | Open documentation |
| `/clear` | Clear screen |
| `/shell` | Interactive mode |

## Themes

slate, green, amber, violet, cyan

## License

Proprietary — Copyright (c) 2026 EMTYPYIE. All Rights Reserved.
