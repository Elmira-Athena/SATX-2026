import test from 'node:test';
import assert from 'node:assert/strict';
import { Link } from '../js/transport/link.js';
import { Cmd } from '../js/protocol.js';

test('WiFi credentials must report a failed asynchronous USB write', async (t) => {
  let failWrite = false;
  const writes = [];
  const port = {
    readable: new ReadableStream(),
    writable: new WritableStream({
      write(bytes) {
        if (failWrite) throw new Error('USB disconnected');
        writes.push(new TextDecoder().decode(bytes));
      },
    }),
    async open() {},
    async close() {},
    getInfo() { return {}; },
  };
  const originalNavigator = Object.getOwnPropertyDescriptor(globalThis, 'navigator');
  Object.defineProperty(globalThis, 'navigator', {
    configurable: true, value: { serial: { requestPort: async () => port } },
  });
  const link = new Link({});
  t.after(async () => {
    await link.disconnect();
    if (originalNavigator) Object.defineProperty(globalThis, 'navigator', originalNavigator);
    else delete globalThis.navigator;
  });

  assert.equal(await link.send(Cmd.wifi('qa-network', 'dummy-password')), false);
  await link.connect('uart');
  assert.equal(await link.send(Cmd.wifi('qa-network', 'dummy-password')), true);
  await new Promise(setImmediate);
  assert.deepEqual(JSON.parse(writes.at(-1)), {
    t: 'cfg', wifi: { ssid: 'qa-network', pass: 'dummy-password' },
  });
  failWrite = true;
  assert.equal(await link.send(Cmd.wifi('qa-network', 'dummy-password')), false);
});

test('closing an old connection must not disconnect the replacement WiFi link', async (t) => {
  const OriginalWebSocket = globalThis.WebSocket;
  const sockets = [];
  globalThis.WebSocket = class {
    static OPEN = 1;
    readyState = 0;
    constructor() {
      sockets.push(this);
      queueMicrotask(() => { this.readyState = 1; this.onopen?.(); });
    }
    send() {}
    close() { this.readyState = 3; }
  };
  const statuses = [];
  const link = new Link({ onStatus: status => statuses.push(status) });
  t.after(async () => { await link.disconnect(); globalThis.WebSocket = OriginalWebSocket; });
  await link.connect('wifi');
  await link.connect('wifi');
  statuses.length = 0;
  sockets[0].onclose();
  assert.equal(link.isOpen, true);
  assert.deepEqual(statuses, []);

  await link.disconnect();
  statuses.length = 0;
  sockets[1].onclose();
  assert.deepEqual(statuses, []);
});

test('WiFi reconnects after ESP32 restarts and stops retrying after Disconnect', async (t) => {
  t.mock.timers.enable({ apis: ['setInterval', 'Date'], now: 10000 });
  const OriginalWebSocket = globalThis.WebSocket;
  const sockets = [];
  let online = true;
  globalThis.WebSocket = class {
    static OPEN = 1;
    readyState = 0;
    constructor() {
      sockets.push(this);
      queueMicrotask(() => {
        if (online) { this.readyState = 1; this.onopen?.(); }
        else { this.readyState = 3; this.onclose?.(); }
      });
    }
    send() {}
    close() { this.readyState = 3; }
  };
  const link = new Link({});
  t.after(async () => { await link.disconnect(); globalThis.WebSocket = OriginalWebSocket; });
  await link.connect('wifi');
  online = false;
  sockets[0].readyState = 3;
  sockets[0].onclose();
  await new Promise(setImmediate);
  assert.equal(sockets.length, 2, 'Must try reconnecting when the ESP32 closes WiFi');
  online = true;
  t.mock.timers.tick(5000);
  await new Promise(setImmediate);
  assert.equal(link.isOpen, true, 'Must reconnect once the ESP32 is back');
  assert.equal(sockets.length, 3);
  await link.disconnect();
  t.mock.timers.tick(10000);
  await new Promise(setImmediate);
  assert.equal(sockets.length, 3, 'Manual Disconnect must cancel automatic retries');
});
