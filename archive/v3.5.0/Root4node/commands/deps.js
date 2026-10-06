const fs = require('fs');
const path = require('path');
const { execSync, spawn } = require('child_process');
const t = require('./theme');

function isCommandAvailable(cmd) {
  try {
    execSync(`${cmd} --version`, { stdio: 'ignore', timeout: 3000 });
    return true;
  } catch {
    return false;
  }
}

async function installPythonDeps(deps, projectName) {
  if (!deps || deps.length === 0) return true;
  const devDir = t.getDevDir(projectName);
  const reqPath = path.join(devDir, 'requirements.txt');
  
  console.log(t.retroDim('  Installing Python dependencies...'));
  try {
    const content = deps.join('\n');
    fs.writeFileSync(reqPath, content);
    const launcher = process.platform === 'win32' ? 'py -3' : 'python3';
    execSync(`${launcher} -m pip install -r "${reqPath}" -q`, { 
      stdio: 'pipe', 
      timeout: 120000 
    });
    console.log(t.retro('  Python dependencies installed.'));
    return true;
  } catch (err) {
    console.log(t.retroWarn('  pip install had issues.'));
    return false;
  }
}

async function installNpmDeps(deps, projectName) {
  if (!deps || deps.length === 0) return true;
  const devDir = t.getDevDir(projectName);
  
  console.log(t.retroDim('  Installing Node.js dependencies...'));
  try {
    const pkgPath = path.join(devDir, 'package.json');
    let pkg = { dependencies: {} };
    if (fs.existsSync(pkgPath)) {
      pkg = JSON.parse(fs.readFileSync(pkgPath, 'utf8'));
    }
    pkg.dependencies = pkg.dependencies || {};
    for (const dep of deps) {
      const [name, version] = dep.split('@');
      pkg.dependencies[name] = version || 'latest';
    }
    fs.writeFileSync(pkgPath, JSON.stringify(pkg, null, 2));
    
    execSync('npm install', { cwd: devDir, stdio: 'pipe', timeout: 120000 });
    console.log(t.retro('  Node.js dependencies installed.'));
    return true;
  } catch (err) {
    console.log(t.retroWarn('  npm install had issues.'));
    return false;
  }
}

async function checkSystemDeps(deps) {
  if (!deps || deps.length === 0) return true;
  
  console.log(t.retroDim('  Checking system dependencies...'));
  const missing = [];
  for (const dep of deps) {
    if (!isCommandAvailable(dep)) {
      missing.push(dep);
    }
  }
  
  if (missing.length > 0) {
    console.log(t.retroWarn(`  Missing system dependencies: ${missing.join(', ')}`));
    return false;
  }
  
  console.log(t.retro('  All system dependencies satisfied.'));
  return true;
}

async function handleDepsCommand(projectName, args) {
  const fetch = require('./fetch');
  
  let proj;
  try {
    proj = await fetch.fetchProject(projectName);
  } catch (err) {
    console.log(t.retroErr(err.message));
    return;
  }

  const action = args[0] || 'install';
  
  switch (action) {
    case 'install':
      await depsInstall(projectName, proj);
      break;
    case 'update':
      await depsUpdate(projectName, proj);
      break;
    case 'list':
      depsList(proj);
      break;
    case 'check':
      await depsCheck(projectName, proj);
      break;
    default:
      console.log(t.retroErr(`  Unknown deps action: ${action}`));
      console.log(t.retroDim('  Available: install, update, list, check'));
  }
}

async function depsInstall(projectName, proj) {
  console.log();
  console.log(t.retroDim('  \u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501'));
  console.log(t.retro(`  Installing dependencies for ${t.retroAccent(proj.name || projectName)}`));
  console.log(t.retroDim('  \u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501'));
  console.log();

  let allOk = true;
  
  if (proj.dependencies?.python) {
    allOk = await installPythonDeps(proj.dependencies.python, projectName) && allOk;
  }
  
  if (proj.dependencies?.npm) {
    allOk = await installNpmDeps(proj.dependencies.npm, projectName) && allOk;
  }
  
  if (proj.dependencies?.system) {
    allOk = await checkSystemDeps(proj.dependencies.system) && allOk;
  }
  
  if (allOk) {
    console.log();
    console.log(t.retro('  \u2714 All dependencies installed.'));
  } else {
    console.log();
    console.log(t.retroWarn('  Some dependencies had issues (see above).'));
  }
  console.log();
}

