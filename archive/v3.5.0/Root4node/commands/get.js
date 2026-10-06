const os = require('os');
const https = require('https');
const http = require('http');
const fs = require('fs');
const path = require('path');
const { execSync, spawnSync } = require('child_process');

const t = require('./theme');
const verify = require('./verify');
const fetchMeta = require('./fetch');
const installtxt = require('./installtxt');
const wizard = require('./wizard');

const NOT_FOUND_MSG = 'installation scripts not found. if this an error please create a issue at emtypyie.in/issue';

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
      const start = Date.now();
      const width = 34;

      response.on('data', (chunk) => {
        downloaded += chunk.length;
        const elapsed = (Date.now() - start) / 1000;
        const speed = elapsed > 0 ? downloaded / elapsed : 0;
        const speedTxt = speed > 1024 * 1024
          ? (speed / (1024 * 1024)).toFixed(1) + 'mb/s'
          : Math.max(1, Math.round(speed / 1024)) + 'kb/s';
        let etaTxt = '';
        if (total && speed > 0) {
          const eta = Math.max(0, (total - downloaded) / speed);
          etaTxt = eta.toFixed(1) + 'sec';
        }
        const pct = total ? Math.min(100, Math.floor((downloaded / total) * 100)) : Math.min(95, Math.floor(downloaded / 1024 / 256));
        const filled = Math.round(width * pct / 100);
        process.stdout.write(`\r  ${'-'.repeat(filled)}> ${pct}%  ${speedTxt}${etaTxt ? '  ' + etaTxt : ''}   `);
      });

      response.pipe(file);

      file.on('finish', () => {
        const elapsed = ((Date.now() - start) / 1000).toFixed(1);
        const speed = downloaded / Math.max(0.1, (Date.now() - start) / 1000);
        const speedTxt = speed > 1024 * 1024
          ? (speed / (1024 * 1024)).toFixed(1) + 'mb/s'
          : Math.max(1, Math.round(speed / 1024)) + 'kb/s';
        process.stdout.write(`\r  ${'-'.repeat(width)}> 100%  ${speedTxt}  ${elapsed}sec   \n`);
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

async function download(url, dest) {
  try {
    return await downloadHttps(url, dest);
  } catch (err) {
    if (fs.existsSync(dest)) fs.unlinkSync(dest);
    throw err;
  }
}

function runCommand(cmd, cwd) {
  console.log(t.retroDim(`  $ ${cmd}`));
  const res = spawnSync(cmd, { shell: true, stdio: 'inherit', cwd, timeout: 0 });
  if (res.error) throw res.error;
  if (res.status !== 0) throw new Error(`exit code ${res.status}`);
}

async function install(name, project, rl) {
  if (!project) {
    project = await installtxt.animatedStep('Fetching Metadata', async () => {
      return fetchMeta.fetchProject(name);
    });
  } else {
    await installtxt.animatedStep('Fetching Metadata', async () => project);
  }

  let script = null;
  try {
    script = await installtxt.animatedStep('Interpreting Installation Script', async () => {
      return installtxt.load(name);
    });
  } catch {}

  if (!script) {
    console.log(t.retroErr(`  ${NOT_FOUND_MSG}`));
    return;
  }

  console.log(t.retroDim(`  ${script.name} v${script.version}`));

  wizard.begin(rl);
  const license = await installtxt.loadLicense(name);
  console.log();
  if (license) {
    console.log(license.trim().split('\n').map(l => t.retroDim('  ' + l)).join('\n'));
    console.log();
  } else {
    console.log(t.retroWarn('  (No license file bundled with this project.)'));
    console.log();
  }

  const wantsInstall = await wizard.confirmYN(t.retro('Do you want to install? [Y/yes/N/No] >>'));
  if (!wantsInstall) {
    console.log(t.retroErr('  Installation aborted.'));
    wizard.end(rl);
    return;
  }

  const target = await wizard.askChoice(t.retro('Install location? [L]ocal / [G]lobal >>'), { l: 'local', g: 'global' });
  let devDir = t.getDevDir(name);
  if (target === 'l') {
    const dir = await wizard.askLine(t.retro(`Install directory [${devDir}]:`), devDir);
    devDir = dir;
    if (!fs.existsSync(devDir)) fs.mkdirSync(devDir, { recursive: true });
  }

  console.log();
  console.log(t.retroDim('  ┃ Summary ┃'));
  console.log(t.retroDim('  Project : ') + t.retroAccent(script.name || name));
  console.log(t.retroDim('  Version : ') + t.retro(script.version || (project && project.version) || 'latest'));
  console.log(t.retroDim('  Target  : ') + t.retro(target === 'l' ? 'local' : 'global'));
  console.log(t.retroDim('  Path    : ') + t.retro(devDir));
  console.log();

  const confirmed = await wizard.pressEnterOrEsc();
  wizard.end(rl);
  if (!confirmed) {
    console.log(t.retroErr('  Installation aborted.'));
    return;
  }

  if (project && project.download) {
    const dest = path.join(devDir, project.filename || `${name}-setup.exe`);
    process.stdout.write('>> Downloading files\n');
    try {
      await download(project.download, dest);
    } catch (err) {
      console.log(t.retroErr(`  Download failed: ${err.message}`));
      return;
    }
  } else {
    await installtxt.animatedStep('Downloading files', async () => null, { note: 'no remote package (skipped)' });
  }

  const installEntry = path.join(devDir, '.emtypyie', 'installation.txt');
  if (!fs.existsSync(installEntry)) {
    const alt = path.join(devDir, 'installation.txt');
    if (fs.existsSync(alt)) {
      script = installtxt.parse(fs.readFileSync(alt, 'utf8'));
    }
  } else {
    script = installtxt.parse(fs.readFileSync(installEntry, 'utf8'));
  }

  await installtxt.animatedStep('Running Build Scripts', async () => {
    for (const cmd of script.deps) runCommand(cmd, devDir);
    for (const cmd of script.build) runCommand(cmd, devDir);
    for (const cmd of script.install) runCommand(cmd, devDir);
  });

  if (script.start.length) {
    process.stdout.write('>> Starting\n');
    let pct = 0;
    const width = 34;
    while (pct < 100) {
      pct = Math.min(100, pct + 10);
      const filled = Math.round(width * pct / 100);
      process.stdout.write(`\r  ${'.'.repeat(filled)}> ${pct}%   `);
      await new Promise(r => setTimeout(r, 90));
    }
    process.stdout.write('\n');
    for (const cmd of script.start) {
      try {
        runCommand(cmd, devDir);
      } catch (err) {
        console.log(t.retroWarn(`  Start command failed: ${err.message}`));
      }
    }
  }

  if (project) {
    await verify.generateManifest(name, project);
  }

  console.log();
  console.log(t.retro('  ✔ Done.'));
  console.log();
}

module.exports = { install, download };
