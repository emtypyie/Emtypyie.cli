#!/usr/bin/env node

/* ─── emtypyie-cli main entry point (Node.js version) ───
 * Provides an interactive shell (REPL) and direct command execution.
 * Commands are dispatched via handleCommand().
 *
 * The Node.js version has more features than the C port:
 *   /wrap (git staging, npm publish, GitHub repo creation)
 *   /issue (file GitHub issues with optional -m message)
 *   Automatic project execution (run installed projects)
 *   Tab completion via readline
 *
 * Future work:
 *  - Port /wrap, /issue to the C version.
 *  - Add unit tests for all command handlers.
 *  - Fetch command list from cdn (dynamic discovery).
 *  - Persistent shell history across sessions.
 */

const readline = require('readline');
const fs = require('fs');
const path = require('path');
const { execSync, spawn } = require('child_process');
const chalk = require('chalk');
const getCommand = require('./commands/get');
const auth = require('./commands/auth');
const publish = require('./commands/publish');
const bakafetch = require('./commands/bakafetch');
const fetch = require('./commands/fetch');
const runtime = require('./commands/install-runtime');

const t = require('./commands/theme');

/* ─── Startup boot animation (Linux kernel style) ───
 * Embedded directly in cli.js so it needs no external JSON file.
 * Skippable via --no-animation flag or EMTYPYIE_NO_ANIM env var.
 */

