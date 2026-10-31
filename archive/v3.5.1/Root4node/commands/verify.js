const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const { execSync } = require('child_process');

const t = require('./theme');

function sha256File(filePath) {
  return new Promise((resolve, reject) => {
    const hash = crypto.createHash('sha256');
    const stream = fs.createReadStream(filePath);
    stream.on('error', reject);
    stream.on('data', chunk => hash.update(chunk));
    stream.on('end', () => resolve(hash.digest('hex')));
  });
}

function getManifestPath(projectName) {
  const devDir = t.getDevDir(projectName);
  return path.join(devDir, '.emtypyie-manifest.json');
}

function loadManifest(projectName) {
  const manifestPath = getManifestPath(projectName);
  if (!fs.existsSync(manifestPath)) return null;
  try {
    return JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
  } catch {
    return null;
  }
}

function saveManifest(projectName, manifest) {
  const manifestPath = getManifestPath(projectName);
  fs.writeFileSync(manifestPath, JSON.stringify(manifest, null, 2));
}

async function generateManifest(projectName, projectMeta) {
  const devDir = t.getDevDir(projectName);
  const files = [];
  
  if (!projectMeta.integrity || !projectMeta.integrity.files) {
    return { project: projectName, version: projectMeta.version, files: [], manifestSha256: '' };
  }

  for (const fileInfo of projectMeta.integrity.files) {
    const fullPath = path.join(devDir, fileInfo.path);
    if (fs.existsSync(fullPath)) {
      const stats = fs.statSync(fullPath);
      const hash = await sha256File(fullPath);
      files.push({
        path: fileInfo.path,
        sha256: hash,
        size: stats.size,
        expectedSha256: fileInfo.sha256,
        expectedSize: fileInfo.size
      });
    }
  }

  const manifest = {
    project: projectName,
    version: projectMeta.version,
    installedAt: new Date().toISOString(),
    files,
    manifestSha256: projectMeta.integrity.manifestSha256 || ''
  };

  const manifestContent = JSON.stringify(manifest, null, 2);
  const manifestHash = crypto.createHash('sha256').update(manifestContent).digest('hex');
  manifest.manifestSha256 = manifestHash;

  saveManifest(projectName, manifest);
  return manifest;
}

async function verifyProject(projectName, quick = false) {
  const manifest = loadManifest(projectName);
  if (!manifest) {
    return { ok: false, error: 'No manifest found. Run /get first or /rebuild to create manifest.' };
  }

  const devDir = t.getDevDir(projectName);
  const results = { ok: true, checked: 0, mismatched: [], missing: [] };

  for (const fileInfo of manifest.files) {
    const fullPath = path.join(devDir, fileInfo.path);
    
    if (!fs.existsSync(fullPath)) {
      results.missing.push(fileInfo.path);
      results.ok = false;
      continue;
    }

    const stats = fs.statSync(fullPath);
    if (stats.size !== fileInfo.size) {
      results.mismatched.push({ path: fileInfo.path, reason: 'size mismatch' });
      results.ok = false;
      continue;
    }

    if (!quick) {
      const hash = await sha256File(fullPath);
      if (hash !== fileInfo.sha256) {
        results.mismatched.push({ path: fileInfo.path, reason: 'hash mismatch' });
        results.ok = false;
      }
    }
    results.checked++;
  }

  if (results.missing.length > 0 || results.mismatched.length > 0) {
    results.ok = false;
  }

  return results;
}

async function rebuildProject(projectName) {
  const fetch = require('./fetch');
  const getCommand = require('./get');
  
  console.log(t.retro(`  Rebuilding ${t.retroAccent(projectName)}...`));
  console.log();

  let proj;
  try {
    proj = await fetch.fetchProject(projectName);
  } catch (err) {
    console.log(t.retroErr(err.message));
    return false;
  }

  const devDir = t.getDevDir(projectName);
  if (fs.existsSync(devDir)) {
    fs.rmSync(devDir, { recursive: true, force: true });
    console.log(t.retroDim('  Removed old files.'));
  }

  await getCommand.install(projectName, proj);
  await generateManifest(projectName, proj);
  
  console.log(t.retro('  Rebuild complete!'));
  console.log();
  return true;
}

function showVerifyResults(results, projectName) {
  if (results.ok) {
    console.log(t.retro(`  ${t.retro('✓')} ${projectName}: All ${results.checked} files verified OK`));
  } else {
    console.log(t.retroErr(`  ${t.retroErr('✗')} ${projectName}: Integrity check FAILED`));
    if (results.missing.length > 0) {
      console.log(t.retroWarn(`  Missing files (${results.missing.length}):`));
      for (const f of results.missing) console.log(t.retroWarn(`    - ${f}`));
    }
    if (results.mismatched.length > 0) {
      console.log(t.retroWarn(`  Mismatched files (${results.mismatched.length}):`));
      for (const f of results.mismatched) console.log(t.retroWarn(`    - ${f.path} (${f.reason})`));
    }
    console.log();
    console.log(t.retroWarn(`  ${projectName} has been tampered. Please run /${projectName} rebuild for smooth experience`));
  }
}

module.exports = {
  sha256File,
  getManifestPath,
  loadManifest,
  saveManifest,
  generateManifest,
  verifyProject,
  rebuildProject,
  showVerifyResults
};