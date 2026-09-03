'use strict';

const vscode = require('vscode');
const fs = require('fs');
const path = require('path');
const { LspConnection } = require('./protocol');
const { formatSource } = require('./formatter');

let client;

function position(value) {
  return new vscode.Position(value.line, value.character);
}

function range(value) {
  return new vscode.Range(position(value.start), position(value.end));
}

function textDocument(document) {
  return { uri: document.uri.toString() };
}

function requestPosition(document, at) {
  return {
    textDocument: textDocument(document),
    position: { line: at.line, character: at.character },
  };
}

function serverCommand(context) {
  const configured = vscode.workspace.getConfiguration('tapas')
    .get('languageServer.path', '').trim();
  if (configured) return configured;
  const candidates = [];
  if (context.extensionMode === vscode.ExtensionMode.Development)
    candidates.push(path.resolve(context.extensionPath, '..', '..', 'build', 'bin', 'tapas-language-server'));
  for (const folder of vscode.workspace.workspaceFolders || []) {
    candidates.push(path.join(folder.uri.fsPath, 'build', 'bin', 'tapas-language-server'));
  }
  for (const candidate of candidates) {
    if (fs.existsSync(candidate)) return candidate;
  }
  return 'tapas-language-server';
}

function runtimeCommand(context, document) {
  const configured = vscode.workspace.getConfiguration('tapas')
    .get('runtime.path', '').trim();
  if (configured) return configured;
  const candidates = [];
  if (context.extensionMode === vscode.ExtensionMode.Development)
    candidates.push(path.resolve(context.extensionPath, '..', '..', 'build', 'bin', 'tapas'));
  const owner = document ? vscode.workspace.getWorkspaceFolder(document.uri) : undefined;
  if (owner) candidates.push(path.join(owner.uri.fsPath, 'build', 'bin', 'tapas'));
  for (const folder of vscode.workspace.workspaceFolders || [])
    candidates.push(path.join(folder.uri.fsPath, 'build', 'bin', 'tapas'));
  for (const candidate of candidates) {
    if (fs.existsSync(candidate)) return candidate;
  }
  return 'tapas';
}

async function runCurrentFile(context) {
  const document = vscode.window.activeTextEditor?.document;
  if (!document || document.languageId !== 'tapas') {
    vscode.window.showWarningMessage('Open a Tapas file before running it.');
    return;
  }
  if (document.isUntitled || document.uri.scheme !== 'file') {
    vscode.window.showWarningMessage('Save the Tapas file before running it.');
    return;
  }
  if (document.isDirty && !await document.save()) return;

  const executable = runtimeCommand(context, document);
  const folder = vscode.workspace.getWorkspaceFolder(document.uri);
  const args = folder ? ['-p', folder.uri.fsPath, document.uri.fsPath] :
    [document.uri.fsPath];
  const execution = new vscode.ProcessExecution(executable, args, {
    cwd: path.dirname(document.uri.fsPath),
  });
  const task = new vscode.Task(
    { type: 'tapas', file: document.uri.fsPath },
    folder || vscode.TaskScope.Workspace,
    `Run ${path.basename(document.uri.fsPath)}`, 'Tapas', execution);
  task.presentationOptions = {
    reveal: vscode.TaskRevealKind.Always,
    panel: vscode.TaskPanelKind.Dedicated,
    clear: true,
    focus: false,
  };
  await vscode.tasks.executeTask(task);
}

async function formatDocument(context, document) {
  const executable = runtimeCommand(context, document);
  const original = document.getText();
  const formatted = await formatSource(executable, original);
  if (formatted === original) return [];
  const entireDocument = new vscode.Range(
    document.positionAt(0), document.positionAt(original.length));
  return [vscode.TextEdit.replace(entireDocument, formatted)];
}

function symbolKind(kind) {
  if (kind === 3) return vscode.SymbolKind.Namespace;
  if (kind === 12) return vscode.SymbolKind.Function;
  return vscode.SymbolKind.Variable;
}

function completionKind(kind) {
  if (kind === 3) return vscode.CompletionItemKind.Function;
  if (kind === 7) return vscode.CompletionItemKind.Class;
  if (kind === 9) return vscode.CompletionItemKind.Module;
  return vscode.CompletionItemKind.Variable;
}

class TapasClient {
  constructor(context) {
    this.context = context;
    this.output = vscode.window.createOutputChannel('Tapas Language Server');
    this.diagnostics = vscode.languages.createDiagnosticCollection('tapas');
    this.disposed = false;
    this.restartPromise = undefined;
    this.connection = new LspConnection(serverCommand(context), [], {
      onStderr: (text) => this.output.append(text),
      onExit: (info) => this.handleServerExit(info),
    });
    this.disposables = [this.output, this.diagnostics];
  }

  async start() {
    this.connection.onNotification('textDocument/publishDiagnostics',
      (params) => this.publishDiagnostics(params));
    await this.connection.start();
    await this.initializeServer();
    this.registerDocuments();
    this.registerProviders();
    this.reopenDocuments();
  }

