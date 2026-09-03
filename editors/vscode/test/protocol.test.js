'use strict';

const assert = require('assert');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { pathToFileURL } = require('url');
const { LspConnection } = require('../protocol');
const { formatSource } = require('../formatter');

async function main() {
  const grammar = JSON.parse(fs.readFileSync(
    path.resolve(__dirname, '..', 'syntaxes', 'tapas.tmLanguage.json'), 'utf8'));
  const namedFunction = new RegExp(grammar.repository.functions.patterns[0].match)
    .exec('function add(left: Int) -> Int');
  assert.strictEqual(namedFunction[3], 'add');
  const operator = new RegExp(grammar.repository.operators.patterns[0].match);
  assert.strictEqual(operator.exec('->')[0], '->');

  const executable = process.argv[2] || path.resolve(
    __dirname, '..', '..', '..', 'build', 'bin', 'tapas-language-server');
  const runtime = process.argv[3] || path.join(path.dirname(executable), 'tapas');
  const standardLibrary = path.resolve(path.dirname(runtime), '..', 'stdlib');
  const formatted = await formatSource(runtime,
    'function compact(value: Int) -> Int {\n' +
    '    if(true){ return value }else{ return 0 }\n}\n' +
    'function expanded(\n        value: Int,\n) -> Int\n{\n    return value\n}\n',
    standardLibrary);
  assert.strictEqual(formatted,
    'function compact(value: Int) -> Int\n{\n' +
    '    if (true) { return value } else { return 0 }\n}\n' +
    'function expanded(\n        value: Int,\n) -> Int {\n    return value\n}\n');
  let resolveExit;
  const exited = new Promise((resolve) => {
    resolveExit = resolve;
  });
  const connection = new LspConnection(executable, [], { onExit: resolveExit });
  const workspace = fs.mkdtempSync(path.join(os.tmpdir(), 'tapas-lsp-'));
  const modulePath = path.join(workspace, 'module.tap');
  const mainPath = path.join(workspace, 'main.tap');
  fs.writeFileSync(modulePath,
    "let answer = 42\nfunction double(value: Int) -> Int { return value + value }\nreturn {'answer': answer, 'double': double}\n");
  const workspaceUri = pathToFileURL(workspace).toString();
  const mainUri = pathToFileURL(mainPath).toString();
  const moduleUri = pathToFileURL(fs.realpathSync(modulePath)).toString();
  let resolveDiagnostics;
  const diagnosticsReceived = new Promise((resolve) => {
    resolveDiagnostics = resolve;
  });
  connection.onNotification('textDocument/publishDiagnostics', resolveDiagnostics);
  await connection.start();
  const initialized = await connection.request('initialize', {
    processId: process.pid,
    rootUri: workspaceUri,
    capabilities: { general: { positionEncodings: ['utf-16'] } },
  });
  assert.strictEqual(initialized.serverInfo.name, 'Tapas Language Server');
  assert.strictEqual(initialized.serverInfo.version, '0.1.0');
  const semanticLegend = initialized.capabilities.semanticTokensProvider.legend;
  assert.deepStrictEqual(semanticLegend.tokenTypes,
    ['namespace', 'type', 'function', 'parameter', 'variable', 'keyword']);
  const catalog = await connection.request('tapas/syntaxCatalog', {});
  const matches = (patterns, text) => patterns.some((pattern) =>
    new RegExp(`^(?:${pattern.match})$`).test(text));
  const keywordPatterns = [
    ...grammar.repository.keywords.patterns,
    ...grammar.repository.constants.patterns,
  ];
  for (const keyword of catalog.keywords)
    assert.ok(matches(keywordPatterns, keyword), `${keyword} is missing from TextMate keywords`);
  for (const type of catalog.types)
    assert.ok(matches(grammar.repository.types.patterns.slice(0, 1), type),
      `${type} is missing from TextMate types`);
  for (const packageName of catalog.packages)
    assert.ok(matches(grammar.repository.types.patterns.slice(1), packageName),
      `${packageName} is missing from TextMate namespaces`);
  connection.notify('initialized', {});
  connection.notify('textDocument/didOpen', {
    textDocument: {
      uri: 'file:///vscode.tap', languageId: 'tapas', version: 1,
      text: 'let value = 1\nlet result = value +\n',
    },
  });
  const diagnostics = await Promise.race([
    diagnosticsReceived,
    new Promise((_, reject) => setTimeout(
      () => reject(new Error('diagnostics notification timed out')), 2000)),
  ]);
  assert.ok(diagnostics && diagnostics.diagnostics.length > 0);
  const hover = await connection.request('textDocument/hover', {
    textDocument: { uri: 'file:///vscode.tap' },
    position: { line: 1, character: 13 },
  });
  assert.match(hover.contents.value, /value: Int/);
  const completion = await connection.request('textDocument/completion', {
    textDocument: { uri: 'file:///vscode.tap' },
    position: { line: 1, character: 13 },
  });
  assert.ok(completion.some((item) => item.label === 'value'));
  const semanticSource =
    'function add(left: Int, right: List[Int]) -> Int {\n' +
    '  return left + right[0]\n}\nlet answer = add(1, [2])\n' +
    'let shown = print(answer)\nlet root = math::sqrt(4.0)\n';
  connection.notify('textDocument/didChange', {
    textDocument: { uri: 'file:///vscode.tap', version: 2 },
    contentChanges: [{ text: semanticSource }],
  });
  const parameterHover = await connection.request('textDocument/hover', {
    textDocument: { uri: 'file:///vscode.tap' },
    position: { line: 1, character: 10 },
  });
  assert.match(parameterHover.contents.value, /parameter left: Int/);
  const functionHover = await connection.request('textDocument/hover', {
    textDocument: { uri: 'file:///vscode.tap' },
    position: { line: 3, character: 14 },
  });
  assert.match(functionHover.contents.value,
    /add: Function\[Int, List\[Int\]\] -> Int/);
  const callResultHover = await connection.request('textDocument/hover', {
    textDocument: { uri: 'file:///vscode.tap' },
    position: { line: 3, character: 5 },
  });
  assert.match(callResultHover.contents.value, /answer: Int/);
  connection.notify('textDocument/didChange', {
    textDocument: { uri: 'file:///vscode.tap', version: 3 },
    contentChanges: [{ text: 'pprint([1])\n' }],
  });
  const builtinHover = await connection.request('textDocument/hover', {
    textDocument: { uri: 'file:///vscode.tap' },
    position: { line: 0, character: 2 },
  });
  assert.match(builtinHover.contents.value,
    /pprint\(\.\.\.values: AnyType\) -> Nil/);
  connection.notify('textDocument/didChange', {
    textDocument: { uri: 'file:///vscode.tap', version: 4 },
    contentChanges: [{ text: 'let primes = [2, 3, 5]\nlet copied = primes.copy()\n' }],
  });
  const tunnelHover = await connection.request('textDocument/hover', {
    textDocument: { uri: 'file:///vscode.tap' },
    position: { line: 1, character: 22 },
  });
  assert.match(tunnelHover.contents.value, /copy\(value: T\) -> T/);
  connection.notify('textDocument/didChange', {
    textDocument: { uri: 'file:///vscode.tap', version: 5 },
    contentChanges: [{ text: semanticSource }],
  });
  const semanticTokens = await connection.request('textDocument/semanticTokens/full', {
    textDocument: { uri: 'file:///vscode.tap' },
  });
  assert.ok(semanticTokens.data.length > 0);
  const semanticKinds = new Set();
  const semanticNames = new Map();
  const lines = semanticSource.split('\n');
  let line = 0;
  let character = 0;
  for (let index = 0; index < semanticTokens.data.length; index += 5) {
    line += semanticTokens.data[index];
    character = semanticTokens.data[index] ? semanticTokens.data[index + 1] :
      character + semanticTokens.data[index + 1];
    const name = lines[line].slice(character, character + semanticTokens.data[index + 2]);
    const kind = semanticLegend.tokenTypes[semanticTokens.data[index + 3]];
    semanticKinds.add(kind);
    semanticNames.set(name, kind);
  }
  for (const kind of ['namespace', 'keyword', 'function', 'parameter', 'type', 'variable'])
    assert.ok(semanticKinds.has(kind), `${kind} semantic token was not returned`);
  assert.strictEqual(semanticNames.get('print'), 'function');
  assert.strictEqual(semanticNames.get('math'), 'namespace');
  connection.notify('textDocument/didOpen', {
    textDocument: {
      uri: mainUri, languageId: 'tapas', version: 1,
      text: 'import module.tap as module\nlet result = module::ans\n',
    },
  });
  const moduleCompletion = await connection.request('textDocument/completion', {
    textDocument: { uri: mainUri }, position: { line: 1, character: 24 },
  });
  assert.ok(moduleCompletion.some((item) => item.label === 'answer' && item.detail === 'Int'));
  connection.notify('textDocument/didChange', {
    textDocument: { uri: mainUri, version: 2 },
    contentChanges: [{ text: 'import module.tap as module\nlet result = module::answer\n' }],
  });
  const definition = await connection.request('textDocument/definition', {
    textDocument: { uri: mainUri }, position: { line: 1, character: 23 },
  });
  assert.strictEqual(definition.uri, moduleUri);
  const memberHover = await connection.request('textDocument/hover', {
    textDocument: { uri: mainUri }, position: { line: 1, character: 23 },
  });
  assert.match(memberHover.contents.value, /answer: Int/);
  const references = await connection.request('textDocument/references', {
    textDocument: { uri: mainUri }, position: { line: 1, character: 23 },
    context: { includeDeclaration: true },
  });
  assert.ok(references.some((item) => item.uri === moduleUri));
  assert.ok(references.some((item) => item.uri === mainUri));
  const rename = await connection.request('textDocument/rename', {
    textDocument: { uri: mainUri }, position: { line: 1, character: 23 },
    newName: 'resultValue',
  });
  assert.ok(rename.changes[moduleUri].some((edit) => edit.newText === 'resultValue'));
  assert.ok(rename.changes[mainUri].some((edit) => edit.newText === 'resultValue'));
  const workspaceSymbols = await connection.request('workspace/symbol', { query: 'double' });
  assert.ok(workspaceSymbols.some((item) =>
    item.name === 'double' && item.location.uri === moduleUri));
  connection.notify('textDocument/didChange', {
    textDocument: { uri: mainUri, version: 3 },
    contentChanges: [{ text: "let custom = {'answer': 42}\nlet result = custom:\n" }],
  });
  const singleColonCompletion = await connection.request('textDocument/completion', {
    textDocument: { uri: mainUri }, position: { line: 1, character: 20 },
  });
  assert.deepStrictEqual(singleColonCompletion, []);
  connection.notify('textDocument/didChange', {
    textDocument: { uri: mainUri, version: 4 },
    contentChanges: [{ text: "let custom = {'answer': 42, 'name': 'Ada'}\nlet result = custom::an\n" }],
  });
  const dictionaryCompletion = await connection.request('textDocument/completion', {
    textDocument: { uri: mainUri }, position: { line: 1, character: 23 },
  });
  assert.ok(dictionaryCompletion.some((item) =>
    item.label === 'answer' && item.detail === 'Int'));
  assert.ok(!dictionaryCompletion.some((item) => item.label === 'name'));
  connection.notify('textDocument/didChange', {
    textDocument: { uri: mainUri, version: 5 },
    contentChanges: [{ text: "let custom = {\n  'answer': 42,\n  'name': 'Ada',\n}\ncustom::" }],
  });
  const emptyDictionaryPrefix = await connection.request('textDocument/completion', {
    textDocument: { uri: mainUri }, position: { line: 4, character: 8 },
  });
  assert.ok(emptyDictionaryPrefix.some((item) => item.label === 'answer'));
  assert.ok(emptyDictionaryPrefix.some((item) => item.label === 'name'));
  connection.notify('textDocument/didChange', {
    textDocument: { uri: mainUri, version: 6 },
    contentChanges: [{ text: 'math::' }],
  });
  const emptyPackagePrefix = await connection.request('textDocument/completion', {
    textDocument: { uri: mainUri }, position: { line: 0, character: 6 },
  });
  assert.ok(emptyPackagePrefix.some((item) => item.label === 'sqrt'));
  connection.notify('textDocument/didChange', {
    textDocument: { uri: mainUri, version: 7 },
    contentChanges: [{ text: 'let result = math::sq\n' }],
  });
  const mathCompletion = await connection.request('textDocument/completion', {
    textDocument: { uri: mainUri }, position: { line: 0, character: 21 },
  });
  assert.ok(mathCompletion.some((item) => item.label === 'sqrt'));
  connection.notify('textDocument/didChange', {
    textDocument: { uri: mainUri, version: 8 },
    contentChanges: [{ text: 'let result = ma\n' }],
  });
  const defaultCompletion = await connection.request('textDocument/completion', {
    textDocument: { uri: mainUri }, position: { line: 0, character: 15 },
  });
  assert.ok(defaultCompletion.some((item) => item.label === 'math' && item.kind === 9));
  connection.notify('textDocument/didChange', {
    textDocument: { uri: mainUri, version: 9 },
    contentChanges: [{ text: 'let result = __\n' }],
  });
  const sessionCompletion = await connection.request('textDocument/completion', {
    textDocument: { uri: mainUri }, position: { line: 0, character: 15 },
  });
  for (const name of ['__ls__', '__path__', '__param__', '__nparam__', '__binary__'])
    assert.ok(sessionCompletion.some((item) => item.label === name), `${name} was not completed`);
  await connection.stop();
  const exitInfo = await exited;
  assert.strictEqual(exitInfo.expected, true);
  fs.rmSync(workspace, { recursive: true, force: true });
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
