// =============================================================================
//  mock.js — Thiet bi gia lap, chay hoan toan trong trinh duyet.
//
//  Muc dich: chay thu tron ven giao dien khi chua co mach. No noi CHINH XAC
//  giao thuc JSON-lines nhu firmware that, ke ca:
//    · phat {"t":"state"} 4 lan moi giay trong luc do,
//    · gui pong kem so ban ghi con trong bo dem,
//    · mo phong mat ket noi -> luu vao bo dem -> noi lai -> day len roi xoa.
//
//  Nho vay khi cam board that vao, khong mot dong ma nao o tang tren phai doi.
// =============================================================================

const STEP_MS = { ppg: 10000, height: 700, weight: 900 };

export class MockTransport {
  name = 'mock';

  #onLine = null;
  #timers = new Set();
  #open = false;
  #buffered = [];       // ban ghi bi giu lai khi gia lap mat ket noi
  #dropped = false;     // dang gia lap mat ket noi
  #epochOffset = 0;
  #uptimeSec = 0;

  constructor({ onLine }) { this.#onLine = onLine; }

  get isOpen() { return this.#open; }

  async open() {
    this.#open = true;
    this.#every(1000, () => {
      this.#uptimeSec += 1;
      this.#emit({ t: 'pong', link: 'mock', up: this.#uptimeSec, pending: this.#buffered.length });
    });
    this.#after(80, () => this.#emit({ t: 'selftest', lcd: true, ppg: true, eeprom: true }));
    this.#after(120, () => this.#emit({
      t: 'info', link: 'mock', ip: '192.168.1.63', pending: 0,
      mount_cm: 200.0, w_scale: 2852.35, fw: 'SATx-2.0-mock',
    }));
    return 'Thiết bị mô phỏng';
  }

  send(text) {
    if (!this.#open) return false;
    for (const line of text.split(/\r?\n/)) {
      const trimmed = line.trim();
      if (trimmed) this.#handle(trimmed);
    }
    return true;
  }

  async close() {
    this.#open = false;
    for (const id of this.#timers) clearInterval(id), clearTimeout(id);
    this.#timers.clear();
  }

  /**
   * Gia lap mot thiet bi khong tra loi tren bus I2C.
   * Dung de xem giao dien canh bao the nao khi lap rap thieu day hoac sai dia chi.
   *   link.mock.simulateMissingDevice('eeprom')
   */
  simulateMissingDevice(which = 'eeprom') {
    this.#emit({
      t: 'selftest',
      lcd:    which !== 'lcd',
      ppg:    which !== 'ppg',
      eeprom: which !== 'eeprom',
    });
  }

  /** Gia lap rut cap: tu day tro di ban ghi bi cat vao bo dem thay vi gui len. */
  simulateDisconnect() { this.#dropped = true; }

  /** Gia lap cam lai cap: day het bo dem roi xoa, dung nhu firmware that. */
  simulateReconnect() {
    this.#dropped = false;
    let delay = 200;
    for (const record of this.#buffered) {
      this.#after(delay, () => this.#emit({ ...record, src: 'eeprom' }));
      delay += 120;
    }
    this.#buffered = [];
    this.#after(delay, () => this.#emit({
      t: 'info', link: 'mock', ip: '192.168.1.63', pending: 0,
      mount_cm: 200.0, w_scale: 2852.35, fw: 'SATx-2.0-mock',
    }));
  }

  // --- Noi bo -------------------------------------------------------------

  #handle(line) {
    let message;
    try { message = JSON.parse(line); } catch { return; }

    switch (message.t) {
      case 'ping':  break;
      case 'sync':
        this.#epochOffset = message.epoch - Math.floor(Date.now() / 1000);
        this.#emit({ t: 'selftest', lcd: true, ppg: true, eeprom: true });
        break;
      case 'measure': this.#runMeasurement(); break;
      case 'reset': this.#emit({ t: 'state', s: 'idle', step: 'none', pct: 0, eta: 0 }); break;
      case 'flush': this.simulateReconnect(); break;
      case 'erase': this.#buffered = []; break;
      default: break;
    }
  }

  #runMeasurement() {
    const total = STEP_MS.ppg + STEP_MS.height + STEP_MS.weight;
    const startedAt = Date.now();
    this.#emit({ t: 'state', s: 'measuring', step: 'ppg', pct: 0, eta: Math.round(total / 1000) });

    const tick = setInterval(() => {
      const elapsed = Date.now() - startedAt;
      if (elapsed >= total) { clearInterval(tick); this.#timers.delete(tick); return; }

      let step = 'ppg';
      let pct = 0;
      if (elapsed < STEP_MS.ppg) {
        step = 'ppg';
        pct = Math.round((elapsed / STEP_MS.ppg) * 70);
      } else if (elapsed < STEP_MS.ppg + STEP_MS.height) {
        step = 'height';
        pct = 78;
      } else {
        step = 'weight';
        pct = 94;
      }
      this.#emit({
        t: 'state', s: 'measuring', step, pct,
        eta: Math.max(0, Math.round((total - elapsed) / 1000)),
      });
    }, 250);
    this.#timers.add(tick);

    this.#after(total, () => {
      const weight = round(52 + Math.random() * 32, 2);
      const height = round(155 + Math.random() * 28, 1);
      const bmi = round(weight / (height / 100) ** 2, 1);
      const record = {
        t: 'result',
        ts: Math.floor(Date.now() / 1000) + this.#epochOffset,
        tsValid: true,
        w: weight, h: height,
        hr: Math.round(62 + Math.random() * 34),
        spo2: Math.round(94 + Math.random() * 5),
        bmi, ok: true,
      };

      this.#emit({ t: 'state', s: 'done', step: 'none', pct: 100, eta: 0 });

      if (this.#dropped) {
        // Mat ket noi hoan toan: firmware that se cat ban ghi vao EEPROM.
        this.#buffered.push(record);
      } else {
        this.#emit({ ...record, src: 'live' });
      }
    });
  }

  #emit(message) {
    if (!this.#open) return;
    this.#onLine?.(JSON.stringify(message));
  }

  #after(ms, fn) {
    const id = setTimeout(() => { this.#timers.delete(id); fn(); }, ms);
    this.#timers.add(id);
    return id;
  }

  #every(ms, fn) {
    const id = setInterval(fn, ms);
    this.#timers.add(id);
    return id;
  }
}

function round(value, digits) {
  const factor = 10 ** digits;
  return Math.round(value * factor) / factor;
}
