const readline = require('readline');

let active = false;

function begin(rl) {
  active = true;
  if (rl) rl.pause();
}

function end(rl) {
  active = false;
  if (rl) { rl.resume(); rl.prompt(); }
}

function keypress() {
  return new Promise((resolve) => {
    const wasRaw = process.stdin.isRaw;
    if (process.stdin.setRawMode) process.stdin.setRawMode(true);
    process.stdin.resume();
    const onData = (d) => {
      const s = d.toString();
      process.stdin.removeListener('data', onData);
      if (process.stdin.setRawMode && wasRaw !== undefined) process.stdin.setRawMode(wasRaw === true);
      if (s === '\x03') resolve('ctrlc');
      else if (s === '\x1b') resolve('esc');
      else if (s === '\r' || s === '\n') resolve('enter');
      else resolve(s.toLowerCase());
    };
    process.stdin.on('data', onData);
  });
}

async function confirmYN(question) {
  process.stdout.write(`${question} `);
  while (true) {
    const k = await keypress();
    if (k === 'y') { process.stdout.write('y\n'); return true; }
    if (k === 'n' || k === 'esc' || k === 'ctrlc') { process.stdout.write(k === 'y' ? '\n' : 'n\n'); return false; }
  }
}

async function askChoice(question, options) {
  process.stdout.write(`${question} `);
  while (true) {
    const k = await keypress();
    if (options[k]) { process.stdout.write(options[k] + '\n'); return k; }
    if (k === 'esc' || k === 'ctrlc') { process.stdout.write('\n'); return null; }
  }
}

function askLine(question, fallback) {
  return new Promise((resolve) => {
    const rl = readline.createInterface({ input: process.stdin, output: process.stdout, terminal: true });
    process.stdout.write(`${question} `);
    rl.on('line', (answer) => {
      rl.close();
      resolve(answer.trim() || fallback);
    });
  });
}

async function pressEnterOrEsc() {
  process.stdout.write('  Press ENTER to continue, ESC to cancel...');
  while (true) {
    const k = await keypress();
    if (k === 'enter') { process.stdout.write('\n'); return true; }
    if (k === 'esc' || k === 'ctrlc') { process.stdout.write('\n'); return false; }
  }
}

module.exports = { get active() { return active; }, begin, end, confirmYN, askChoice, askLine, pressEnterOrEsc };
