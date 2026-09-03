'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawn } = require('child_process');

function execute(executable, args, cwd) {
  return new Promise((resolve, reject) => {
    const child = spawn(executable, args, {
      cwd,
      stdio: ['ignore', 'ignore', 'pipe'],
      windowsHide: true,
    });
    let stderr = '';
    child.stderr.on('data', (chunk) => {
      stderr += chunk.toString('utf8');
    });
    child.once('error', reject);
    child.once('exit', (code, signal) => {
      if (code === 0) resolve();
      else reject(new Error(stderr.trim() ||
        `Tapas formatter exited with ${code ?? signal}`));
    });
  });
}

async function formatSource(executable, source) {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'tapas-format-'));
  const filename = path.join(directory, 'document.tap');
  try {
    fs.writeFileSync(filename, source, 'utf8');
    await execute(executable, ['-m', 'format', filename], directory);
    return fs.readFileSync(filename, 'utf8');
  } finally {
    fs.rmSync(directory, { recursive: true, force: true });
  }
}

module.exports = { formatSource };
