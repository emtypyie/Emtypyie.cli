# Pyielink v0.5.0

> Remote access framework — screen sharing, file transfer, and terminal access

**Documentation:** https://wiki.emtypyie.in/docs/pyielink
**Main site:** https://emtypyie.in/cli
**Issues:** https://emtypyie.in/issues/pyielink

## Install

```bash
# Via emtypyie.cli
emtypyie /get pyielink

# Or npm (downloads Rust binary automatically)
npm install -g pyielink
```

## Usage

```bash
# Connect to host (GUI mode)
pyielink user@ip

# REPL terminal mode
pyielink --repl user@ip

# Enable host for connections
pyielink enable
pyielink enable --all              # Open to any IP
pyielink enable --whitelist IP     # Allow specific IP only

# Whitelist management
pyielink whitelist add IP
pyielink whitelist remove IP

# Setup (run after install)
pyielink --setup

# Version
pyielink --version
```

## Features

- **Screen sharing** — Remote desktop with low latency
- **File transfer** — Bidirectional file transfer
- **Terminal access** — REPL mode for remote command execution
- **Whitelist/ACL** — IP-based access control
- **Cross-platform** — Windows, Linux, macOS

## Requirements

- **Node.js** 18+ (for npm package)
- **Rust** 1.70+ (binary downloaded automatically)
- System: git, curl, openssl

## Links

- **Documentation:** https://wiki.emtypyie.in/docs/pyielink
- **Main site:** https://emtypyie.in/cli
- **Issues:** https://emtypyie.in/issues/pyielink
- **GitHub:** https://github.com/emtypyie/Pyielink
- **npm:** https://www.npmjs.com/package/pyielink

## License

Proprietary — Copyright (c) 2026 EMTYPYIE. All Rights Reserved.