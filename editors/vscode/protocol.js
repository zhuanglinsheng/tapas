'use strict';

const { spawn } = require('child_process');

class LspConnection {
  constructor(command, args = [], options = {}) {
    this.command = command;
    this.args = args;
    this.cwd = options.cwd;
    this.onStderr = options.onStderr || (() => {});
    this.onExit = options.onExit || (() => {});
    this.process = undefined;
    this.buffer = Buffer.alloc(0);
    this.nextId = 1;
    this.pending = new Map();
    this.notifications = new Map();
    this.stopping = false;
  }

  start() {
    if (this.process) return Promise.resolve();
    return new Promise((resolve, reject) => {
      const child = spawn(this.command, this.args, {
        cwd: this.cwd,
        stdio: ['pipe', 'pipe', 'pipe'],
        windowsHide: true,
      });
      let settled = false;
      this.stopping = false;
      const fail = (error) => {
        if (!settled) {
          settled = true;
          reject(error);
        }
      };
      child.once('error', fail);
      child.once('spawn', () => {
        settled = true;
        this.process = child;
        resolve();
      });
      child.stdout.on('data', (chunk) => this._accept(chunk));
      child.stderr.on('data', (chunk) => this.onStderr(chunk.toString('utf8')));
      child.on('exit', (code, signal) => {
        if (this.process === child) this.process = undefined;
        const expected = this.stopping;
        const error = new Error(expected ?
          'Tapas language server stopped' :
          `Tapas language server exited (${code ?? signal})`);
        error.code = 'TAPAS_SERVER_EXIT';
        error.expected = expected;
        for (const request of this.pending.values()) {
          clearTimeout(request.timer);
          request.reject(error);
        }
        this.pending.clear();
        this.onExit({ code, signal, expected });
      });
    });
  }

  onNotification(method, handler) {
    this.notifications.set(method, handler);
  }

  notify(method, params) {
    this._send({ jsonrpc: '2.0', method, params });
  }

  request(method, params, timeoutMs = 10000) {
    const id = this.nextId++;
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(id);
        reject(new Error(`Tapas language server request timed out: ${method}`));
      }, timeoutMs);
      this.pending.set(id, { resolve, reject, timer });
      this._send({ jsonrpc: '2.0', id, method, params });
    });
  }

  async stop() {
    const child = this.process;
    if (!child) return;
    this.stopping = true;
    try {
      await this.request('shutdown', null, 2000);
      this.notify('exit', null);
    } catch (_) {
      child.kill();
      return;
    }
    setTimeout(() => {
      if (this.process === child) child.kill();
    }, 1000).unref();
  }

  _send(message) {
    if (!this.process || !this.process.stdin.writable) {
      const error = new Error('Tapas language server is not running');
      error.code = 'TAPAS_SERVER_EXIT';
      throw error;
    }
    const body = Buffer.from(JSON.stringify(message), 'utf8');
    const header = Buffer.from(`Content-Length: ${body.length}\r\n\r\n`, 'ascii');
    this.process.stdin.write(Buffer.concat([header, body]));
  }

  _accept(chunk) {
    this.buffer = Buffer.concat([this.buffer, chunk]);
    while (true) {
      const separator = this.buffer.indexOf('\r\n\r\n');
      if (separator < 0) return;
      const header = this.buffer.subarray(0, separator).toString('ascii');
      const match = /(?:^|\r\n)Content-Length:\s*(\d+)/i.exec(header);
      if (!match) {
        this.buffer = this.buffer.subarray(separator + 4);
        continue;
      }
      const length = Number(match[1]);
      const bodyStart = separator + 4;
      if (this.buffer.length < bodyStart + length) return;
      const body = this.buffer.subarray(bodyStart, bodyStart + length).toString('utf8');
      this.buffer = this.buffer.subarray(bodyStart + length);
      try {
        this._dispatch(JSON.parse(body));
      } catch (error) {
        this.onStderr(`Invalid language server message: ${error.message}\n`);
      }
    }
  }

  _dispatch(message) {
    if (Object.prototype.hasOwnProperty.call(message, 'id') &&
        (Object.prototype.hasOwnProperty.call(message, 'result') || message.error)) {
      const request = this.pending.get(message.id);
      if (!request) return;
      this.pending.delete(message.id);
      clearTimeout(request.timer);
      if (message.error) {
        const error = new Error(message.error.message || 'Language server error');
        error.code = message.error.code;
        request.reject(error);
      } else {
        request.resolve(message.result);
      }
      return;
    }
    if (message.method) {
      const handler = this.notifications.get(message.method);
      if (handler) handler(message.params);
    }
  }
}

module.exports = { LspConnection };
