// =============================================================================
//  uart.js — Kenh chinh: Web Serial API.
//
//  Yeu cau: Chrome hoac Edge, va trang phai chay trong ngu canh bao mat. Do la
//  ly do phai mo qua http://localhost:8080 (xem serve.sh) chu khong phai mo
//  thang file:// — file:// cung chan luon camera quet QR.
// =============================================================================
import { LineSplitter } from '../protocol.js';

export class UartTransport {
  name = 'uart';

  #port = null;
  #reader = null;
  #writer = null;
  #splitter = new LineSplitter();
  #onLine = null;
  #onClosed = null;
  #readLoop = null;

  static isSupported() {
    return 'serial' in navigator;
  }

  constructor({ onLine, onClosed }) {
    this.#onLine = onLine;
    this.#onClosed = onClosed;
  }

  get isOpen() { return this.#port !== null; }

  /** Mo hop thoai cho nguoi dung chon cong. Phai goi tu mot cu bam chuot. */
  async open({ baudRate = 115200 } = {}) {
    if (!UartTransport.isSupported()) {
      throw new Error('Trình duyệt này không hỗ trợ Web Serial. Hãy dùng Chrome hoặc Edge.');
    }
    this.#port = await navigator.serial.requestPort();
    await this.#port.open({ baudRate, dataBits: 8, stopBits: 1, parity: 'none' });

    this.#writer = this.#port.writable.getWriter();
    this.#splitter.reset();
    this.#readLoop = this.#pump();

    return this.describe();
  }

  describe() {
    const info = this.#port?.getInfo?.() ?? {};
    if (info.usbVendorId === undefined) return 'Cổng nối tiếp';
    const hex = (n) => n.toString(16).padStart(4, '0').toUpperCase();
    return `USB ${hex(info.usbVendorId)}:${hex(info.usbProductId)}`;
  }

  async #pump() {
    const decoder = new TextDecoderStream();
    const closed = this.#port.readable.pipeTo(decoder.writable).catch(() => {});
    this.#reader = decoder.readable.getReader();

    try {
      for (;;) {
        const { value, done } = await this.#reader.read();
        if (done) break;
        for (const line of this.#splitter.push(value)) this.#onLine?.(line);
      }
    } catch {
      // Rut cap giua chung se nem loi o day — do la duong dan binh thuong.
    } finally {
      await closed;
      this.#onClosed?.();
    }
  }

  async send(text) {
    if (!this.#writer) return false;
    try {
      await this.#writer.write(new TextEncoder().encode(text));
      return true;
    } catch {
      return false;   // cap vua bi rut
    }
  }

  async close() {
    const port = this.#port;
    this.#port = null;
    try { await this.#reader?.cancel(); } catch { /* dang dong */ }
    try { this.#writer?.releaseLock(); } catch { /* dang dong */ }
    this.#writer = null;
    try { await port?.close(); } catch { /* dang dong */ }
  }
}
