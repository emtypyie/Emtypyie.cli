const https = require('https');
const fs = require('fs');
const path = require('path');

const API_HOST = 'cdn.emtypyie.in';
const API_BASE = '/dev';
const TIMEOUT = 10000;

// Local dev-cdn path (primary source) - relative to this file
// __dirname = .../archive/v3.0.3/Root4node/commands
// Need to go up: commands -> Root4node -> v3.0.3 -> archive -> Emtypyie.cli -> dev-cdn (5 levels up to Emtypyie.cli, then dev-cdn)
const LOCAL_CDN_PATH = path.resolve(__dirname, '..', '..', '..', '..', 'dev-cdn');

function _get(remotePath) {
  // Try local dev-cdn first
  return tryLocalFallback(remotePath).then(resolve => {
    return resolve;
  }).catch(() => {
    // Fallback to CDN
    return fetchFromCDN(remotePath);
  });
}

function tryLocalFallback(remotePath) {
  const localPath = path.join(LOCAL_CDN_PATH, remotePath.replace('/dev/', ''));
  
  return new Promise((resolve, reject) => {
    fs.readFile(localPath, 'utf8', (err, data) => {
      if (err) {
        reject(err);
        return;
      }
      try {
        resolve(JSON.parse(data));
      } catch {
        reject(new Error('Local parse error'));
      }
    });
  });
}

function fetchFromCDN(remotePath) {
  return new Promise((resolve, reject) => {
    const opts = {
      hostname: API_HOST,
      path: remotePath,
      method: 'GET',
      headers: { 'User-Agent': 'emtypyie-cli' },
      timeout: TIMEOUT
    };

    const req = https.get(opts, (res) => {
      let data = '';
      res.on('data', chunk => data += chunk);
      res.on('end', () => {
        if (res.statusCode === 404) {
          reject(new Error('Failed to fetch data from cdn.emtypyie.in - use /list to see if it actually exists'));
          return;
        }
        if (res.statusCode !== 200) {
          reject(new Error('Cdn network error'));
          return;
        }
        try {
          resolve(JSON.parse(data));
        } catch {
          reject(new Error('Cdn network error'));
        }
      });
    });

    req.on('error', () => {
      req.destroy();
      reject(new Error('Cdn network error'));
    });

    req.on('timeout', () => {
      req.destroy();
      reject(new Error('Cdn network error'));
    });
  });
}

function fetchProjectList() {
  return _get(`${API_BASE}/meta.json`);
}

function fetchProject(name) {
  return _get(`${API_BASE}/${encodeURIComponent(name)}/metadata.json`);
}

module.exports = { fetchProjectList, fetchProject };