const ANIM_LINES = [
  '[  OK  ] Initializing emtypyie-core v2.5.4 (Wandering Witches)',
  '[  OK  ] Architecture: x86_64-windows-nt',
  '[  OK  ] Memory arena allocated: 2.1 GiB / 16 GiB available',
  '[  OK  ] Page table initialized: 4KB pages, 52-bit virtual addressing',
  '[  OK  ] CPU features detected: AVX2, FMA, SSE4.2, POPCNT, BMI1/2',
  '[  OK  ] High-resolution timer: TSC @ 3.4 GHz calibrated',
  '[  OK  ] Interrupt controller: APIC x2APIC mode enabled',
  '[  OK  ] NUMA topology: 1 node, 8 cores, 16 threads detected',
  '[  OK  ] L1 cache: 32KB data + 32KB instruction per core',
  '[  OK  ] L2 cache: 256KB per core, L3 cache: 16MB shared',
  '[  OK  ] Memory bandwidth: ~45 GB/s dual-channel DDR4-3200',
  '[  OK  ] Boot loader: Windows Boot Manager 10.0.19045',
  '[  OK  ] Kernel command line: BOOT_IMAGE=\\Windows\\system32\\winload.efi',
  '[  OK  ] ACPI tables loaded: FACP, APIC, FPDT, SSDT, UEFI',
  '[  OK  ] PCIe topology: Root complex 0000:00:00.0, 3 endpoints',
  '[  OK  ] NVMe controller: Samsung 970 EVO Plus 1TB detected',
  '[  OK  ] Block layer: 512B logical, 4KB physical sectors',
  '[  OK  ] Filesystem: NTFS 3.1 mounted on C:\\ (1.8TB/2.0TB free)',
  '[  OK  ] Console: VT100 virtual terminal sequences enabled',
  '[  OK  ] Codepage: UTF-8 (65001) active for stdout/stderr',
  '[  OK  ] Loading module: core/emtypyie-core (static)',
  '[  OK  ] Symbol table loaded: 2,847 symbols resolved',
  '[  OK  ] Loading module: core/theme-engine (violet, slate, green, amber, cyan)',
  '[  OK  ] Theme registry: 5 themes, 128 color slots each',
  '[  OK  ] Loading module: core/bakafetch (braille renderer v3.1)',
  '[  OK  ] Braille font: 256 glyphs, 8-dot patterns loaded',
  '[  OK  ] System info providers: CPU, GPU, RAM, Disk, OS, Kernel',
  '[  OK  ] Loading module: core/fetch (HTTPS/CDN client)',
  '[  OK  ] HTTP backend: WinHTTP 10.0 (Windows) / libcurl 8.4 (Unix)',
  '[  OK  ] TLS context: TLS 1.3, cipher suites: AES-GCM, CHACHA20-POLY1305',
  '[  OK  ] CDN endpoints: cdn.emtypyie.in, api.emtypyie.in resolved',
  '[  OK  ] Loading module: core/download (multi-threaded)',
  '[  OK  ] Download engine: 4 concurrent streams, resume support',
  '[  OK  ] Checksum verification: SHA256, BLAKE3 hardware accelerated',
  '[  OK  ] Loading module: core/project (registry manager)',
  '[  OK  ] Project index: 47 projects, 12 categories, 3.2MB metadata',
  '[  OK  ] Dependency resolver: topological sort, cycle detection',
  '[  OK  ] Loading module: core/runtime (compiler detector)',
  '[  OK  ] GCC toolchain: gcc.exe 13.2.0 (mingw64) found in PATH',
  '[  OK  ] Clang toolchain: clang.exe 17.0.1 found in PATH',
  '[  OK  ] Node.js runtime: node.exe 20.12.0 found in PATH',
  '[  OK  ] Python runtime: python.exe 3.13.0 found in PATH',
  '[  OK  ] Loading module: core/larpino (LLM inference engine)',
  '[  OK  ] Larpino: GGUF parser v3 (Q4_0, Q4_1, Q5_0, Q5_1, Q8_0, F16, F32)',
  '[  OK  ] BPE tokenizer: 128,256 tokens, 350k merge rules loaded',
  '[  OK  ] Model loader: mmap() zero-copy, lazy page-in enabled',
  '[  OK  ] KV cache allocator: 32 layers x 4096 ctx x 32 heads x 128 dim',
  '[  OK  ] Attention: FlashAttention-2 kernel (AVX2/FMA path)',
  '[  OK  ] RoPE: Rotary positional embeddings precomputed to 8192',
  '[  OK  ] SwiGLU: Fused silu+gate+up projection, BF16 accumulation',
  '[  OK  ] RMSNorm: Vectorized eps=1e-6, 64-byte aligned',
  '[  OK  ] GQA: Grouped-query attention (8 groups, 32 heads)',
  '[  OK  ] Sampler: Top-k=40, Top-p=0.95, temp=0.8, min-p=0.05',
  '[  OK  ] Grammar constraints: GBNF parser for structured output',
  '[  OK  ] Loading module: core/shell (interactive REPL)',
  '[  OK  ] Readline: tab completion, history, vi/emacs modes',
  '[  OK  ] Command registry: 23 built-in commands registered',
  '[  OK  ] Alias system: 12 user aliases loaded from config',
  '[  OK  ] Loading module: core/auth (credential manager)',
  '[  OK  ] Secret store: Windows Credential Manager / libsecret',
  '[  OK  ] GitHub token: ******** (scopes: repo, workflow, gist)',
  '[  OK  ] NPM token: ******** (registry: npm.emtypyie.in)',
  '[  OK  ] Chocolatey key: ******** (source: choco.emtypyie.in)',
  '[  OK  ] Loading module: core/runtime-install (compiler bootstrap)',
  '[  OK  ] MinGW-w64: 13.2.0 package cached (247MB)',
  '[  OK  ] LLVM/Clang: 17.0.1 package cached (1.2GB)',
  '[  OK  ] Bootstrapping: ccache 4.9.1, ninja 1.12.1 ready',
  '[  OK  ] Loading module: core/publish (release automation)',
  '[  OK  ] Git: 2.45.0, GitHub CLI: 2.48.0, npm: 10.5.0',
  '[  OK  ] Signing: cosign 2.2.4, GPG: 2.4.5 (ED25519 key)',
  '[  OK  ] Loading module: core/wrap (project packaging)',
  '[  OK  ] Git staging: diff, status, add, commit, push pipeline',
  '[  OK  ] npm publish: version bump, changelog, provenance',
  '[  OK  ] GitHub Repo API: create, PR, release, deploy keys',
  '[  OK  ] Configuration: ~/.emtypyie/config.json parsed',
  '[  OK  ] Theme preference: violet (saved 2026-07-17)',
  '[  OK  ] Bakafetch color: violet (matches theme)',
  '[  OK  ] Shell prompt: retro-accent \'>> \' with git branch',
  '[  OK  ] History file: ~/.emtypyie/history (1000 entries)',
  '[  OK  ] Log level: INFO (default), DEBUG available via --verbose',
  '[  OK  ] Telemetry: disabled (opt-in only)',
  '[  OK  ] Update channel: stable (github.com/emtypyie/Emtypyie.cli)',
  '[  OK  ] Version check: current 2.5.4, latest 2.5.4 (up to date)',
  '[  OK  ] Plugin system: 0 external plugins loaded',
  '[  OK  ] Sandbox: project isolation via separate working dirs',
  '[  OK  ] Resource limits: max 4GB RAM, 50% CPU per project',
  '[  OK  ] Signal handlers: SIGINT, SIGTERM, SIGBREAK registered',
  '[  OK  ] Crash reporter: minidump to ~/.emtypyie/crashes/',
  '[  OK  ] Loading module: net/cdn-client',
  '[  OK  ] CDN health: 3/3 edge nodes responsive (<50ms)',
  '[  OK  ] Manifest: dev branch, 47 projects, 12.4MB compressed',
  '[  OK  ] ETag cache: 34 entries valid, 13 stale (will refresh)',
  '[  OK  ] Compression: Brotli q=11, Zstd q=19, Gzip q=9',
  '[  OK  ] Loading module: net/github-api',
  '[  OK  ] GitHub API: rate limit 5000/hr, 4987 remaining',
  '[  OK  ] GraphQL endpoint: api.github.com/graphql ready',
  '[  OK  ] REST endpoint: api.github.com REST v3 ready',
  '[  OK  ] Loading module: ui/progress (download bars)',
  '[  OK  ] Progress renderer: Unicode blocks, 60fps cap',
  '[  OK  ] Spinner: 12-frame braille, 80ms/frame',
  '[  OK  ] Loading module: ui/banner (ASCII art)',
  '[  OK  ] Banner: 12 lines, 78 cols, braille witch art',
  '[  OK  ] Loading module: util/fs (filesystem abstraction)',
  '[  OK  ] Path resolver: Windows \\\\?\\ prefix, UNC support',
  '[  OK  ] Atomic write: temp + rename, fsync on close',
  '[  OK  ] Directory watcher: ReadDirectoryChangesW / inotify',
  '[  OK  ] Loading module: util/process (spawn wrapper)',
  '[  OK  ] Job objects: Windows child process groups',
  '[  OK  ] PTY emulation: ConPTY API (Windows 10 1809+)',
  '[  OK  ] Loading module: util/encoding (charset)',
  '[  OK  ] UTF-8: native on Windows 10+, iconv fallback',
  '[  OK  ] Loading module: util/time (monotonic clock)',
  '[  OK  ] QueryPerformanceCounter: 100ns resolution',
  '[  OK  ] Loading module: util/hash (checksums)',
  '[  OK  ] SHA256: Intel SHA-NI, fallback: OpenSSL 3.2',
  '[  OK  ] BLAKE3: SIMD (AVX2/SSE4.1), 3.2 GB/s single-thread',
  '[  OK  ] Loading module: util/json (cJSON 1.7.18)',
  '[  OK  ] Parser: streaming, 1.2M tokens/sec',
  '[  OK  ] Loading module: util/args (CLI parser)',
  '[  OK  ] Flag parser: short/long, positional, subcommands',
  '[  OK  ] Completion: bash, zsh, fish, PowerShell generators',
  '[  OK  ] Loading module: core/doctor (self-diagnostics)',
  '[  OK  ] Health checks: 12/12 passing (network, disk, perms)',
  '[  OK  ] Doctor: PATH, compiler, runtime, tokens, config OK',
  '[  OK  ] Loading module: core/migrate (config upgrades)',
  '[  OK  ] Migration v2.5.x -> v2.5.4: 0 steps required',
  '[  OK  ] Finalizing: flushing stdout/stderr buffers',
  '[  OK  ] Terminal reset: cursor position (0,0), alternate screen',
  '[  OK  ] Signal mask: SIGWINCH handler for resize events',
  '[  OK  ] Emtypyie CLI v2.5.4 ready -- type /help for commands',
  '[  OK  ] Welcome back. The witches are wandering.',
  '[  OK  ] ████████████████████████████████████████ 100%',
];

