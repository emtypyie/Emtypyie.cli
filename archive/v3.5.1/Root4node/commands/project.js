const fetch = require('./fetch');
const getCommand = require('./get');
const verify = require('./verify');
const deps = require('./deps');
const t = require('./theme');

async function handleProjectSubcommand(projectName, subcommand, args) {
  const normalized = subcommand.replace(/^-+/, '').toLowerCase();
  
  let proj;
  try {
    proj = await fetch.fetchProject(projectName);
  } catch (err) {
    console.log(t.retroErr(err.message));
    return;
  }

  switch (normalized) {
    case 'upgrade':
      await upgradeProject(projectName, proj);
      break;
    case 'v':
    case 'version':
      showProjectVersion(projectName, proj);
      break;
    case 'rebuild':
      await verify.rebuildProject(projectName);
      break;
    case 'verify':
      await verifyAndShow(projectName, false);
      break;
    case 'deps':
      await deps.handleDepsCommand(projectName, args);
      break;
    case 'info':
      await showProjectInfo(projectName, proj);
      break;
    default:
      console.log(t.retroErr(`  Unknown subcommand: ${subcommand}`));
      console.log(t.retroDim('  Available: --upgrade, -v, rebuild, verify, deps, info'));
  }
}

async function upgradeProject(name, proj) {
  console.log(t.retro(`  Checking for updates to ${t.retroAccent(proj.name || name)}...`));
  
  try {
    const latest = await fetch.fetchProject(name);
    const currentVersion = proj.version || 'unknown';
    const latestVersion = latest.version || 'unknown';
    
    if (currentVersion === latestVersion) {
      console.log(t.retro(`  ${t.retro('✓')} Already at latest version (${currentVersion})`));
      return;
    }
    
    console.log(t.retroWarn(`  Update available: ${currentVersion} ---> ${latestVersion}`));
    console.log(t.retro('  Re-downloading...'));
    
    const devDir = t.getDevDir(name);
    if (fs.existsSync(devDir)) {
      fs.rmSync(devDir, { recursive: true, force: true });
    }
    
    await getCommand.install(name, latest);
    await verify.generateManifest(name, latest);
    
    console.log(t.retro(`  ${t.retro('✓')} Upgraded to ${latestVersion}`));
  } catch (err) {
    console.log(t.retroErr(`  Upgrade failed: ${err.message}`));
  }
}

function showProjectVersion(name, proj) {
  console.log();
  console.log(t.retroDim('  \u2503 ') + t.retroAccent(proj.name || name) + t.retroDim(' \u2503'));
  console.log(t.retroDim('  Version: ') + t.retro(proj.version || 'latest'));
  if (proj.packageManagers) {
    console.log();
    console.log(t.retroDim('  Package Managers:'));
    for (const [pm, info] of Object.entries(proj.packageManagers)) {
      console.log(t.retroDim(`    ${pm}: ${info.installCmd} (v${info.version || 'latest'})`));
    }
  }
  console.log();
}

async function showProjectInfo(name, proj) {
  console.log();
  console.log(t.retroDim('  \u2503 ') + t.retroAccent(proj.name || name) + t.retroDim(' \u2503'));
  console.log(t.retroDim('  Description: ') + t.retro(proj.description || 'N/A'));
  console.log(t.retroDim('  Version:     ') + t.retro(proj.version || 'latest'));
  console.log(t.retroDim('  Repo:        ') + t.retroAccent(proj.repo || 'N/A'));
  console.log(t.retroDim('  Download:    ') + t.retroDim(proj.download || 'N/A'));
  
  if (proj.techStack && proj.techStack.length > 0) {
    console.log();
    console.log(t.retroDim('  Tech Stack:'));
    for (const tech of proj.techStack) {
      const req = tech.required ? t.retroWarn(' (required)') : t.retroDim(' (optional)');
      console.log(t.retroDim(`    ${tech.name} ${tech.version}${req}`));
    }
  }
  
  if (proj.dependencies) {
    console.log();
    console.log(t.retroDim('  Dependencies:'));
    for (const [type, deps] of Object.entries(proj.dependencies)) {
      if (Array.isArray(deps) && deps.length > 0) {
        console.log(t.retroDim(`    ${type}: ${deps.join(', ')}`));
      }
    }
  }
  
  if (proj.integrity) {
    console.log();
    console.log(t.retroDim('  Integrity: ') + t.retro(`${proj.integrity.files?.length || 0} files tracked`));
  }
  
  if (proj.installScripts) {
    console.log();
    console.log(t.retroDim('  Install Scripts:'));
    for (const [phase, script] of Object.entries(proj.installScripts)) {
      if (script) console.log(t.retroDim(`    ${phase}: ${script}`));
    }
  }
  
  if (proj.info) {
    console.log();
    console.log(proj.info.trim().split('\n').map(l => `  ${l}`).join('\n'));
  }
  console.log();
}

async function verifyAndShow(projectName, quick) {
  const results = await verify.verifyProject(projectName, quick);
  verify.showVerifyResults(results, projectName);
}

module.exports = { handleProjectSubcommand };