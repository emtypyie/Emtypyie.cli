const fs = require('fs');
const path = require('path');
const os = require('os');
const https = require('https');
const { execSync } = require('child_process');
const t = require('./theme');

const RUNTIMES_DIR = path.join(os.homedir(), '.emtypyie', 'runtimes');
const MINGW_DIR = path.join(RUNTIMES_DIR, 'mingw64');
const MINGW_BIN = path.join(MINGW_DIR, 'bin');

const W64DEVKIT_URL = 'https://github.com/skeeto/w64devkit/releases/download/v2.8.0/w64devkit-x64-2.8.0.7z.exe';
const SEVENZR_URL = 'https://www.7-zip.org/a/7zr.exe';

const PYTHON_VERSION = '3.12.8';
const PYTHON_DIR = path.join(RUNTIMES_DIR, 'python');
const PYTHON_EXE = os.platform() === 'win32'
  ? path.join(PYTHON_DIR, 'python.exe')
  : path.join(PYTHON_DIR, 'bin', 'python3');
const PYTHON_SCRIPTS = os.platform() === 'win32'
  ? path.join(PYTHON_DIR, 'Scripts')
  : path.join(PYTHON_DIR, 'bin');
const PYTHON_EMBED_URL = `https://www.python.org/ftp/python/${PYTHON_VERSION}/python-${PYTHON_VERSION}-embed-amd64.zip`;
const GET_PIP_URL = 'https://bootstrap.pypa.io/get-pip.py';
const PYTHON_MARKER = path.join(PYTHON_DIR, '.emtypyie-python-version');

function isInPath(tool) {
  try {
    execSync(`where ${tool}`, { stdio: 'ignore', timeout: 3000 });
    return true;
  } catch {
    return false;
  }
}

function find7z() {
  try {
    execSync('7z --help', { stdio: 'ignore', timeout: 3000 });
    return '7z';
  } catch {}
  const paths = [
    'C:\\Program Files\\7-Zip\\7z.exe',
    'C:\\Program Files (x86)\\7-Zip\\7z.exe',
  ];
  for (const p of paths) {
    if (fs.existsSync(p)) return p;
  }
  return null;
}

function downloadFile(url, dest, quiet = false) {
  return new Promise((resolve, reject) => {
    const file = fs.createWriteStream(dest);
    https.get(url, (res) => {
      if (res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
        file.close();
        fs.unlinkSync(dest);
        return downloadFile(res.headers.location, dest, quiet).then(resolve).catch(reject);
      }
      if (res.statusCode !== 200) {
        file.close();
        fs.unlinkSync(dest);
        reject(new Error(`HTTP ${res.statusCode}`));
        return;
      }
      const total = parseInt(res.headers['content-length'], 10);
      let downloaded = 0;
      res.on('data', (chunk) => {
        downloaded += chunk.length;
        if (total && !quiet) {
          const pct = ((downloaded / total) * 100).toFixed(1);
          process.stdout.write(`\r  ${t.retroDim('\u25bc')} ${t.retro(pct + '%')}`);
        }
      });
      res.pipe(file);
      file.on('finish', () => {
        if (!quiet) process.stdout.write('\r' + ' '.repeat(50) + '\r');
        file.close();
        resolve(dest);
      });
    }).on('error', (err) => {
      file.close();
      if (fs.existsSync(dest)) fs.unlinkSync(dest);
      reject(err);
    });
  });
}

