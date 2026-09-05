const os = require('os');
const https = require('https');
const http = require('http');
const fs = require('fs');
const path = require('path');
const { execSync, spawn } = require('child_process');

const t = require('./theme');
const verify = require('./verify');

function downloadHttps(url, dest) {
  return new Promise((resolve, reject) => {
    const file = fs.createWriteStream(dest);
    const protocol = url.startsWith('https') ? https : http;

    protocol.get(url, (response) => {
      if (response.statusCode >= 300 && response.statusCode < 400 && response.headers.location) {
        file.close();
        fs.unlinkSync(dest);
        return downloadHttps(response.headers.location, dest).then(resolve).catch(reject);
      }

      if (response.statusCode !== 200) {
        file.close();
        fs.unlinkSync(dest);
        reject(new Error(`HTTP ${response.statusCode}`));
        return;
      }

      const total = parseInt(response.headers['content-length'], 10);
      let downloaded = 0;

      response.on('data', (chunk) => {
        downloaded += chunk.length;
        if (total) {
          const pct = ((downloaded / total) * 100).toFixed(1);
          process.stdout.write(`\r  ${t.retroDim('\u25bc')} ${t.retro(pct + '%')}`);
        }
      });

      response.pipe(file);

      file.on('finish', () => {
        process.stdout.write('\r' + ' '.repeat(50) + '\r');
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

function downloadCurl(url, dest) {
  return new Promise((resolve, reject) => {
    const proc = spawn('curl', ['-L', '--progress-bar', '-o', dest, url], { stdio: 'inherit', timeout: 300000 });
    proc.on('close', (code) => {
      if (code === 0) {
        resolve(dest);
      } else {
        if (fs.existsSync(dest)) fs.unlinkSync(dest);
        reject(new Error(`curl exited with code ${code}`));
      }
    });
    proc.on('error', (err) => {
      if (fs.existsSync(dest)) fs.unlinkSync(dest);
      reject(err);
    });
  });
}

function isCurlAvailable() {
  try {
    execSync('curl --version', { stdio: 'ignore', timeout: 3000 });
    return true;
  } catch {
    return false;
  }
}

async function download(url, dest) {
  console.log(`  ${t.retroDim('\u25bc')} ${t.retro('Downloading...')}`);
  if (isCurlAvailable()) {
    return downloadCurl(url, dest);
  }
  return downloadHttps(url, dest);
}

async function install(name, project) {
  console.log();
  console.log(t.retroDim('  \u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501'));
  console.log(t.retro(`  Installing ${t.retroAccent(project.name || name)}`));
  console.log(t.retroDim('  \u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501'));
  console.log();

  const version = project.version || 'latest';
  console.log(t.retroDim(`  Version: ${t.retro(version)}`));
  console.log(t.retroDim(`  Repo: ${t.retro(project.repo || 'N/A')}`));
  console.log();

  if (project.install) {
    await project.install(name, download);
    return;
  }

  // Prefer package manager installation (npm, pip, cargo) over direct download
  // as they handle cross-platform binary downloads automatically
  const hasPackageManager = project.packageManagers && (
    project.packageManagers.npm ||
    project.packageManagers.pypi ||
    project.packageManagers.cargo
  );

  if (hasPackageManager && project.download) {
    // Both available - prefer package manager for cross-platform support
    console.log(t.retroDim('  Using package manager for cross-platform install...'));
  }

  if (project.download && !hasPackageManager) {
    const dest = path.join(t.getDevDir(name), project.filename || `${name}-setup.exe`);

    try {
      await download(project.download, dest);
      const rel = dest.replace(os.homedir(), '~');
      console.log(`  ${t.retro('\u2713')} ${t.retroDim('Saved to')} ${t.retroAccent(rel)}`);

      if (project.postInstall) {
        project.postInstall(dest);
      }
    } catch (err) {
      console.log(`  ${t.retroErr('\u2717')} Download failed: ${err.message}`);
      process.exit(1);
    }
  } else if (project.packageManagers && project.packageManagers.npm) {
    // Install via npm
    const npmInfo = project.packageManagers.npm;
    console.log(t.retroDim(`  Installing via npm: ${npmInfo.installCmd}`));
    try {
      execSync(npmInfo.installCmd, { stdio: 'inherit', timeout: 300000 });
      console.log(t.retro('  npm package installed successfully.'));
    } catch (err) {
      console.log(`  ${t.retroErr('\u2717')} npm install failed: ${err.message}`);
      process.exit(1);
    }
  } else if (project.packageManagers && project.packageManagers.pypi) {
    // Install via pip
    const pypiInfo = project.packageManagers.pypi;
    console.log(t.retroDim(`  Installing via pip: ${pypiInfo.installCmd}`));
    try {
      execSync(pypiInfo.installCmd, { stdio: 'inherit', timeout: 300000 });
      console.log(t.retro('  pip package installed successfully.'));
    } catch (err) {
      console.log(`  ${t.retroErr('\u2717')} pip install failed: ${err.message}`);
      process.exit(1);
    }
  } else if (project.packageManagers && project.packageManagers.cargo) {
    // Install via cargo
    const cargoInfo = project.packageManagers.cargo;
    console.log(t.retroDim(`  Installing via cargo: ${cargoInfo.installCmd}`));
    try {
      execSync(cargoInfo.installCmd, { stdio: 'inherit', timeout: 300000 });
      console.log(t.retro('  cargo package installed successfully.'));
    } catch (err) {
      console.log(`  ${t.retroErr('\u2717')} cargo install failed: ${err.message}`);
      process.exit(1);
    }
  }

  // Run postInstall script if defined
  if (project.installScripts && project.installScripts.postInstall) {
    console.log(t.retroDim(`  Running post-install: ${project.installScripts.postInstall}`));
    try {
      execSync(project.installScripts.postInstall, { stdio: 'inherit', timeout: 120000 });
      console.log(t.retro('  Post-install completed.'));
    } catch (err) {
      console.log(t.retroWarn(`  Post-install had issues: ${err.message}`));
    }
  }

  if (project.info) {
    console.log();
    console.log(t.retroDim('  \u2503 Info \u2503'));
    console.log(project.info.trim().split('\n').map(l => `  ${l}`).join('\n'));
  }

  // Generate integrity manifest
  await verify.generateManifest(name, project);

  console.log();
  console.log(t.retro('  \u2714 Done.'));
  console.log();
}

module.exports = { install, download };