const ANIM_TOTAL_MS = 2000;
const ANIM_TARGET_COUNT = 300;

async function playStartupAnimation(options = {}) {
  const { skip = false } = options;
  if (skip || process.env.EMTYPYIE_NO_ANIM) return;

  const termWidth = process.stdout.columns || 80;
  const targetWidth = Math.max(40, Math.min(termWidth, Math.floor(termWidth * 0.65)));
  const intervalMs = ANIM_TOTAL_MS / ANIM_TARGET_COUNT;

  for (let i = 0; i < ANIM_TARGET_COUNT; i++) {
    const line = ANIM_LINES[i % ANIM_LINES.length];
    const padded = line.length >= targetWidth
      ? line.slice(0, targetWidth)
      : line + ' '.repeat(targetWidth - line.length);
    process.stdout.write(padded + '\n');
    if (intervalMs > 0) await new Promise(r => setTimeout(r, intervalMs));
  }
}

const BANNER = `
___________        __                         .__                  .__  .__ 
\\_   _____/ ______/  |_ ___.__. ______ ___.__.|__| ____       ____ |  | |__|
 |    __)_ /     \\   __<   |  | \\____ <   |  ||  |/ __ \\    _/ ___\\|  | |  |
 |        \\  Y Y  \\  |  \\___  | |  |_> >___  ||  \\  ___/    \\  \\___|  |_|  |
/_______  /__|_|  /__|  / ____| |   __// ____||__|\\___  > /\\ \\___  >____/__|
        \\/      \\/      \\/      |__|   \\/             \\/  \\/     \\/ 
`;