async function installCompiler(tool) {
  console.log();

  if (isInPath('gcc') && isInPath('g++')) {
    const ver = execSync('gcc --version', { encoding: 'utf8' }).split('\n')[0].trim();
    console.log(`  ${t.retro('\u2713')} ${t.retroDim('Already installed:')} ${t.retroAccent(ver)}`);
    console.log();
    return;
  }

  if (os.platform() !== 'win32') {
    console.log(`  ${t.retroErr('\u2717')} ${t.retro('Auto-install is Windows only.')}`);
    if (os.platform() === 'linux') {
      console.log(`  ${t.retroDim('  Run:')} ${t.retroAccent('sudo apt install build-essential')}`);
    } else if (os.platform() === 'darwin') {
      console.log(`  ${t.retroDim('  Run:')} ${t.retroAccent('xcode-select --install')}`);
    }
    console.log();
    return;
  }

  const tempDir = fs.mkdtempSync(path.join(os.tmpdir(), 'emty-'));

  try {
    let sevenz = find7z();

    if (!sevenz) {
      console.log(`  ${t.retro('\u25bc')} ${t.retroDim('Downloading 7-Zip portable...')}`);
      const sevenzPath = path.join(tempDir, '7zr.exe');
      await downloadFile(SEVENZR_URL, sevenzPath);
      sevenz = sevenzPath;
    }

    console.log(`  ${t.retro('\u25bc')} ${t.retroDim('Downloading w64devkit (GCC + G++)...')}`);
    const archivePath = path.join(tempDir, 'w64devkit.7z.exe');
    await downloadFile(W64DEVKIT_URL, archivePath);

    console.log(`  ${t.retro('\u25bc')} ${t.retroDim('Extracting...')}`);
    const extractTemp = path.join(tempDir, 'extracted');
    fs.mkdirSync(extractTemp, { recursive: true });
    execSync(`"${sevenz}" x "${archivePath}" -o"${extractTemp}" -y`, { stdio: 'ignore', timeout: 120000 });

    if (!fs.existsSync(MINGW_DIR)) fs.mkdirSync(MINGW_DIR, { recursive: true });

    const entries = fs.readdirSync(extractTemp);
    const topDir = entries.find(e => fs.statSync(path.join(extractTemp, e)).isDirectory());
    const srcDir = topDir ? path.join(extractTemp, topDir) : extractTemp;

    for (const item of fs.readdirSync(srcDir)) {
      const src = path.join(srcDir, item);
      const dst = path.join(MINGW_DIR, item);
      fs.renameSync(src, dst);
    }

    process.env.Path = MINGW_BIN + ';' + (process.env.Path || '');

    const currentUserPath = execSync(
      '[Environment]::GetEnvironmentVariable("Path","User")',
      { encoding: 'utf8', shell: 'powershell', timeout: 10000 }
    ).trim();
    if (!currentUserPath.includes(MINGW_BIN)) {
      console.log(`  ${t.retro('\u25bc')} ${t.retroDim('Adding to system PATH...')}`);
      execSync(
        `[Environment]::SetEnvironmentVariable('Path', '${MINGW_BIN};' + [Environment]::GetEnvironmentVariable('Path','User'), 'User')`,
        { shell: 'powershell', timeout: 10000 }
      );
    }

    const ver = execSync(`"${path.join(MINGW_BIN, 'gcc.exe')}" --version`, { encoding: 'utf8' }).split('\n')[0].trim();
    console.log(`  ${t.retro('\u2713')} ${t.retroDim('Installed')} ${t.retroAccent(ver)}`);
    console.log(`  ${t.retroDim('  Location:')} ${t.retro(MINGW_DIR)}`);
    console.log(`  ${t.retroDim('  Restart terminal or run:')} ${t.retroAccent('refreshenv')}`);
  } catch (err) {
    console.log(`  ${t.retroErr('\u2717')} ${t.retroDim('Installation failed:')} ${err.message}`);
  } finally {
    if (fs.existsSync(tempDir)) {
      fs.rmSync(tempDir, { recursive: true, force: true });
    }
  }
  console.log();
}

function isPythonInstalled() {
  return fs.existsSync(PYTHON_EXE);
}

function applyPythonPath() {
  if (!isPythonInstalled()) return false;
  const key = Object.keys(process.env).find(k => k.toUpperCase() === 'PATH') || 'Path';
  const current = process.env[key] || '';
  const present = current.toLowerCase().split(path.delimiter);
  const missing = [PYTHON_DIR, PYTHON_SCRIPTS].filter(d => !present.includes(d.toLowerCase()));
  if (missing.length) {
    process.env[key] = missing.join(path.delimiter) + (current ? path.delimiter + current : '');
  }
  return true;
}

function getPythonExecutable() {
  return isPythonInstalled()
    ? PYTHON_EXE
    : (os.platform() === 'win32' ? 'python' : 'python3');
}

function hasPip() {
  return fs.existsSync(path.join(PYTHON_SCRIPTS, os.platform() === 'win32' ? 'pip.exe' : 'pip'));
}

