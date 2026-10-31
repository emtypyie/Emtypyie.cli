const fs = require('fs');
const path = require('path');
const https = require('https');

const LOCAL_CDN_PATH = path.resolve(__dirname, '..', '..', '..', '..', 'dev-cdn');

const sleep = (ms) => new Promise(r => setTimeout(r, ms));

function parse(text) {
  const lines = text.split(/\r?\n/).map(l => l.trim()).filter(Boolean);
  const first = lines.shift() || '';
  const m = first.match(/^(.*?)\s+v?([\w.\-]+)$/);
  const result = {
    name: m ? m[1].trim() : first,
    version: m ? m[2] : '',
    deps: [],
    build: [],
    install: [],
    start: [],
  };
  for (const line of lines) {
    const tag = line.match(/^\[([a-z]+)\]\s*(.+)$/i);
    if (tag) {
      const key = tag[1].toLowerCase();
      if (result[key]) result[key].push(tag[2].trim());
    }
  }
  return result;
}

function fetchText(url) {
  return new Promise((resolve, reject) => {
    const req = https.get(url, { headers: { 'User-Agent': 'emtypyie-cli' }, timeout: 10000 }, (res) => {
      if (res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
        res.resume();
        fetchText(res.headers.location).then(resolve, reject);
        return;
      }
      if (res.statusCode !== 200) {
        res.resume();
        reject(new Error(`HTTP ${res.statusCode}`));
        return;
      }
      let data = '';
      res.on('data', c => data += c);
      res.on('end', () => resolve(data));
    });
    req.on('timeout', () => { req.destroy(); reject(new Error('timeout')); });
    req.on('error', reject);
  });
}

async function load(name) {
  const localCandidates = [
    path.join(LOCAL_CDN_PATH, name, '.emtypyie', 'installation.txt'),
    path.join(LOCAL_CDN_PATH, name, 'installation.txt'),
  ];
  for (const p of localCandidates) {
    try {
      return parse(fs.readFileSync(p, 'utf8'));
    } catch {}
  }
  const remoteCandidates = [
    `https://cdn.emtypyie.in/dev/${encodeURIComponent(name)}/.emtypyie/installation.txt`,
    `https://cdn.emtypyie.in/dev/${encodeURIComponent(name)}/installation.txt`,
  ];
  for (const url of remoteCandidates) {
    try {
      return parse(await fetchText(url));
    } catch {}
  }
  return null;
}

async function loadLicense(name) {
  const localCandidates = [
    path.join(LOCAL_CDN_PATH, name, '.emtypyie', 'LICENSE'),
    path.join(LOCAL_CDN_PATH, name, 'LICENSE'),
  ];
  for (const p of localCandidates) {
    try { return fs.readFileSync(p, 'utf8'); } catch {}
  }
  const remoteCandidates = [
    `https://cdn.emtypyie.in/dev/${encodeURIComponent(name)}/.emtypyie/LICENSE`,
    `https://cdn.emtypyie.in/dev/${encodeURIComponent(name)}/LICENSE`,
  ];
  for (const url of remoteCandidates) {
    try { return await fetchText(url); } catch {}
  }
  return null;
}

async function animatedStep(label, promiseFactory, options = {}) {
  process.stdout.write(`>> ${label}\n`);
  const width = 34;
  let done = false;
  let err = null;
  let value;
  const p = Promise.resolve().then(promiseFactory).then(
    v => { done = true; value = v; },
    e => { done = true; err = e; }
  );
  let pct = 0;
  while (!done) {
    pct = Math.min(92, pct + 4 + Math.floor(Math.random() * 9));
    const filled = Math.round(width * pct / 100);
    process.stdout.write(`\r  ${'-'.repeat(filled)}> ${pct}%   `);
    await sleep(90);
  }
  await p;
  const note = options.note || '';
  process.stdout.write(`\r  ${'-'.repeat(width)}> 100%${note ? '  ' + note : '   '}\n`);
  if (err) throw err;
  return value;
}

module.exports = { parse, load, loadLicense, animatedStep, fetchText };
