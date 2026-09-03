'use strict';

const assert = require('assert');
const fs = require('fs');
const path = require('path');

const root = path.resolve(__dirname, '..');
const manifest = JSON.parse(fs.readFileSync(
  path.join(root, 'package.json'), 'utf8'));
const ignored = fs.readFileSync(path.join(root, '.vscodeignore'), 'utf8')
  .split(/\r?\n/)
  .map((line) => line.trim())
  .filter(Boolean);

assert.match(manifest.version, /^\d+\.\d+\.\d+$/);
assert.strictEqual(manifest.license, 'MIT');
assert.ok(manifest.publisher);
assert.ok(manifest.repository?.url);
assert.deepStrictEqual(manifest.extensionKind, ['workspace']);

for (const filename of [
  'extension.js', 'formatter.js', 'protocol.js', 'README.md', 'LICENSE',
  'language-configuration.json', 'syntaxes/tapas.tmLanguage.json',
]) {
  assert.ok(fs.statSync(path.join(root, filename)).isFile(),
    `${filename} must be included in the extension`);
}

for (const directory of ['runtime/**', 'server/**', 'stdlib/**']) {
  assert.ok(ignored.includes(directory),
    `${directory} must remain excluded from the thin extension`);
}
for (const filename of ['README.md', 'LICENSE']) {
  assert.ok(!ignored.includes(filename),
    `${filename} must remain visible in the Marketplace package`);
}

const clientSource = fs.readFileSync(path.join(root, 'extension.js'), 'utf8');
assert.ok(clientSource.includes("return 'tapas-language-server'"),
  'the Language Server must fall back to PATH');
assert.ok(clientSource.includes("return 'tapas'"),
  'the runtime must fall back to PATH');

console.log(`Tapas VS Code ${manifest.version} package metadata is valid.`);