/* Open a URL in the default browser (Windows-only). */
function openBrowser(url) {
  try {
    execSync(`start ${url}`, { stdio: 'ignore' });
    return true;
  } catch {
    try {
      execSync(`explorer ${url}`, { stdio: 'ignore' });
      return true;
    } catch {
      return false;
    }
  }
}

async function handleCommand(cmd, arg, rl) {
  const normalized = cmd.replace(/^\//, '').toLowerCase();

  switch (normalized) {
    case 'help':
      await showHelp();
      break;

    case 'about':
      showAbout();
      break;

    case 'wiki':
      console.log(t.retro('  Opening wiki.emtypyie.in...'));
      if (openBrowser('https://wiki.emtypyie.in')) {
        console.log(t.retroDim('  Browser opened.'));
      } else {
        console.log(t.retroErr('  Could not open browser. Visit https://wiki.emtypyie.in'));
      }
      break;

    case 'info':
      await showProjectInfo(arg);
      break;

    case 'get':
      await doGet(arg);
      break;

    case 'flash':
      await doFlash(arg);
      break;

    case 'rm':
      await doRemove(arg);
      break;

    case 'issue':
      await doIssue(arg);
      break;

    case 'bakafetch':
    case 'bf':
      bakafetch.show();
      break;

    case 'wrap':
      try {
        await publish.doWrap(rl, arg);
      } catch (e) {
        console.log(t.retroErr(`  Wrap failed: ${e.message}`));
      }
      break;

    case 'setenv':
      await auth.setenv(rl);
      break;

    case 'theme':
      doTheme(arg);
      break;

    case 'clear':
      console.clear();
      break;

    case 'update':
      doUpdate();
      break;

    case 'list':
      await doList();
      break;

    case 'docs':
      await doDocs(arg);
      break;

    case 'changelog':
      doChangelog();
      break;

    case 'exit':
    case 'quit':
      console.log(t.retroDim('\n  System halted.'));
      if (rl) rl.close();
      process.exit(0);
      break;

    default:
      await runProject(normalized);
  }
}

async function showHelp() {
  console.log();
  console.log(t.retroDim('  ─── Commands ───'));
  console.log();
  console.log(t.retro('  /get <project>') + t.retroDim('     install a project'));
  console.log(t.retro('  /get gcc') + t.retroDim('           auto-install GCC/G++ compiler'));
  console.log(t.retro('  /flash <project>') + t.retroDim('   re-download latest version'));
  console.log(t.retro('  /info <project>') + t.retroDim('    show project details'));
  console.log(t.retro('  /rm <project>') + t.retroDim('      delete project files'));
  console.log(t.retro('  /issue <project>') + t.retroDim('   open issue tracker'));
  console.log(t.retro('  /issue <project> -m') + t.retroDim('  file a bug report'));
  console.log(t.retro('  /setenv') + t.retroDim('            set env variables (tokens, creds)'));
  console.log(t.retro('  /theme <name>') + t.retroDim('       change CLI color theme'));
  console.log(t.retro('  /theme bakafetch <c>') + t.retroDim(' change bakafetch color'));
  console.log(t.retro('  /bakafetch') + t.retroDim('         system info with style'));
  console.log(t.retro('  /bf') + t.retroDim('               shortcut for /bakafetch'));
  console.log(t.retro('  /wrap <dir>') + t.retroDim('        stage files in directory'));
  console.log(t.retro('  /wrap <dir> --commit -m') + t.retroDim('  stage, commit & push'));
  console.log(t.retro('  /wrap all') + t.retroDim('          stage all subdirectories'));
  console.log(t.retro('  /wrap npm publish') + t.retroDim('   auto-bump & npm publish'));
  console.log(t.retro('  /wrap repo "name"') + t.retroDim('   create/PR on GitHub'));
  console.log(t.retro('  /clear') + t.retroDim('              clear screen'));
  console.log(t.retro('  /update') + t.retroDim('            update emtypyie'));
  console.log(t.retro('  /list') + t.retroDim('              list projects'));
  console.log(t.retro('  /docs <project>') + t.retroDim('    open project docs'));
  console.log(t.retro('  /changelog') + t.retroDim('         what\'s new'));
  console.log(t.retro('  /about') + t.retroDim('             about emtypyie'));
  console.log(t.retro('  /wiki') + t.retroDim('              open wiki.emtypyie.in'));
  console.log(t.retro('  /help') + t.retroDim('             this screen'));
  console.log(t.retro('  /exit') + t.retroDim('             quit'));
  console.log();
  console.log(t.retroDim('  ─── Projects ───'));
  console.log();
  try {
    const list = await fetch.fetchProjectList();
    const projList = Array.isArray(list) ? list : (list.projects || []);
    for (const entry of projList) {
      const name = typeof entry === 'string' ? entry : entry.name;
      const desc = entry.description || '';
      const pad = ' '.repeat(Math.max(0, 12 - name.length));
      console.log(`  ${t.retroAccent(name)}${pad}${t.retroDim(desc)}`);
    }
  } catch {
    console.log(t.retroDim('  (Could not fetch project list)'));
  }
  console.log();
}

function getReleaseDate() {
  try {
    return execSync('git log -1 --format=%ad --date=short', { encoding: 'utf8', timeout: 5000, stdio: ['pipe', 'pipe', 'ignore'] }).toString().trim();
  } catch {
    const pkg = require('./package.json');
    return pkg.releaseDate || 'unknown';
  }
}

function generateLicense() {
  const chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
  let result = '';
  for (let i = 0; i < 16; i++) {
    result += chars.charAt(Math.floor(Math.random() * chars.length));
  }
  return result;
}

const WITCHES_ART = [
  '⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢢⡄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀',
  '⠀⠀⠀⠀⣀⣤⡀⠀⠀⠍⠭⣔⣄⠀⠀⠀⠀⠀⢈⣿⣆⠀⠀⠀⠀⠀⠀⠀⠀⠀',
  '⢰⢂⣀⣀⣤⣀⣹⣷⣤⣀⣀⠀⢉⣿⣶⣶⣶⣶⣿⣿⣿⠷⠆⠀⠀⠀⠀⣀⣄⠀',
  '⠀⠈⠉⠹⠟⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣟⠋⠀⠀⠀⡤⠀⠀⣿⣏⠀',
  '⠀⠀⠀⠐⠹⢟⣿⣿⣿⣿⣿⣿⣿⣿⡿⠋⣽⣿⣿⠿⣆⠀⠀⠀⠀⢸⢀⣿⣿⠀',
  '⠀⠀⠀⠀⠀⠠⢴⣿⠟⢛⣿⣿⣟⣩⣴⣾⣿⡉⠹⢦⡈⠳⢤⡀⠀⠈⢼⣿⣿⡄',
  '⠀⢀⣀⣀⣀⡀⠀⠀⠀⠀⠋⠀⣿⣿⣿⣿⣿⣿⣦⣀⡉⠶⡤⠈⠛⠂⠉⠁⠀⠀',
  '⠀⠀⢺⣿⣿⣿⡿⠿⠿⣟⣛⣭⣽⣿⣿⣿⣿⣿⣿⠏⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀',
  '⠀⠀⠀⢉⠕⠺⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀',
  '⠀⠀⠀⠀⠀⢠⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀',
  '⠀⠀⠀⠀⠀⠈⠉⠁⠛⢻⣿⠟⢑⡾⠁⣿⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀',
  '⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⠁⠀⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀ ',
];

function showAbout() {
  console.log();
  const pkg = require('./package.json');
  console.log(t.retro(`  EMTYPYIE CLI v${pkg.version}`));
  console.log(t.retroDim('  "Wandering Witches"'));
  console.log();
  for (const line of WITCHES_ART) console.log(t.retroAccent(line));
  console.log();
  console.log(t.retroDim('  Release: ') + t.retroAccent('Wandering Witches'));
  console.log(t.retroDim('  DESIGNED AND ENGINEERED BY  EMTYPYIE'));
  console.log(t.retroDim(`  Copyright \u00a9 ${new Date().getFullYear()} EMTYPYIE. All rights reserved.`));
  console.log();
}

async function showProjectInfo(name) {
  if (!name) {
    console.log(t.retroErr('  Specify a project: /info <project>'));
    return;
  }
  let proj;
  try {
    proj = await fetch.fetchProject(name.toLowerCase());
  } catch (err) {
    console.log(t.retroErr(err.message));
    return;
  }
  console.log();
  console.log(t.retroDim('  ─── ') + t.retroAccent(proj.name || name) + t.retroDim(' ───'));
  console.log(t.retroDim('  Description: ') + t.retro(proj.description || 'N/A'));
  console.log(t.retroDim('  Version:     ') + t.retro(proj.version || 'latest'));
  console.log(t.retroDim('  Repo:        ') + t.retroAccent(proj.repo || 'N/A'));
  console.log(t.retroDim('  Download:    ') + t.retroDim(proj.download || 'N/A'));
  if (proj.info) {
    console.log();
    console.log(proj.info.trim().split('\n').map(l => `  ${l}`).join('\n'));
  }
  console.log();
}

async function doGet(name) {
  if (!name) {
    console.log(t.retroErr('  Specify a project: /get <project>'));
    return;
  }
  const lower = name.toLowerCase();
  if (lower === 'gcc' || lower === 'g++') {
    await runtime.installCompiler(lower);
    return;
  }
  let proj;
  try {
    proj = await fetch.fetchProject(lower);
  } catch (err) {
    console.log(t.retroErr(err.message));
    return;
  }
  await getCommand.install(lower, proj);
  installDeps(t.getDevDir(lower));
}

async function doFlash(name) {
  if (!name) {
    console.log(t.retroErr('  Specify a project: /flash <project>'));
    return;
  }
  let proj;
  try {
    proj = await fetch.fetchProject(name.toLowerCase());
  } catch (err) {
    console.log(t.retroErr(err.message));
    return;
  }
  console.log();
  console.log(t.retroWarn('  Re-downloading ') + t.retroAccent(proj.name || name) + t.retroWarn('...'));
  console.log();
  const dir = t.getDevDir(name.toLowerCase());
  if (fs.existsSync(dir)) {
    fs.rmSync(dir, { recursive: true, force: true });
    console.log(t.retroDim('  Removed old files.'));
  }
  await getCommand.install(name.toLowerCase(), proj);
  installDeps(t.getDevDir(name.toLowerCase()));
}

async function doRemove(name) {
  if (!name) {
    console.log(t.retroErr('  Specify a project: /rm <project>'));
    return;
  }
  try {
    await fetch.fetchProject(name.toLowerCase());
  } catch (err) {
    console.log(t.retroErr(err.message));
    return;
  }
  const dir = t.getDevDir(name.toLowerCase());
  if (fs.existsSync(dir)) {
    fs.rmSync(dir, { recursive: true, force: true });
    console.log(t.retro('  Removed ') + t.retroDim(dir));
  } else {
    console.log(t.retroDim('  No local files found for ') + t.retroAccent(name));
  }
}

async function doIssue(arg) {
  let name, message;
  const msgMatch = arg.match(/^(\S+)\s+-m\s+"(.+?)"/);
  if (msgMatch) {
    name = msgMatch[1].toLowerCase();
    message = msgMatch[2];
  } else {
    name = arg.trim().toLowerCase();
    message = '';
  }

  if (!name) {
    console.log(t.retroErr('  Usage: /issue <project> -m "your message"'));
    return;
  }

  let proj;
  try {
    proj = await fetch.fetchProject(name);
  } catch (err) {
    console.log(t.retroErr(err.message));
    return;
  }

  const repoPath = proj.repo || `emtypyie/${name}`;
  let url;
  if (message) {
    url = `https://github.com/${repoPath}/issues/new?title=${encodeURIComponent(message)}`;
  } else {
    url = `https://github.com/${repoPath}/issues`;
  }

  console.log(t.retro(`  Opening issue tracker for ${t.retroAccent(proj.name || name)}...`));
  if (openBrowser(url)) {
    console.log(t.retroDim('  Browser opened.'));
  } else {
    console.log(t.retroDim(`  Visit: ${url}`));
  }
}

function installDeps(dir) {
  const reqPath = path.join(dir, 'requirements.txt');
  if (!fs.existsSync(reqPath)) return;
  console.log(t.retroDim('  Installing Python dependencies...'));
  try {
    const launcher = process.platform === 'win32' ? 'py -3' : 'python3';
    execSync(`${launcher} -m pip install -r "${reqPath}" -q`, { stdio: 'pipe', timeout: 120000 });
    console.log(t.retro('  Dependencies installed.'));
  } catch {
    console.log(t.retroWarn('  pip install had issues (see above).'));
  }
}

function launch(path) {
  const isBat = /\.bat$|\.cmd$/i.test(path);
  if (isBat) {
    spawn(process.env.COMSPEC || 'cmd.exe', ['/c', path], { detached: true, stdio: 'ignore' }).unref();
  } else {
    spawn(path, [], { detached: true, stdio: 'ignore' }).unref();
  }
}

function isScript(path) {
  return /\.(bat|cmd|py|sh)$/i.test(path);
}

function runSync(path) {
  const isBat = /\.bat$|\.cmd$/i.test(path);
  if (isBat) {
    const cmd = `${process.env.COMSPEC || 'cmd.exe'} /c "${path}"`;
    execSync(cmd, { stdio: 'inherit', timeout: 0 });
  } else {
    execSync(`"${path}"`, { stdio: 'inherit', timeout: 0 });
  }
}

async function runProject(name) {
  let proj;
  try {
    proj = await fetch.fetchProject(name);
  } catch (err) {
    console.log(t.retroErr(err.message));
    return;
  }
  const runTarget = proj.run || proj.filename;
  if (!runTarget) {
    console.log(t.retroErr(`  Project "${name}" has no run target.`));
    return;
  }
  const runPath = path.resolve(t.getDevDir(name), runTarget);
  if (!fs.existsSync(runPath)) {
    console.log(t.retroErr(`  "${name}" not downloaded yet. Run /get ${name} first.`));
    return;
  }
  installDeps(t.getDevDir(name));

  if (isScript(runTarget)) {
    console.log(t.retro(`  Running ${t.retroAccent(proj.name || name)}...`));
    try { runSync(runPath); } catch (e) {
      console.log(t.retroErr(`  Project exited with error: ${e.message}`));
    }
  } else {
    console.log(t.retro(`  Launching ${t.retroAccent(proj.name || name)}...`));
    launch(runPath);
  }
}

function doUpdate() {
  console.log(t.retro('  Updating emtypyie...'));
  try {
    execSync('npm update -g emtypyie-cli', { stdio: 'inherit' });
    console.log(t.retro('  Update complete!'));
  } catch {
    console.log(t.retroErr('  Update failed. Try: npm update -g emtypyie-cli'));
  }
}

async function doList() {
  console.log();
  console.log(t.retroDim('  ─── Projects ───'));
  console.log();
  try {
    const list = await fetch.fetchProjectList();
    const projList = Array.isArray(list) ? list : (list.projects || []);
    for (const entry of projList) {
      const name = typeof entry === 'string' ? entry : entry.name;
      const ver = entry.version || '?';
      const desc = entry.description || '';
      const pad = ' '.repeat(Math.max(0, 12 - name.length));
      console.log(`  ${t.retroAccent(name)}${pad}${t.retroDim(ver)}  ${t.retro(desc)}`);
    }
  } catch {
    console.log(t.retroDim('  (Could not fetch project list)'));
  }
  console.log();
}

async function doDocs(arg) {
  if (!arg) {
    console.log(t.retroErr('  Specify a project: /docs <project>'));
    return;
  }
  let proj;
  try {
    proj = await fetch.fetchProject(arg.toLowerCase());
  } catch (err) {
    console.log(t.retroErr(err.message));
    return;
  }
  const repo = proj.repo || `emtypyie/${arg}`;
  const url = `https://github.com/${repo}#readme`;
  console.log(t.retro(`  Opening docs for ${t.retroAccent(proj.name || arg)}...`));
  if (openBrowser(url)) {
    console.log(t.retroDim('  Browser opened.'));
  } else {
    console.log(t.retroDim(`  Visit: ${url}`));
  }
}

function doChangelog() {
  const url = 'https://github.com/emtypyie/emtypyie-cli/releases';
  console.log(t.retro('  Opening changelog...'));
  if (openBrowser(url)) {
    console.log(t.retroDim('  Browser opened.'));
  } else {
    console.log(t.retroDim(`  Visit: ${url}`));
  }
}

function doTheme(arg) {
  const parts = arg.trim().split(/\s+/);
  if (!arg) {
    console.log(t.retroDim('  Usage: ') + t.retro('/theme <name>'));
    console.log(t.retroDim('  Usage: ') + t.retro('/theme bakafetch <color>'));
    console.log(t.retroDim('  Themes: ') + t.retro(Object.keys(t.THEMES).join(', ')));
    console.log(t.retroDim('  Bakafetch colors: ') + t.retro(Object.keys(bakafetch.COLORS).join(', ')) + t.retroDim(' or hex'));
    return;
  }
  if (parts[0].toLowerCase() === 'bakafetch') {
    const color = parts.slice(1).join(' ');
    if (bakafetch.setColor(color)) {
      console.log(t.retro('  Bakafetch color set to ') + chalk.hex(bakafetch.getColor())(color));
    } else {
      console.log(t.retroErr('  Invalid color. Use: ') + t.retro(Object.keys(bakafetch.COLORS).join(', ')) + t.retroErr(' or hex like #ff8800'));
    }
    return;
  }
  const theme = arg.trim().toLowerCase();
  if (t.THEMES[theme]) {
    t.apply(theme);
    bakafetch.setColor(theme);
    console.log(t.retro('  Theme set to ') + t.retroAccent(theme));
  } else {
    console.log(t.retroErr('  Invalid theme. Use: ') + t.retro(Object.keys(t.THEMES).join(', ')));
  }
}

const COMMANDS = ['help', 'about', 'wiki', 'setenv', 'theme', 'info', 'get', 'flash', 'rm', 'issue', 'bakafetch', 'bf', 'wrap', 'clear', 'update', 'list', 'docs', 'changelog', 'exit', 'quit'];

function completer(line) {
  const input = line.replace(/^\//, '');
  const parts = input.split(/\s+/);
  const hits = COMMANDS.filter(c => c.startsWith(parts[0])).map(c => '/' + c + ' ');
  return [hits.length ? hits : [], line];
}

async function interactive() {
  process.title = 'emtypyie cli';
  const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout,
    prompt: t.retro('>> '),
    completer
  });

  console.clear();
  await playStartupAnimation({ skip: args.includes('--no-animation') });
  console.log(BANNER);
  console.log(t.retro('  EMTYPYIE CLI v2.5.4'));
  console.log(t.retroDim('  "Wandering Witches"'));
  console.log();
  console.log(t.retroDim(`
  ─────────────────────────────────────────────
  `) + t.retro('"code. create. conquer."') + t.retroDim(`
  ─────────────────────────────────────────────

  Type `) + t.retroAccent('/help') + t.retroDim(` for available commands.
  ─────────────────────────────────────────────`));
  console.log();

  rl.prompt();

  rl.on('line', async (line) => {
    const input = line.trim();
    const parts = input.split(/\s+/);
    const cmd = parts[0]?.toLowerCase() || '';
    const arg = parts.slice(1).join(' ');

    if (!cmd) {
      rl.prompt();
      return;
    }

    try {
      await handleCommand(cmd, arg, rl);
    } catch (e) {
      console.log(t.retroErr(`  Error: ${e.message}`));
    }
    rl.prompt();
  });

  rl.on('close', () => {
    console.log(t.retroDim('\n  System halted.'));
    process.exit(0);
  });
}

async function direct(args) {
  process.title = 'emtypyie cli';
  const cmd = args[0]?.toLowerCase();
  const arg = args.slice(1).join(' ');

  if (!cmd || cmd === 'help' || cmd === '/help') {
    await showHelp();
    return;
  }

  try {
    await handleCommand(cmd, arg, null);
  } catch (e) {
    console.log(t.retroErr(`  Error: ${e.message}`));
  }
}

const args = process.argv.slice(2);

if (args.length === 0) {
  interactive();
} else {
  direct(args);
}
