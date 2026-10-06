const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');
const fetch = require('./fetch');
const t = require('./theme');

const TEMPLATE_API_BASE = '/dev/templates';

async function listTemplates() {
  console.log();
  console.log(t.retroDim('  \u2503 Templates \u2503'));
  console.log();
  
  try {
    const list = await fetch.fetchProjectList(); // This won't work for templates, need separate endpoint
    // For now, we'll hardcode known templates
    const templates = [
      { name: 'python-cli', description: 'Python CLI application with Click' },
      { name: 'node-cli', description: 'Node.js CLI application with Commander' },
      { name: 'rust-cli', description: 'Rust CLI application with Clap' }
    ];
    
    for (const tmpl of templates) {
      const pad = ' '.repeat(Math.max(0, 14 - tmpl.name.length));
      console.log(`  ${t.retroAccent(tmpl.name)}${pad}${t.retroDim(tmpl.description)}`);
    }
  } catch {
    console.log(t.retroDim('  (Could not fetch templates)'));
  }
  console.log();
}

async function createProjectFromTemplate(templateName, projectName, targetDir) {
  try {
    // Fetch template metadata
    const url = `https://cdn.emtypyie.in${TEMPLATE_API_BASE}/${templateName}/metadata.json`;
    const response = await fetchWithTimeout(url);
    if (!response.ok) throw new Error('Template not found');
    
    const template = await response.json();
    
    const fullPath = path.resolve(targetDir || process.cwd(), projectName);
    
    if (fs.existsSync(fullPath)) {
      console.log(t.retroErr(`  Directory "${projectName}" already exists.`));
      return false;
    }
    
    fs.mkdirSync(fullPath, { recursive: true });
    
    console.log(t.retro(`  Creating ${t.retroAccent(projectName)} from ${t.retroAccent(templateName)}...`));
    
    for (const file of template.files) {
      const filePath = path.join(fullPath, file.path);
      const dir = path.dirname(filePath);
      if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true });
      
      let content = file.content
        .replace(/\{\{name\}\}/g, projectName)
        .replace(/\{\{description\}\}/g, template.description);
      
      fs.writeFileSync(filePath, content);
      
      if (file.executable) {
        fs.chmodSync(filePath, 0o755);
      }
      
      console.log(t.retroDim(`  Created ${file.path}`));
    }
    
    if (template.postCreate) {
      console.log(t.retroDim('  Running post-create script...'));
      try {
        execSync(template.postCreate, { cwd: fullPath, stdio: 'inherit', timeout: 120000 });
        console.log(t.retro('  Post-create completed.'));
      } catch (err) {
        console.log(t.retroWarn(`  Post-create had issues: ${err.message}`));
      }
    }
    
    console.log();
    console.log(t.retro(`  ${t.retro('✓')} Project created at ${t.retroAccent(fullPath)}`));
    console.log(t.retroDim(`  cd ${projectName} && ${template.postCreate || '# start coding'}`));
    console.log();
    
    return true;
  } catch (err) {
    console.log(t.retroErr(`  Failed to create project: ${err.message}`));
    return false;
  }
}

function fetchWithTimeout(url, timeout = 10000) {
  return new Promise((resolve, reject) => {
    const controller = new AbortController();
    const id = setTimeout(() => controller.abort(), timeout);
    
    fetch(url, { signal: controller.signal })
      .then(res => { clearTimeout(id); resolve(res); })
      .catch(err => { clearTimeout(id); reject(err); });
  });
}

async function handleTemplateCommand(args) {
  const subcommand = args[0];
  
  if (!subcommand || subcommand === 'list') {
    await listTemplates();
    return;
  }
  
  if (subcommand === 'create' || subcommand === 'init') {
    const templateName = args[1];
    const projectName = args[2];
    const targetDir = args[3];
    
    if (!templateName || !projectName) {
      console.log(t.retroErr('  Usage: /new <template> <project-name> [target-dir]'));
      console.log(t.retroDim('  Or:    /init <template> <project-name> [target-dir]'));
      return;
    }
    
    await createProjectFromTemplate(templateName, projectName, targetDir);
    return;
  }
  
  console.log(t.retroErr(`  Unknown template command: ${subcommand}`));
  console.log(t.retroDim('  Available: list, create, init'));
}

module.exports = { handleTemplateCommand };