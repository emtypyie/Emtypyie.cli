# Pyielink

Pyielink v0.5.0 — Remote access framework

## Quick install

```bash
# Via emtypyie.cli
emtypyie /get pyielink

# Or npm
npm install -g pyielink
```

## Launch — C CLI

Once installed, the binary is on your `PATH` as `pyielink`:

```bash
pyielink user@ip                   # Connect to host (GUI mode)
pyielink --repl user@ip            # REPL terminal mode
pyielink enable                    # Enable host for connections
pyielink enable --all              # Open host to any IP
pyielink enable --whitelist IP     # Allow specific IP only
pyielink whitelist add IP          # Add IP to whitelist
pyielink whitelist remove IP       # Remove IP from whitelist
```

This prints the startup boot animation and drops you into the interactive shell.
Type `/help` for commands, or `/about` for version info.

To skip the boot animation:
```bash
pyielink --no-animation
# or
set EMTYPYIE_NO_ANIM=1
```

Direct commands also work without entering the shell, e.g. `pyielink /get gcc` or `pyielink /list`.

## Launch — GUI (Windows only)

Download `emtypyie.cli-Wrapper.zip` from the release, extract it anywhere, and run:
```bash
emtypyieWrapper.exe
```

On first launch, a desktop shortcut is created automatically. The GUI opens a frameless window with a custom title bar (drag to move, grouped split/settings/win controls). Each tab runs its own C engine session with streaming output and a status bar.

## Commands

| Command | Description |
|---------|-------------|
| `/help` | Show help |
| `/list` | List available projects |
| `/get <project>` | Install a project |
| `/get pyielink` | Install Pyielink remote access framework |
| `/get gcc` | Auto-install GCC/G++ compiler |
| `/info <project>` | Show project details |
| `/flash <project>` | Re-download latest version |
| `/rm <project>` | Remove project |
| `/theme <name>` | Change color theme |
| `/bf` | System info screen (bakafetch) |
| `/docs <project>` | Open project docs |
| `/shell` | Interactive mode |
| `/clear` | Clear screen |

## Structure

| Path | Description |
|------|-------------|
| `archive/vX.Y.Z/Root4c/`    | C CLI — portable single binary (primary, recommended) |
| `archive/vX.Y.Z/Root4node/` | Node.js CLI — published to npm |
| `archive/vX.Y.Z/Root4gui/`  | Electron GUI wrapper (frameless, multi-tab, streaming) |
| `mainsite/`  | Website landing pages |
| `manifests/` | Winget package manifests |
| `choco/`     | Chocolatey package |

## C CLI (Root4c)

Single-binary CLI written in C11/C++17, no runtime dependencies.

- **Build:** `cmake -B build && cmake --build build` (MinGW-w64 / MSVC)
- **Themes:** slate, green, amber, violet, cyan
- **Bakafetch:** Unicode/braille art system info screen
- **Larpino:** Built-in LLM inference engine
  - Loads GGUF format LLAMA models (Q4_0, Q4_1, Q5_0, Q5_1, Q8_0, F16, F32)
  - BPE tokenizer with merge rules
  - KV-cached transformer (RoPE, SwiGLU, RMS norm, GQA)
  - Top-k / temperature sampling
- **/larpino enable/disable/status:** LLAMA inference engine
- **/get larpino@1b:** Downloads a model from the CDN
- **CDN registry:** fetches project list and metadata from `cdn.emtypyie.in/dev`
- **No args:** opens interactive shell (with startup boot animation)

## GUI (Root4gui) — emtypyie.cli-Wrapper

Electron-based GUI wrapper for the C CLI, targeting Windows x64.

- **Wrapper version:** 1.0.1
- **Executable:** `emtypyieWrapper.exe`
- **Engine:** Electron 32 + asar packaging
- **Frameless window:** custom title bar with drag, 46x30px window buttons
- **Multi-tab:** each tab spawns its own C engine process (child_process.spawn)
- **Streaming output:** per-line DOM writes — no buffer, visible during long operations
- **Status bar:** per-tab footer showing current command + animated dot
- **Settings panel:** accent swatches, font size, env variables (GitHub/npm tokens)
- **Ricing section:** background image opacity slider (0–100%, persisted to localStorage)
- **Runtime check:** on startup, verifies `emtypyie.exe` exists; if missing, prompts to download from GitHub
- **Update mechanism:** checks GitHub releases, downloads + extracts `emtypyie.exe` from asset zip
- **Icon:** custom logo.ico, set as EXE and window icon
- **Packaging:** `electron-packager` with `--asar`, `--extraResource` for the C binary

## Release artifacts

Each GitHub release ships three Windows artifacts:

| ZIP | Contents | Source |
|-----|----------|--------|
| `emtypyie-cli-windows-x64-3.0.1.zip` | `emtypyie.exe` | Node.js (pkg) — npm release |
| `emtypyie-cli-native-windows-x64-3.0.1.zip` | `emtypyie.exe` | C native build |
| `emtypyie.cli-Wrapper.zip` | `emtypyieWrapper.exe` + resources | Electron GUI wrapper |