  async initializeServer() {
    const root = vscode.workspace.workspaceFolders?.[0]?.uri.toString() || null;
    const workspaceFolders = (vscode.workspace.workspaceFolders || []).map((folder) => ({
      uri: folder.uri.toString(), name: folder.name,
    }));
    const initialized = await this.connection.request('initialize', {
      processId: process.pid,
      rootUri: root,
      workspaceFolders,
      capabilities: {
        general: { positionEncodings: ['utf-16'] },
        textDocument: {
          hover: { contentFormat: ['markdown', 'plaintext'] },
          definition: {}, references: {}, documentSymbol: {}, completion: {}, rename: {},
          semanticTokens: { requests: { full: true }, tokenTypes: [], tokenModifiers: [] },
          publishDiagnostics: { versionSupport: true },
        },
      },
      clientInfo: {
        name: 'Tapas VS Code',
        version: this.context.extension.packageJSON.version,
      },
    });
    const semantic = initialized.capabilities?.semanticTokensProvider?.legend;
    if (semantic) this.semanticLegend = new vscode.SemanticTokensLegend(
      semantic.tokenTypes || [], semantic.tokenModifiers || []);
    this.connection.notify('initialized', {});
  }

  reopenDocuments() {
    for (const document of vscode.workspace.textDocuments) {
      if (document.languageId === 'tapas') this.open(document);
    }
  }

  handleServerExit(info) {
    if (this.disposed || info.expected) return;
    this.output.appendLine(
      `Tapas language server exited (${info.code ?? info.signal}); restarting...`);
    void this.restart().catch((error) => {
      if (!this.disposed) this.output.appendLine(
        `Tapas language server restart failed: ${error.stack || error}`);
    });
  }

  restart() {
    if (this.disposed) return Promise.resolve(false);
    if (this.restartPromise) return this.restartPromise;
    this.restartPromise = new Promise((resolve) => setTimeout(resolve, 250))
      .then(async () => {
        if (this.disposed) return false;
        await this.connection.start();
        await this.initializeServer();
        this.reopenDocuments();
        this.output.appendLine('Tapas language server restarted.');
        return true;
      })
      .finally(() => {
        this.restartPromise = undefined;
      });
    return this.restartPromise;
  }

  async request(method, params, fallback) {
    try {
      return await this.connection.request(method, params);
    } catch (error) {
      if (error.code !== 'TAPAS_SERVER_EXIT') throw error;
      if (!await this.restart()) return fallback;
      try {
        return await this.connection.request(method, params);
      } catch (retryError) {
        if (retryError.code !== 'TAPAS_SERVER_EXIT') throw retryError;
        return fallback;
      }
    }
  }

  notify(method, params) {
    if (this.disposed) return;
    try {
      this.connection.notify(method, params);
    } catch (error) {
      if (error.code !== 'TAPAS_SERVER_EXIT') throw error;
      void this.restart().catch((restartError) => {
        if (!this.disposed) this.output.appendLine(
          `Tapas language server restart failed: ${restartError.stack || restartError}`);
      });
    }
  }

  registerDocuments() {
    const watcher = vscode.workspace.createFileSystemWatcher('**/*.tap');
    const changed = (uri, type) => this.notify(
      'workspace/didChangeWatchedFiles', { changes: [{ uri: uri.toString(), type }] });
    this.disposables.push(
      watcher,
      watcher.onDidCreate((uri) => changed(uri, 1)),
      watcher.onDidChange((uri) => changed(uri, 2)),
      watcher.onDidDelete((uri) => changed(uri, 3)),
      vscode.workspace.onDidOpenTextDocument((document) => {
        if (document.languageId === 'tapas') this.open(document);
      }),
      vscode.workspace.onDidChangeTextDocument((event) => {
        if (event.document.languageId !== 'tapas') return;
        this.notify('textDocument/didChange', {
          textDocument: {
            uri: event.document.uri.toString(),
            version: event.document.version,
          },
          contentChanges: [{ text: event.document.getText() }],
        });
      }),
      vscode.workspace.onDidCloseTextDocument((document) => {
        if (document.languageId !== 'tapas') return;
        this.notify('textDocument/didClose', {
          textDocument: textDocument(document),
        });
        this.diagnostics.delete(document.uri);
      }),
    );
  }

