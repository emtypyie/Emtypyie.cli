# emtypyie-cli v3.0.3

> Node.js CLI for EMTYPYIE — run emtypyie projects from your terminal.

**Release:** Baking Bread
**npm:** `npm install -g emtypyie-cli`
**Documentation:** https://wiki.emtypyie.in
**Main site:** https://emtypyie.in/cli

## Quick Start

```bash
# Install globally
npm install -g emtypyie-cli

# Run interactive shell
emtypyie

# Or run direct commands
emtypyie /list
emtypyie /get qrkraft
emtypyie /version
emtypyie --upgrade
```

## Commands

### Core Commands

| Command | Description |
|---------|-------------|
| `/help` | Show help |
| `/about` | About emtypyie |
| `/version` / `-v` / `--version` | Show version with ASCII art |
| `/list` | List available projects from CDN |
| `/get <project>` | Install a project |
| `/get gcc` | Auto-install GCC/G++ compiler |
| `/get larpino@1b` | Download a LLAMA GGUF model |
| `/info <project>` | Show project details |
| `/flash <project>` | Re-download latest version |
| `/rm <project>` | Remove project |
| `/theme <name>` | Change color theme |
| `/theme bakafetch <color>` | Change bakafetch color |
| `/bakafetch` / `/bf` | System info screen |
| `/docs <project>` | Open project documentation |
| `/wiki` | Open wiki.emtypyie.in |
| `/changelog` | Open GitHub releases |
| `/clear` | Clear screen |
| `/update` | Check for CLI updates |
| `/upgrade` / `--upgrade` | Update CLI and restart |
| `/shell` | Enter interactive mode |
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

### Project Templates

Create new projects from templates:

| Command | Description |
|---------|-------------|
| `/new <template> <name> [dir]` | Create project from template |
| `/init <template> <name> [dir]` | Alias for /new |
| `/new list` | List available templates |

**Available templates:** `python-cli`, `node-cli`, `rust-cli`

### Dependency Management

For installed projects with declared dependencies:

```bash
/<project> deps install   # Install all dependencies
/<project> deps update    # Update to latest versions
/<project> deps list      # List declared dependencies
/<project> deps check     # Verify all dependencies satisfied
```

Supports Python (pip), Node.js (npm), Rust (cargo), and system binaries.

### Issues

```bash
/issue <project>   # Open emtypyie.in/issues/<project>
```

### Interactive Shell

Run `emtypyie` without arguments to enter the interactive shell with:
- Startup boot animation
- Tab completion
- Command history
- Bakafetch integration

Skip animation: `emtypyie --no-animation` or `EMTYPYIE_NO_ANIM=1`

## Themes

slate, green, amber, violet, cyan

Change theme: `/theme <name>`

## Auto-Update Check

On every launch, the CLI checks for updates in the background and notifies if a new version is available:

```
A new Version Of Emtypyie.cli is Available v3.0.2 ---> v3.0.3
Run emtypyie --upgrade to update and restart
```

## Integrity Verification

Projects are verified on install and run:
- SHA256 manifest generated on install
- Quick integrity check before each run
- Tamper warning: `<Project> has been tampered. Please run /<project> rebuild for smooth experience`
- Full verification: `/<project> verify`

## CDN Registry

Fetches project list and metadata from `https://cdn.emtypyie.in/dev`

## Links

- **Main site:** https://emtypyie.in/cli
- **Documentation:** https://wiki.emtypyie.in
- **Issues:** https://emtypyie.in/issues
- **GitHub:** https://github.com/emtypyie/emtypyie-cli
- **npm:** https://www.npmjs.com/package/emtypyie-cli

## License

Proprietary — Copyright (c) 2026 EMTYPYIE. All Rights Reserved.