async function depsUpdate(projectName, proj) {
  console.log(t.retroDim('  Updating dependencies...'));
  
  if (proj.dependencies?.npm) {
    const devDir = t.getDevDir(projectName);
    try {
      execSync('npm update', { cwd: devDir, stdio: 'pipe', timeout: 120000 });
      console.log(t.retro('  Node.js dependencies updated.'));
    } catch {
      console.log(t.retroWarn('  npm update had issues.'));
    }
  }
  
  if (proj.dependencies?.python) {
    const devDir = t.getDevDir(projectName);
    const reqPath = path.join(devDir, 'requirements.txt');
    if (fs.existsSync(reqPath)) {
      try {
        const launcher = process.platform === 'win32' ? 'py -3' : 'python3';
        execSync(`${launcher} -m pip install --upgrade -r "${reqPath}" -q`, { 
          stdio: 'pipe', 
          timeout: 120000 
        });
        console.log(t.retro('  Python dependencies updated.'));
      } catch {
        console.log(t.retroWarn('  pip upgrade had issues.'));
      }
    }
  }
  
  console.log();
}

function depsList(proj) {
  console.log();
  console.log(t.retroDim('  \u2503 Dependencies \u2503'));
  console.log();
  
  if (proj.dependencies?.python) {
    console.log(t.retro('  Python:'));
    for (const dep of proj.dependencies.python) {
      console.log(t.retroDim(`    ${dep}`));
    }
  }
  
  if (proj.dependencies?.npm) {
    console.log(t.retro('  Node.js:'));
    for (const dep of proj.dependencies.npm) {
      console.log(t.retroDim(`    ${dep}`));
    }
  }
  
  if (proj.dependencies?.system) {
    console.log(t.retro('  System:'));
    for (const dep of proj.dependencies.system) {
      const status = isCommandAvailable(dep) ? t.retro('✓') : t.retroErr('✗');
      console.log(t.retroDim(`    ${status} ${dep}`));
    }
  }
  
  if (proj.dependencies?.cargo) {
    console.log(t.retro('  Rust (Cargo):'));
    for (const dep of proj.dependencies.cargo) {
      console.log(t.retroDim(`    ${dep}`));
    }
  }
  
  console.log();
}

async function depsCheck(projectName, proj) {
  console.log();
  console.log(t.retroDim('  \u2503 Dependency Check \u2503'));
  console.log();
  
  let allOk = true;
  
  if (proj.dependencies?.system) {
    for (const dep of proj.dependencies.system) {
      const available = isCommandAvailable(dep);
      console.log(t.retroDim(`  ${available ? t.retro('✓') : t.retroErr('✗')} ${dep}`));
      if (!available) allOk = false;
    }
  }
  
  if (proj.dependencies?.npm) {
    const devDir = t.getDevDir(projectName);
    const pkgPath = path.join(devDir, 'package.json');
    if (fs.existsSync(pkgPath)) {
      const pkg = JSON.parse(fs.readFileSync(pkgPath, 'utf8'));
      const deps = pkg.dependencies || {};
      for (const [name, version] of Object.entries(deps)) {
        try {
          execSync(`npm list ${name}`, { cwd: devDir, stdio: 'ignore' });
          console.log(t.retroDim(`  ${t.retro('✓')} ${name}@${version}`));
        } catch {
          console.log(t.retroDim(`  ${t.retroErr('✗')} ${name}@${version} (not installed)`));
          allOk = false;
        }
      }
    } else {
      console.log(t.retroWarn('  No package.json found'));
      allOk = false;
    }
  }
  
  if (proj.dependencies?.python) {
    const devDir = t.getDevDir(projectName);
    const reqPath = path.join(devDir, 'requirements.txt');
    if (fs.existsSync(reqPath)) {
      const content = fs.readFileSync(reqPath, 'utf8');
      for (const line of content.split('\n')) {
        const dep = line.trim();
        if (!dep || dep.startsWith('#')) continue;
        try {
          const launcher = process.platform === 'win32' ? 'py -3' : 'python3';
          execSync(`${launcher} -c "import ${dep.split('>=')[0].split('==')[0].replace('-', '_')}"`, { stdio: 'ignore' });
          console.log(t.retroDim(`  ${t.retro('✓')} ${dep}`));
        } catch {
          console.log(t.retroDim(`  ${t.retroErr('✗')} ${dep} (not installed)`));
          allOk = false;
        }
      }
    }
  }
  
  console.log();
  if (allOk) {
    console.log(t.retro('  All dependencies satisfied.'));
  } else {
    console.log(t.retroWarn('  Some dependencies missing. Run /<project> deps install'));
  }
  console.log();
}

module.exports = { handleDepsCommand };