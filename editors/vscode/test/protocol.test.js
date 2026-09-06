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
  const keywords = grammar.repository.keywords.patterns;
  const logical = new RegExp(keywords.find(
    pattern => pattern.name === 'keyword.operator.logical.tapas').match);
  for (const word of ['not', 'and', 'or']) {
    assert.strictEqual(logical.exec(word)?.[0], word);
  }
  assert.ok(!keywords.some(pattern => new RegExp(pattern.match).test('require')),
    'retired require must not be highlighted as a keyword');

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
  const parameterCases = [
    [
      'let Check = rule (x: Int) {\n    Positive(x) implies x > 0\n}\n',
      'let Check = rule (x: Int) {\n    (Positive(x)) implies {\n        x > 0\n    }\n}\n',
    ],
    [
      'let Check = rule (a: Bool, b: Bool, n: Int) {\n' +
      '    (a) implies { b }\n    a and b implies n > 0\n}\n',
      'let Check = rule (a: Bool, b: Bool, n: Int) {\n' +
      '    a implies b\n    (a and b) implies {\n        n > 0\n    }\n}\n',
    ],
    // Colon alignment is optional: preserve both compact and manually aligned lists.
    ...['function check(', 'let check = rule ('].flatMap((header) =>
      ['        x: Int,\n        maximum: Int,',
        '        x      : Int,\n        maximum: Int,'].map((parameters) => {
        const source = `${header}\n${parameters}\n) {\n}\n`;
        return [source, source];
      })),
    [
      'function expanded(\n x: Int,\n    y: Int,\n  ) {\n}\n',
      'function expanded(\n        x: Int,\n        y: Int,\n) {\n}\n',
    ],
    [
      '    let check = rule (\n// explanation\n x: List[Int],\n  y: Int,\n  ) {\n    }\n',
      '    let check = rule (\n            // explanation\n            x: List[Int],\n            y: Int,\n    ) {\n    }\n',
    ],
    [
      'let callback = (\nx: Int,\n ) -> Int {\n    return x\n}\n',
      'let callback = (\n        x: Int,\n) -> Int {\n    return x\n}\n',
    ],
    [
      '\tlet check = rule (\r\n\tx: Int,\r\n\t) {\r\n    }\r\n',
      '    let check = rule (\r\n            x: Int,\r\n    ) {\r\n    }\r\n',
    ],
    [
      'let t = types::rule(\n    types::Int,\n)\n',
      'let t = types::rule(\n    types::Int,\n)\n',
    ],
  ];
  for (const [input, expected] of parameterCases) {
    const actual = await formatSource(runtime, input, standardLibrary);
    assert.strictEqual(actual, expected);
    assert.strictEqual(await formatSource(runtime, actual, standardLibrary), actual);
  }
  const exited = new Promise((resolve) => {
    resolveExit = resolve;
  });
  const connection = new LspConnection(executable, [], { onExit: resolveExit });
  const workspace = fs.realpathSync(fs.mkdtempSync(path.join(os.tmpdir(), 'tapas-lsp-')));
  try {
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
  const cmakeSource = fs.readFileSync(
    path.resolve(__dirname, '..', '..', '..', 'CMakeLists.txt'), 'utf8');
  const projectVersion = cmakeSource.match(/project\(tapas VERSION (\d+\.\d+\.\d+)/);
  assert.ok(projectVersion, 'CMake project version must be declared');
  assert.strictEqual(initialized.serverInfo.version, projectVersion[1]);
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
    /Function add\[Int, List\[Int\]\] -> Int/);
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
    /Function pprint\[\.\.\.\] -> Nil/);
  connection.notify('textDocument/didChange', {
    textDocument: { uri: 'file:///vscode.tap', version: 4 },
    contentChanges: [{ text: 'let primes = [2, 3, 5]\nlet copied = primes.copy()\n' }],
  });
  const tunnelHover = await connection.request('textDocument/hover', {
    textDocument: { uri: 'file:///vscode.tap' },
    position: { line: 1, character: 22 },
  });
  assert.match(tunnelHover.contents.value, /Function copy\[T\] -> T/);
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
  assert.ok(moduleCompletion.some((item) => item.label === 'answer' && item.detail === 'let answer: Int'));
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
    item.label === 'answer' && item.detail === 'answer: Int'));
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
  // Structured member facts come from the shared Type model, not literals.
  let memberVersion = 10;
  async function memberRequest(text, marker, method) {
    connection.notify('textDocument/didChange', {
      textDocument: { uri: mainUri, version: memberVersion++ },
      contentChanges: [{ text }],
    });
    const end = text.indexOf(marker) + marker.length;
    assert.ok(end >= marker.length);
    const lines = text.slice(0, end).split('\n');
    return connection.request(`textDocument/${method}`, {
      textDocument: { uri: mainUri },
      position: { line: lines.length - 1, character: lines.at(-1).length },
    });
  }
  const structured = "let Inner = types::make_type('count': types::Int)\n" +
    "let State = types::make_type('status': types::String, 'inner': Inner)\n";
  const typedHover = await memberRequest(structured +
    'function inspect(state: State) { return state::status }\n',
    'state::stat', 'hover');
  assert.match(typedHover.contents.value, /status: String/);
  const nestedHover = await memberRequest(structured +
    'function inspect(state: State) { return state::inner::count }\n',
    'state::inner::cou', 'hover');
  assert.match(nestedHover.contents.value, /count: Int/);
  for (const [expression, expected] of [
    ['state::', 'status'], ['state::sta', 'status'], ['state::inner::', 'count'],
  ]) {
    const items = await memberRequest(structured +
      `function inspect(state: State) { return ${expression} }\n`,
      expression, 'completion');
    assert.ok(items.some(item => item.label === expected &&
      item.detail === `${expected}: ${expected === 'count' ? 'Int' : 'String'}`),
    `${expression}: ${JSON.stringify(items)}`);
  }
  // Keep navigation fixtures independent of the retail example's module layout.
  const modelPath = path.join(workspace, 'model.tap');
  fs.writeFileSync(modelPath,
    "let State = types::make_type('order_id': types::String, 'payment_capacity': types::Int)\n" +
    "let ExchangeParameters = types::make_type('target_item_id': types::String)\n" +
    "let ExchangeOutput = types::make_type('charged_difference': types::Int)\n" +
    'let StateSpace = rule (state: State) { true }\n' +
    'let Exchange = rule (input: ExchangeParameters) { true }\n' +
    'let ExchangeResult = rule (before: State, input: ExchangeParameters, output: ExchangeOutput, after: State) { true }\n' +
    "return {'State': State, 'ExchangeParameters': ExchangeParameters, 'ExchangeOutput': ExchangeOutput, " +
    "'StateSpace': StateSpace, 'Exchange': Exchange, 'ExchangeResult': ExchangeResult}\n");
  const imported = `import ${path.relative(workspace, modelPath).split(path.sep).join('/')} as model\n`;
  // Type presentation preserves named references without replacing the Type with source.
  const localRuleSource = structured +
    'let StateSpace = rule (state: State) { true }\nlet check = StateSpace\n';
  for (const marker of ['let StateSpa', 'check = StateSpa']) {
    const hover = await memberRequest(localRuleSource, marker, 'hover');
    assert.ok(hover);
    assert.match(hover.contents.value, /let StateSpace: Rule\[State\]/);
    assert.ok(!hover.contents.value.includes('status: String'));
  }
  for (const [name, expected] of [
    ['StateSpace', ['Rule[State]']],
    ['ExchangeResult', ['Rule[State, ExchangeParameters, ExchangeOutput, State]']],
  ]) {
    const hover = await memberRequest(imported + `let check = model::${name}\n`,
      `model::${name.slice(0, -1)}`, 'hover');
    assert.ok(hover);
    assert.ok(hover.contents.value.includes(`${name}: Rule[`));
    for (const parameter of expected) assert.ok(hover.contents.value.includes(parameter));
    assert.ok(!hover.contents.value.includes('order_id: String'));
  }
  const qualifiedRuleHover = await memberRequest(imported +
    'let Check = rule (state: model::State, actions: List[InstanceOf[model::Exchange]]) { true }\n',
    'let Chec', 'hover');
  assert.match(qualifiedRuleHover.contents.value, /Rule\[model::State, List\[InstanceOf\[model::Exchange\]\]\]/);
  const emptyRuleHover = await memberRequest('let Empty = rule { true }\n', 'let Empt', 'hover');
  assert.match(emptyRuleHover.contents.value, /let Empty: Rule\[\]/);
  const aliasSource = structured + 'let Snapshot = State\n' +
    'let Check = rule (state: State, snapshot: Snapshot, batch: List[Snapshot]) { true }\n' +
    'let Alias = Check\n';
  const aliasHover = await memberRequest(aliasSource, 'let Alia', 'hover');
  assert.match(aliasHover.contents.value, /Rule\[State, Snapshot, List\[Snapshot\]\]/);
  const namedFunctionHover = await memberRequest(structured +
    'function transform(state: State) -> List[State] { return [state] }\n',
    'function transfor', 'hover');
  assert.match(namedFunctionHover.contents.value, /Function transform\[State\] -> List\[State\]/);
  const namedParameterHover = await memberRequest(structured +
    'function transform(state: State) { return state }\n', 'return stat', 'hover');
  assert.match(namedParameterHover.contents.value, /parameter state: State/);
  const importedAliasHover = await memberRequest(imported + 'let Check = model::StateSpace\n',
    'let Chec', 'hover');
  assert.match(importedAliasHover.contents.value, /Rule\[model::State\]/);
  const stateDefinitionHover = await memberRequest(imported + 'let Shape = model::State\n',
    'model::Sta', 'hover');
  assert.match(stateDefinitionHover.contents.value, /Type State \{/);
  assert.match(stateDefinitionHover.contents.value, /order_id: String/);
  const localDefinitionHover = await memberRequest(structured, 'let Stat', 'hover');
  assert.match(localDefinitionHover.contents.value, /Type State \{/);
  assert.match(localDefinitionHover.contents.value, /status: String/);
  const ruleCompletions = await memberRequest(localRuleSource + 'let choice = StateSp\n',
    'choice = StateSp', 'completion');
  assert.ok(ruleCompletions.some(item => item.label === 'StateSpace' &&
    item.detail === 'let StateSpace: Rule[State]'));
  const privateModulePath = path.join(workspace, 'private_types.tap');
  fs.writeFileSync(privateModulePath,
    "let Hidden = types::make_type('count': types::Int)\n" +
    "let Check = rule (state: Hidden) { true }\nreturn {'Check': Check}\n");
  const privateAliasHover = await memberRequest(
    'import private_types.tap as private_model\nlet Check = private_model::Check\n',
    'let Chec', 'hover');
  assert.match(privateAliasHover.contents.value, /Rule\[\{count: Int\}\]/);
  assert.ok(!privateAliasHover.contents.value.includes('Hidden'));
  // Identical structures do not justify guessing a name for inferred values.
  const anonymousHover = await memberRequest(structured +
    "let inferred = {'status': 'ok', 'inner': {'count': 1}}\n", 'let inferre', 'hover');
  assert.match(anonymousHover.contents.value, /let inferred: Dictionary\n/);
  assert.ok(!anonymousHover.contents.value.includes('status: String'));
  assert.ok(!anonymousHover.contents.value.includes(': State'));
  const retailCatalogSource = fs.readFileSync(path.resolve(
    __dirname, '../../../examples/retail/data.tap'), 'utf8');
  for (const marker of ['let catal', "'items': catal"]) {
    const hover = await memberRequest(retailCatalogSource, marker, 'hover');
    assert.strictEqual(hover.contents.value, '```tapas\nlet catalog: Dictionary\n```');
  }
  const annotatedDictionaryHover = await memberRequest(
    "let values: Dictionary[String, Int] = {'count': 1}\n", 'let valu', 'hover');
  assert.match(annotatedDictionaryHover.contents.value, /values: Dictionary\[String, Int\]/);
  const annotatedStructureHover = await memberRequest(structured +
    "let state: State = {'status': 'ok', 'inner': {'count': 1}}\n", 'let stat', 'hover');
  assert.match(annotatedStructureHover.contents.value, /let state: State \{/);
  for (const annotation of ['InstanceOf[model::Exchange]', 'InstanceOf[; model::Exchange]']) {
    const source = imported + `function inspect(action: ${annotation}) { return action }\n`;
    const hover = await memberRequest(source, 'model::Exch', 'hover');
    assert.ok(hover, 'InstanceOf target must support hover');
    assert.match(hover.contents.value, /Exchange/);
    const definition = await memberRequest(source, 'model::Exch', 'definition');
    assert.strictEqual(definition.uri, pathToFileURL(modelPath).toString());
  }
  for (const body of [
    'let Check = rule (state: model::State) { true }\n',
    'function read(state: model::State) { return state }\n',
    'function read() -> model::State { return {} }\n',
    'let state: model::State = {}\n',
    'let values: List[model::State] = []\n',
    'function read(model: model::State) { return model }\n',
    'function read(callback: Function[model::State] -> model::State) { return callback }\n',
  ]) {
    const source = imported + body;
    const typeHover = await memberRequest(source, 'model::Sta', 'hover');
    assert.ok(typeHover, `missing annotation member hover: ${body}`);
    assert.match(typeHover.contents.value, /Type State/);
    const typeDefinition = await memberRequest(source, 'model::Sta', 'definition');
    assert.strictEqual(typeDefinition.uri, pathToFileURL(modelPath).toString());
    const namespaceMarker = source.slice(0, source.indexOf('model::') + 3);
    const namespaceHover = await memberRequest(source, namespaceMarker, 'hover');
    assert.match(namespaceHover.contents.value, /package model/);
    const namespaceDefinition = await memberRequest(source, namespaceMarker, 'definition');
    assert.strictEqual(namespaceDefinition.uri, pathToFileURL(modelPath).toString());
  }
  const localTypeHover = await memberRequest(
    "let Local = types::make_type('count': types::Int)\n" +
    'function read(Local: Local) { return Local }\n', ': Loc', 'hover');
  assert.match(localTypeHover.contents.value, /Type Local \{/);
  const builtinTypeHover = await memberRequest('let count: Int = 1\n', ': In', 'hover');
  assert.ok(builtinTypeHover, 'builtin Type annotation hover missing');
  const annotationSource = imported +
    'let first: model::State = {}\nlet second: model::State = {}\n';
  await memberRequest(annotationSource, 'model::Sta', 'hover');
  const annotationReferences = await connection.request('textDocument/references', {
    textDocument: { uri: mainUri }, position: { line: 1, character: 19 },
    context: { includeDeclaration: false },
  });
  assert.strictEqual(annotationReferences.filter(item => item.uri === mainUri).length, 2);
  const annotationRename = await connection.request('textDocument/rename', {
    textDocument: { uri: mainUri }, position: { line: 1, character: 19 }, newName: 'OrderState',
  });
  assert.strictEqual(annotationRename.changes[mainUri].length, 2);
  const aliasRename = await connection.request('textDocument/rename', {
    textDocument: { uri: mainUri }, position: { line: 1, character: 13 }, newName: 'retail_model',
  });
  assert.strictEqual(aliasRename.changes[mainUri].length, 3);
  // Updating a document replaces its index, including unresolved/incomplete annotations.
  const missingTypeHover = await memberRequest(imported +
    'let value: model::Missing = {}\n', 'model::Miss', 'hover');
  assert.strictEqual(missingTypeHover, null);
  const importedHover = await memberRequest(imported +
    'let Check = rule (state: model::State) { state::payment_capacity >= 0 }\n',
    'state::payment_cap', 'hover');
  assert.match(importedHover.contents.value, /payment_capacity: Int/);
  const importedItems = await memberRequest(imported +
    'let Check = rule (state: model::State) { state::pay }\n',
    'state::pay', 'completion');
  assert.ok(importedItems.some(item => item.label === 'payment_capacity' && item.detail === 'payment_capacity: Int'));
  assert.ok(!importedItems.some(item => item.label === 'status'));
  const argumentItems = await memberRequest('let result = argu\n', 'argu', 'completion');
  assert.ok(argumentItems.some(item => item.label === 'arguments' &&
    item.detail.includes('RuleInstance')));
  const parameterItems = await memberRequest('let result = para\n', 'para', 'completion');
  assert.ok(parameterItems.some(item => item.label === 'parameters' &&
    item.detail.includes('Pair[String, Type]')));
  // The showcase must remain valid under the same frontend used by the CLI.
  for (const name of ['model', 'model_defs', 'data', 'simulation',
    'test_valid_exchange', 'test_rejected_exchange', 'test_invalid_transition',
    'test_generate_valid_exchange', 'test_generate_rejected_exchange', 'test_generate_joint_inputs',
    'rule_implies', 'rule_instance_implies', 'rule_not', 'rule_logic']) {
    const file = path.resolve(__dirname, name.startsWith('rule_')
      ? '../../../test/language_rules/regression' : '../../../examples/retail', `${name}.tap`);
    const uri = pathToFileURL(file).toString();
    const received = new Promise((resolve, reject) => {
      const timer = setTimeout(() => reject(new Error(`No diagnostics for ${name}`)), 5000);
      connection.onNotification('textDocument/publishDiagnostics', (result) => {
        if (result.uri !== uri) return;
        clearTimeout(timer);
        resolve(result.diagnostics);
      });
    });
    connection.notify('textDocument/didOpen', {
      textDocument: { uri, languageId: 'tapas', version: 1, text: fs.readFileSync(file, 'utf8') },
    });
    assert.deepStrictEqual(await received, [], `${name}.tap should have no diagnostics`);
  }
  await connection.stop();
  const exitInfo = await exited;
  assert.strictEqual(exitInfo.expected, true);
  } finally {
    await connection.stop();
    fs.rmSync(workspace, { recursive: true, force: true });
  }
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