  registerProviders() {
    const selector = [{ language: 'tapas', scheme: 'file' }, { language: 'tapas', scheme: 'untitled' }];
    const providers = [
      vscode.languages.registerHoverProvider(selector, {
        provideHover: async (document, at) => {
          const result = await this.request('textDocument/hover', requestPosition(document, at), null);
          if (!result) return undefined;
          const value = typeof result.contents === 'string' ? result.contents : result.contents.value;
          return new vscode.Hover(new vscode.MarkdownString(value), result.range ? range(result.range) : undefined);
        },
      }),
      vscode.languages.registerDefinitionProvider(selector, {
        provideDefinition: async (document, at) => {
          const result = await this.request('textDocument/definition', requestPosition(document, at), null);
          return result ? new vscode.Location(vscode.Uri.parse(result.uri), range(result.range)) : undefined;
        },
      }),
      vscode.languages.registerReferenceProvider(selector, {
        provideReferences: async (document, at, options) => {
          const params = requestPosition(document, at);
          params.context = { includeDeclaration: options.includeDeclaration };
          const result = await this.request('textDocument/references', params, []);
          return (result || []).map((item) => new vscode.Location(vscode.Uri.parse(item.uri), range(item.range)));
        },
      }),
      vscode.languages.registerDocumentSymbolProvider(selector, {
        provideDocumentSymbols: async (document) => {
          const result = await this.request('textDocument/documentSymbol', {
            textDocument: textDocument(document),
          }, []);
          return (result || []).map((item) => new vscode.DocumentSymbol(
            item.name, item.detail || '', symbolKind(item.kind), range(item.range), range(item.selectionRange)));
        },
      }),
      vscode.languages.registerWorkspaceSymbolProvider({
        provideWorkspaceSymbols: async (query) => {
          const result = await this.request('workspace/symbol', { query }, []);
          return (result || []).map((item) => new vscode.SymbolInformation(
            item.name, symbolKind(item.kind), '', new vscode.Location(
              vscode.Uri.parse(item.location.uri), range(item.location.range))));
        },
      }),
      vscode.languages.registerCompletionItemProvider(selector, {
        provideCompletionItems: async (document, at) => {
          const result = await this.request('textDocument/completion', requestPosition(document, at), []);
          return (result || []).map((item) => {
            const completion = new vscode.CompletionItem(item.label, completionKind(item.kind));
            completion.detail = item.detail;
            return completion;
          });
        },
      }, ':'),
      vscode.languages.registerRenameProvider(selector, {
        prepareRename: async (document, at) => {
          const result = await this.request('textDocument/prepareRename', requestPosition(document, at), null);
          if (!result) throw new Error('This symbol cannot be renamed');
          return { range: range(result.range), placeholder: result.placeholder };
        },
        provideRenameEdits: async (document, at, newName) => {
          const params = requestPosition(document, at);
          params.newName = newName;
          const result = await this.request('textDocument/rename', params, null);
          if (!result) return undefined;
          const edit = new vscode.WorkspaceEdit();
          for (const [uri, edits] of Object.entries(result.changes || {})) {
            const target = vscode.Uri.parse(uri);
            for (const item of edits) edit.replace(target, range(item.range), item.newText);
          }
          return edit;
        },
      }),
    ];
    if (this.semanticLegend) providers.push(
      vscode.languages.registerDocumentSemanticTokensProvider(selector, {
        provideDocumentSemanticTokens: async (document) => {
          const result = await this.request('textDocument/semanticTokens/full', {
            textDocument: textDocument(document),
          }, { data: [] });
          return new vscode.SemanticTokens(Uint32Array.from(result?.data || []));
        },
      }, this.semanticLegend));
    this.disposables.push(...providers);
  }

  open(document) {
    this.notify('textDocument/didOpen', {
      textDocument: {
        uri: document.uri.toString(),
        languageId: 'tapas',
        version: document.version,
        text: document.getText(),
      },
    });
  }

  publishDiagnostics(params) {
    const uri = vscode.Uri.parse(params.uri);
    const items = (params.diagnostics || []).map((item) => {
      const severity = item.severity === 1 ? vscode.DiagnosticSeverity.Error :
        item.severity === 2 ? vscode.DiagnosticSeverity.Warning :
          item.severity === 3 ? vscode.DiagnosticSeverity.Information : vscode.DiagnosticSeverity.Hint;
      const diagnostic = new vscode.Diagnostic(range(item.range), item.message, severity);
      diagnostic.source = item.source || 'tapas';
      diagnostic.code = item.code;
      return diagnostic;
    });
    this.diagnostics.set(uri, items);
  }

  async dispose() {
    if (this.disposed) return;
    this.disposed = true;
    for (const disposable of this.disposables.splice(0)) disposable.dispose();
    await this.connection.stop();
  }
}

async function activate(context) {
  const selector = [
    { language: 'tapas', scheme: 'file' },
    { language: 'tapas', scheme: 'untitled' },
  ];
  context.subscriptions.push(
    vscode.commands.registerCommand(
      'tapas.runFile', () => runCurrentFile(context).catch((error) =>
        vscode.window.showErrorMessage(`Could not run Tapas file: ${error.message}`))),
    vscode.commands.registerCommand(
      'tapas.formatDocument', () => vscode.commands.executeCommand(
        'editor.action.formatDocument')),
    vscode.languages.registerDocumentFormattingEditProvider(selector, {
      provideDocumentFormattingEdits: (document) =>
        formatDocument(context, document),
    }),
  );
  client = new TapasClient(context);
  try {
    await client.start();
    context.subscriptions.push({ dispose: () => client?.dispose() });
  } catch (error) {
    client.output.appendLine(error.stack || String(error));
    client.output.show(true);
    vscode.window.showErrorMessage(
      `Tapas Language Server could not start: ${error.message}. ` +
      'Install Tapas Core or set tapas.languageServer.path.');
  }
}

async function deactivate() {
  const active = client;
  client = undefined;
  if (active) await active.dispose();
}

module.exports = { activate, deactivate };
