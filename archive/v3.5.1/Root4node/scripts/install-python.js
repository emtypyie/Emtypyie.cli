#!/usr/bin/env node

const runtime = require('../commands/install-runtime');

if (process.env.EMTYPYIE_SKIP_PYTHON === '1') {
  process.exit(0);
}

runtime.ensurePython({ quiet: true }).then((ok) => {
  if (ok) {
    console.log(`  [emtypyie] local Python ${runtime.PYTHON_VERSION} ready`);
  } else {
    console.log('  [emtypyie] local Python setup skipped (the CLI will retry on first run)');
  }
}).catch(() => {
  console.log('  [emtypyie] local Python setup skipped (the CLI will retry on first run)');
});
