# root4c — EMTYPYIE CLI v3.0.4 (C port)

C-based port of the emtypyie CLI. Single portable binary, zero dependencies.

**Release:** Baking Bread
**Build:** `cmake -B build && cmake --build build`
**Documentation:** https://wiki.emtypyie.in
**Main site:** https://emtypyie.in/cli

Requires MinGW-w64 or MSVC on Windows, GCC/Clang on POSIX.

## Commands

### Core Commands

| Command | Description |
|---------|-------------|
| `/help` | Show help |
| `/about` | About emtypyie |
| `/version` | Show version with ASCII art |
| `/list` | List available projects |
| `/get <project>` | Install a project |
| `/get gcc` | Auto-install GCC/G++ compiler |
| `/get larpino@1b` | Download a LLAMA GGUF model |
| `/info <project>` | Show project details |
| `/flash <project>` | Re-download latest version |
| `/rm <project>` | Remove project |
| `/theme <name>` | Change color theme |
| `/bakafetch` / `/bf` | System info (bakafetch) |
| `/docs <project>` | Open documentation |
| `/wiki` | Open wiki.emtypyie.in |
| `/changelog` | Open GitHub releases |
| `/clear` | Clear screen |
| `/shell` | Interactive mode |
| `/larpino enable|disable|status` | LLAMA inference engine |
| `/update` | Check for CLI updates |
| `/upgrade` | Update CLI and restart |
| `/run <project>` | Launch/run a project |
| `/exit` / `/quit` | Exit interactive shell |

### Project Subcommands

After installing a project (`/get <project>`), manage it with:

| Command | Description |
|---------|-------------|
| `/<project> --upgrade` | Upgrade project to latest version |
| `/<project> -v` | Show project version & package managers |
| `/<project> rebuild` | Re-download and verify integrity |
| `/<project> verify` | Check file integrity (SHA256) |
| `/<project> deps [action]` | Manage dependencies (install/update/list/check) |
| `/<project> info` | Show detailed info (tech stack, deps, integrity) |

### Interactive Shell

Run `emtypyie` without arguments to enter the interactive shell with:
- Startup boot animation
- Tab completion
- Command history
- Bakafetch integration
- Larpino chat mode (`/larpino enable`)

Skip animation: `emtypyie --no-animation` or `EMTYPYIE_NO_ANIM=1`

## Themes

slate, green, amber, violet, cyan

Change theme: `/theme <name>`

## Auto-Update Check

On every launch, the CLI checks for updates in the background and notifies if a new version is available.

## Integrity Verification

Projects are verified on install and run:
- SHA256 manifest generated on install (Windows: BCrypt, POSIX: OpenSSL)
- Quick integrity check before each run
- Tamper warning: `<Project> has been tampered. Please run /<project> rebuild for smooth experience`
- Full verification: `/<project> verify`

## CDN Registry

Fetches project list and metadata from `https://cdn.emtypyie.in/dev`

## Links

- **Main site:** https://emtypyie.in/cli
- **Documentation:** https://wiki.emtypyie.in
- **Issues:** https://emtypyie.in/issues
- **GitHub:** https://github.com/emtypyie/Emtypyie.cli

## License

Proprietary — Copyright (c) 2026 EMTYPYIE. All Rights Reserved.