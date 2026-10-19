// =============================================================================
//  link.js — Uu tien cap USB, tu chuyen sang WiFi khi cap rot.
//
//  Doi xung voi LinkManager ben firmware:
//
//      Gui {"t":"ping"} moi 1 giay. Khong thay {"t":"pong"} trong 3 giay thi coi
//      nhu kenh hien tai da chet:
//        · dang dung UART -> thu mo WebSocket toi satx.local
//        · dang dung WiFi -> bao mat ket noi, nhung van de nguoi dung cam lai cap
//
//      Cam lai cap: nguoi dung bam "Kết nối lại" (trinh duyet KHONG cho tu dong
//      mo cong nghe noi tiep neu khong co cu bam chuot — day la rang buoc bao
//      mat cua Web Serial, khong phai thieu sot cua thiet ke).
// =============================================================================
import { Cmd, encode, decode } from '../protocol.js';
import { UartTransport } from './uart.js';
import { WifiTransport } from './wifi.js';
import { MockTransport } from './mock.js';

const PING_INTERVAL_MS = 1000;
const PONG_TIMEOUT_MS  = 3000;

export class Link {
  #transport = null;
  #onMessage = null;
  #onStatus = null;
  #pingTimer = null;
  #lastPongAt = 0;
  #wifiConfig = { host: 'satx.local', port: 81 };
  #failingOver = false;

  constructor({ onMessage, onStatus }) {
    this.#onMessage = onMessage;
    this.#onStatus = onStatus;
  }

  get kind() { return this.#transport?.name ?? 'none'; }
  get isOpen() { return this.#transport?.isOpen === true; }
  get mock() { return this.#transport instanceof MockTransport ? this.#transport : null; }

  setWifiConfig(host, port) {
    this.#wifiConfig = { host: host || 'satx.local', port: Number(port) || 81 };
  }

  /** kind: 'uart' | 'wifi' | 'mock' */
  async connect(kind, options = {}) {
    await this.disconnect();

    const hooks = {
      onLine: (line) => this.#receive(line),
      onClosed: () => this.#onTransportClosed(),
    };

    if (kind === 'uart')      this.#transport = new UartTransport(hooks);
    else if (kind === 'wifi') this.#transport = new WifiTransport(hooks);
    else                      this.#transport = new MockTransport(hooks);

    const label = kind === 'wifi'
      ? await this.#transport.open({ ...this.#wifiConfig, ...options })
      : await this.#transport.open(options);

    this.#lastPongAt = Date.now();
    this.#startHeartbeat();
    this.#onStatus?.({ kind, label, connected: true });

    // Dong bo dong ho ngay khi bat tay: thiet bi khong co RTC nuoi pin.
    this.send(Cmd.sync(Math.floor(Date.now() / 1000)));
    return label;
  }

  async disconnect() {
    this.#stopHeartbeat();
    const transport = this.#transport;
    this.#transport = null;
    await transport?.close();
    if (transport) this.#onStatus?.({ kind: 'none', label: '', connected: false });
  }

  send(command) {
    if (!this.#transport) return false;
    return this.#transport.send(encode(command)) !== false;
  }

  // --- Noi bo -------------------------------------------------------------

  #receive(line) {
    const message = decode(line);
    if (!message) return;   // log bootloader va nhieu duong truyen
    if (message.t === 'pong') {
      this.#lastPongAt = Date.now();
      this.#failingOver = false;
    }
    this.#onMessage?.(message);
  }

  #startHeartbeat() {
    this.#stopHeartbeat();
    this.#pingTimer = setInterval(() => {
      this.send(Cmd.ping());
      if (Date.now() - this.#lastPongAt > PONG_TIMEOUT_MS) this.#handleSilence();
    }, PING_INTERVAL_MS);
  }

  #stopHeartbeat() {
    if (this.#pingTimer !== null) clearInterval(this.#pingTimer);
    this.#pingTimer = null;
  }

  async #handleSilence() {
    if (this.#failingOver) return;
    this.#failingOver = true;

    if (this.kind === 'uart') {
      this.#onStatus?.({
        kind: 'uart', connected: false, failingOver: true,
        message: 'Mất tín hiệu qua cáp — đang chuyển sang WiFi…',
      });
      try {
        await this.connect('wifi');
        this.#onStatus?.({
          kind: 'wifi', connected: true, label: this.#transport.describe(),
          message: 'Đã chuyển sang WiFi. Cắm lại cáp để quay về kênh chính.',
        });
      } catch (error) {
        this.#failingOver = false;
        this.#onStatus?.({
          kind: 'none', connected: false,
          message: `Mất kết nối hoàn toàn. Máy sẽ tự lưu vào bộ đệm. (${error.message})`,
        });
      }
      return;
    }

    this.#onStatus?.({
      kind: 'none', connected: false,
      message: 'Mất kết nối hoàn toàn. Máy sẽ tự lưu kết quả vào bộ đệm.',
    });
  }

  #onTransportClosed() {
    this.#onStatus?.({ kind: 'none', connected: false, message: 'Kênh truyền đã đóng.' });
    this.#handleSilence();
  }
}