function enableSitePackages() {
  const pth = path.join(PYTHON_DIR, 'python312._pth');
  if (!fs.existsSync(pth)) return;
  let text = fs.readFileSync(pth, 'utf8');
  if (/(^|\r?\n)#\s*import site\s*(\r?\n|$)/.test(text)) {
    text = text.replace(/(^|\r?\n)#\s*import site\s*(\r?\n|$)/, '$1import site$2');
    fs.writeFileSync(pth, text);
  } else if (!/(^|\r?\n)\s*import site\s*(\r?\n|$)/.test(text)) {
    fs.writeFileSync(pth, text + (text.endsWith('\n') ? '' : '\n') + 'import site\n');
  }
}

function extractZip(zipPath, destDir) {
  fs.mkdirSync(destDir, { recursive: true });
  const esc = s => s.replace(/'/g, "''");
  execSync(
    `powershell -NoProfile -Command "Expand-Archive -LiteralPath '${esc(zipPath)}' -DestinationPath '${esc(destDir)}' -Force"`,
    { stdio: 'ignore', timeout: 180000 }
  );
}

async function bootstrapPip(tempDir, quiet) {
  if (hasPip()) return true;
  const getPipPath = path.join(tempDir, 'get-pip.py');
  await downloadFile(GET_PIP_URL, getPipPath, quiet);
  execSync(`"${PYTHON_EXE}" "${getPipPath}" --no-warn-script-location --disable-pip-version-check`, {
    stdio: quiet ? 'ignore' : 'inherit',
    timeout: 180000
  });
  return hasPip();
}

async function installPythonWindows(quiet) {
  const tempDir = fs.mkdtempSync(path.join(os.tmpdir(), 'emty-py-'));
  try {
    if (!quiet) {
      console.log(`  ${t.retroDim('Downloading Python')} ${t.retroAccent(PYTHON_VERSION)} ${t.retroDim('(local build)')}...`);
    }
    const zipPath = path.join(tempDir, `python-${PYTHON_VERSION}-embed-amd64.zip`);
    await downloadFile(PYTHON_EMBED_URL, zipPath, quiet);

    if (!quiet) console.log(`  ${t.retroDim('Extracting to')} ${t.retro(PYTHON_DIR)}...`);
    if (fs.existsSync(PYTHON_DIR)) fs.rmSync(PYTHON_DIR, { recursive: true, force: true });
    extractZip(zipPath, PYTHON_DIR);
    enableSitePackages();

    try {
      await bootstrapPip(tempDir, quiet);
    } catch (err) {
      if (!quiet) console.log(`  ${t.retroWarn('pip bootstrap skipped:')} ${t.retroDim(err.message)}`);
    }

    fs.writeFileSync(PYTHON_MARKER, PYTHON_VERSION + '\n');
    return isPythonInstalled();
  } finally {
    if (fs.existsSync(tempDir)) fs.rmSync(tempDir, { recursive: true, force: true });
  }
}

function installPythonPosix(quiet) {
  fs.mkdirSync(RUNTIMES_DIR, { recursive: true });
  if (fs.existsSync(PYTHON_DIR)) fs.rmSync(PYTHON_DIR, { recursive: true, force: true });
  execSync(`python3 -m venv "${PYTHON_DIR}"`, {
    stdio: quiet ? 'ignore' : 'inherit',
    timeout: 180000
  });
  return isPythonInstalled();
}

async function ensurePython(options = {}) {
  const quiet = !!options.quiet;

  if (isPythonInstalled()) {
    applyPythonPath();
    if (os.platform() === 'win32' && !hasPip()) {
      const tempDir = fs.mkdtempSync(path.join(os.tmpdir(), 'emty-py-'));
      try {
        await bootstrapPip(tempDir, true);
      } catch {}
      finally {
        if (fs.existsSync(tempDir)) fs.rmSync(tempDir, { recursive: true, force: true });
      }
    }
    return true;
  }

  if (!quiet) {
    console.log();
    console.log(`  ${t.retro('\u25bc')} ${t.retroDim('Setting up local Python')} ${t.retroAccent(PYTHON_VERSION)} ${t.retroDim('(isolated, no system changes)')}`);
  }

  try {
    if (os.platform() === 'win32') await installPythonWindows(quiet);
    else installPythonPosix(quiet);
    if (!isPythonInstalled()) throw new Error('python executable not found after install');
    applyPythonPath();
    if (!quiet) {
      console.log(`  ${t.retro('\u2713')} ${t.retroDim('Python ready:')} ${t.retroAccent(PYTHON_DIR)}`);
      console.log();
    }
    return true;
  } catch (err) {
    if (!quiet) {
      console.log(`  ${t.retroWarn('\u26a0')} ${t.retroDim('Local Python setup skipped:')} ${err.message}`);
      console.log(`  ${t.retroDim('  The CLI will retry on the next install or update.')}`);
      console.log();
    }
    return false;
  }
}

module.exports = {
  installCompiler,
  ensurePython,
  applyPythonPath,
  getPythonExecutable,
  isPythonInstalled,
  PYTHON_VERSION,
  PYTHON_DIR,
  PYTHON_EXE,
};