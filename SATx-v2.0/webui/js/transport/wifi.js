// =============================================================================
//  wifi.js — Kenh du phong: WebSocket toi ESP32.
//
//  Dia chi mac dinh la ten mDNS satx.local nen khong phai nho IP. macOS va
//  Windows 10 tro len phan giai duoc ten nay san; Linux can cai avahi.
// =============================================================================
import { LineSplitter } from '../protocol.js';

export class WifiTransport {
  name = 'wifi';

  #socket = null;
  #splitter = new LineSplitter();
  #onLine = null;
  #onClosed = null;
  #host = '';

  constructor({ onLine, onClosed }) {
    this.#onLine = onLine;
    this.#onClosed = onClosed;
  }

  get isOpen() { return this.#socket?.readyState === WebSocket.OPEN; }

  open({ host = 'satx.local', port = 81, timeoutMs = 4000 } = {}) {
    this.#host = `${host}:${port}`;
    this.#splitter.reset();

    return new Promise((resolve, reject) => {
      let settled = false;
      const socket = new WebSocket(`ws://${this.#host}/`);
      this.#socket = socket;

      const timer = setTimeout(() => {
        if (settled) return;
        settled = true;
        socket.close();
        reject(new Error(`Không kết nối được tới ${this.#host}`));
      }, timeoutMs);

      socket.onopen = () => {
        if (settled) return;
        settled = true;
        clearTimeout(timer);
        resolve(this.describe());
      };

      socket.onmessage = (event) => {
        for (const line of this.#splitter.push(String(event.data))) this.#onLine?.(line);
      };

      socket.onclose = () => {
        clearTimeout(timer);
        if (this.#socket === socket) this.#socket = null;
        if (!settled) { settled = true; reject(new Error(`Mất kết nối tới ${this.#host}`)); }
        else this.#onClosed?.();
      };

      socket.onerror = () => { /* onclose se xu ly */ };
    });
  }

  describe() { return this.#host; }

  send(text) {
    if (!this.isOpen) return false;
    this.#socket.send(text);
    return true;
  }

  async close() {
    const socket = this.#socket;
    this.#socket = null;
    socket?.close();
  }
}
