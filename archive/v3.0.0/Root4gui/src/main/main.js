'use strict';

const { app, BrowserWindow, Menu, ipcMain } = require('electron');
const path = require('path');
const fs = require('fs');
const os = require('os');
const https = require('https');
const { Engine, enginePath, readEnvJson, writeEnvJson, envJsonPath, envDir } = require('../engine/engine');

// Handle Squirrel install/uninstall events on Windows (no-op if not present).
try {
  if (require('electron-squirrel-startup')) app.quit();
} catch (_) {}

let mainWindow = null;
const tabs = new Map(); // tabId -> Engine

function createWindow() {
  Menu.setApplicationMenu(null);
  mainWindow = new BrowserWindow({
    width: 1100,
    height: 760,
    minWidth: 860,
    minHeight: 600,
    frame: false,
    backgroundColor: '#0c0712',
    title: 'Emtypyie — Baking Bread',
    icon: path.join(__dirname, '..', 'renderer', 'logo.ico'),
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false
    }
  });

  mainWindow.loadFile(path.join(__dirname, '..', 'renderer', 'index.html'));
  mainWindow.on('closed', () => { mainWindow = null; });
}

app.whenReady().then(() => {
  createWindow();

  // ─── Desktop shortcut (first launch only) ───
  if (process.platform === 'win32') {
    const flagFile = path.join(envDir(), '.shortcut_created');
    if (!fs.existsSync(flagFile)) {
      try {
        if (!fs.existsSync(envDir())) fs.mkdirSync(envDir(), { recursive: true });
        const desktop = path.join(os.homedir(), 'Desktop');
        const shortcut = path.join(desktop, 'emtypyie.cli-Wrapper.lnk');
        const target = process.execPath;
        const icon = path.join(__dirname, '..', 'renderer', 'logo.ico');
        if (!fs.existsSync(shortcut)) {
          const { exec } = require('child_process');
          exec(
            `powershell -NoProfile -Command "$ws=New-Object -ComObject WScript.Shell;$s=$ws.CreateShortcut('${shortcut.replace(/'/g,"''")}');$s.TargetPath='${target.replace(/'/g,"''")}';$s.IconLocation='${icon.replace(/'/g,"''")}';$s.Save()"`,
            () => {}
          );
        }
        fs.writeFileSync(flagFile, '1');
      } catch (_) {}
    }
  }

  // ─── Custom title bar controls ───
  ipcMain.on('win:min', () => mainWindow?.minimize());
  ipcMain.on('win:max', () => { if (mainWindow?.isMaximized()) mainWindow.unmaximize(); else mainWindow?.maximize(); });
  ipcMain.on('win:close', () => mainWindow?.close());

  // ─── IPC bridge: buttons 1:1 to C CLI commands ───

  // One-shot structured command (no live tab process needed).
  ipcMain.handle('engine:exec', async (_e, { cmd, arg, opts }) => {
    const eng = new Engine({ label: 'exec' });
    return eng.exec(cmd, arg, opts || {});
  });

  // Open a persistent project tab -> spawn a dedicated C engine.
  ipcMain.handle('tab:open', async (_e, { tabId, project }) => {
    if (tabs.has(tabId)) return { ok: true, msg: 'already open' };
    const eng = new Engine({
      label: tabId,
      project,
      onData: (txt) => mainWindow && mainWindow.webContents.send('tab:data', { tabId, txt }),
      onJson: (obj) => mainWindow && mainWindow.webContents.send('tab:json', { tabId, obj }),
      onExit: () => mainWindow && mainWindow.webContents.send('tab:exit', { tabId })
    }).start();
    tabs.set(tabId, eng);
    return { ok: true, tabId };
  });

  // Send a command into a live tab's engine.
  ipcMain.on('tab:send', (_e, { tabId, cmd, arg }) => {
    const eng = tabs.get(tabId);
    if (eng) eng.run(cmd, arg);
  });

  // Close a tab's engine.
  ipcMain.on('tab:close', (_e, { tabId }) => {
    const eng = tabs.get(tabId);
    if (eng) { eng.stop(); tabs.delete(tabId); }
  });

  // ─── Environment: ~/.emtypyie/env.json ───
  ipcMain.handle('env:get', async () => readEnvJson());
  ipcMain.handle('env:set', async (_e, obj) => {
    const ok = writeEnvJson(obj || {});
    return { ok };
  });

  // ─── Update flow: Node-side GitHub check + native zip apply ───
  const WRAPPER_VERSION = '1.0.1';
  const REPO = 'emtypyie/Emtypyie.cli';

  function httpsJson(url) {
    return new Promise((resolve, reject) => {
      const req = https.get(url, {
        headers: { 'User-Agent': 'Emtypyie-GUI', 'Accept': 'application/vnd.github+json' }
      }, (res) => {
        let data = '';
        res.on('data', (d) => (data += d));
        res.on('end', () => {
          try { resolve(JSON.parse(data)); }
          catch (e) { reject(e); }
        });
      });
      req.on('error', reject);
    });
  }

  function httpsText(url) {
    return new Promise((resolve, reject) => {
      https.get(url, { headers: { 'User-Agent': 'Emtypyie-GUI' } }, (res) => {
        let data = '';
        res.on('data', (d) => (data += d));
        res.on('end', () => resolve(data));
      }).on('error', reject);
    });
  }

  function compareVersions(a, b) {
    const pa = a.replace(/^v/, '').split('.').map(Number);
    const pb = b.replace(/^v/, '').split('.').map(Number);
    for (let i = 0; i < Math.max(pa.length, pb.length); i++) {
      const x = pa[i] || 0, y = pb[i] || 0;
      if (x !== y) return x - y;
    }
    return 0;
  }

  ipcMain.handle('update:check', async () => {
    try {
      const rel = await httpsJson(`https://api.github.com/repos/${REPO}/releases/latest`);
      const engineVer = (rel.tag_name || '').replace(/^v/, '');
      let wrapperVer = WRAPPER_VERSION;
      const wvAsset = (rel.assets || []).find(a => a.name === 'wrapper-version.txt');
      if (wvAsset) {
        try { wrapperVer = (await httpsText(wvAsset.browser_download_url)).trim(); } catch (_) {}
      }
      const isNewer = compareVersions(wrapperVer, WRAPPER_VERSION) > 0;
      const engineAsset = (rel.assets || []).find(a => /emtypyie-cli-native-windows-x64/i.test(a.name || ''));
      const wrapperAsset = (rel.assets || []).find(a => a.name === 'emtypyie.cli-Wrapper.zip');
      return {
        updateAvailable: isNewer,
        engineVersion: engineVer,
        wrapperVersion: wrapperVer,
        notes: rel.body || '',
        engineUrl: engineAsset ? engineAsset.browser_download_url : null,
        wrapperUrl: wrapperAsset ? wrapperAsset.browser_download_url : null
      };
    } catch (e) {
      return { updateAvailable: false, engineVersion: '', wrapperVersion: WRAPPER_VERSION, notes: '', engineUrl: null, wrapperUrl: null, error: e.message };
    }
  });

  ipcMain.handle('update:apply', async (_e, { url, type }) => {
    return new Promise((resolve) => {
      if (!url) { resolve({ ok: false, msg: 'no asset url' }); return; }
      const tmp = path.join(envDir(), 'update.zip');
      let received = 0;
      const req = https.get(url, { headers: { 'User-Agent': 'Emtypyie-GUI' } }, (res) => {
        const total = parseInt(res.headers['content-length'] || '0', 10);
        const file = fs.createWriteStream(tmp);
        res.on('data', (chunk) => {
          received += chunk.length;
          if (total) mainWindow && mainWindow.webContents.send('update:progress', (received / total) * 100);
        });
        res.on('end', () => {
          file.end(() => {
            try {
              const AdmZip = require('adm-zip');
              const zip = new AdmZip(tmp);
              const entries = zip.getEntries();
              if (type === 'wrapper') {
                const appRoot = path.join(process.resourcesPath, '..');
                zip.extractAllOverwrite(appRoot);
                fs.unlinkSync(tmp);
                if (mainWindow) mainWindow.webContents.send('update:progress', 100);
                mainWindow && mainWindow.webContents.send('update:done');
                return resolve({ ok: true });
              }
              const target = enginePath();
              let wrote = false;
              for (const e of entries) {
                if (/emtypyie\.exe$/i.test(e.entryName)) {
                  zip.extractEntryTo(e, path.dirname(target), false, true);
                  wrote = true;
                  break;
                }
              }
              fs.unlinkSync(tmp);
              if (mainWindow) mainWindow.webContents.send('update:progress', 100);
              mainWindow && mainWindow.webContents.send('update:done');
              resolve({ ok: wrote });
            } catch (err) {
              resolve({ ok: false, msg: err.message });
            }
          });
        });
      });
      req.on('error', (e) => resolve({ ok: false, msg: e.message }));
    });
  });

  // ─── Runtime (C engine binary) check + download ───
  const RUNTIME_ASSET_PATTERN = /emtypyie-cli-native-windows-x64/i;

  ipcMain.handle('runtime:check', async () => {
    const exePath = enginePath();
    const exists = fs.existsSync(exePath);
    let url = null;
    try {
      const rel = await httpsJson(`https://api.github.com/repos/${REPO}/releases/latest`);
      const asset = (rel.assets || []).find(a => RUNTIME_ASSET_PATTERN.test(a.name || ''));
      if (asset) url = asset.browser_download_url;
    } catch (_) {}
    return { exists, url };
  });

  ipcMain.handle('runtime:install', async (_e, { url }) => {
    return new Promise((resolve) => {
      if (!url) { resolve({ ok: false, msg: 'no asset url' }); return; }
      const tmp = path.join(envDir(), 'runtime.zip');
      let received = 0;
      const req = https.get(url, { headers: { 'User-Agent': 'Emtypyie-GUI' } }, (res) => {
        const total = parseInt(res.headers['content-length'] || '0', 10);
        const file = fs.createWriteStream(tmp);
        res.on('data', (chunk) => {
          received += chunk.length;
          if (total) mainWindow && mainWindow.webContents.send('runtime:progress', (received / total) * 100);
        });
        res.on('end', () => {
          file.end(() => {
            try {
              const AdmZip = require('adm-zip');
              const zip = new AdmZip(tmp);
              const entries = zip.getEntries();
              const target = enginePath();
              let wrote = false;
              for (const e of entries) {
                if (/emtypyie\.exe$/i.test(e.entryName)) {
                  zip.extractEntryTo(e, path.dirname(target), false, true);
                  wrote = true;
                  break;
                }
              }
              fs.unlinkSync(tmp);
              if (mainWindow) mainWindow.webContents.send('runtime:progress', 100);
              mainWindow && mainWindow.webContents.send('runtime:done');
              resolve({ ok: wrote });
            } catch (err) {
              resolve({ ok: false, msg: err.message });
            }
          });
        });
      });
      req.on('error', (e) => resolve({ ok: false, msg: e.message }));
    });
  });

  // Auto-check for updates shortly after launch.
  setTimeout(() => {
    mainWindow && mainWindow.webContents.send('update:autocheck');
  }, 2500);

  app.on('activate', () => { if (BrowserWindow.getAllWindows().length === 0) createWindow(); });
});

app.on('window-all-closed', () => {
  for (const eng of tabs.values()) eng.stop();
  tabs.clear();
  if (process.platform !== 'darwin') app.quit();
});
