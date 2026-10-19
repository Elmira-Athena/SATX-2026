// =============================================================================
//  protocol.js — Ban tin JSON-lines, dung chung cho ca UART va WebSocket.
//
//  Dac ta day du: docs/giao-thuc.md
//  Ban cai firmware tuong ung: firmware/SATx/src/comm/Protocol.cpp
// =============================================================================

/** Ma lenh gui xuong thiet bi. */
export const Cmd = {
  ping:    ()               => ({ t: 'ping' }),
  sync:    (epochSec)       => ({ t: 'sync', epoch: epochSec }),
  measure: ()               => ({ t: 'measure' }),
  reset:   ()               => ({ t: 'reset' }),
  flush:   ()               => ({ t: 'flush' }),
  tare:    ()               => ({ t: 'cal', target: 'weight' }),
  calWeight: (knownKg)      => ({ t: 'cal', target: 'weight', known_kg: knownKg }),
  calHeight: (mountCm)      => ({ t: 'cal', target: 'height', mount_cm: mountCm }),
  wifi:    (ssid, pass)     => ({ t: 'cfg', wifi: { ssid, pass } }),
  erase:   (physical=false) => ({ t: 'erase', physical }),
};

/** Ma hoa mot lenh thanh dong de gui di. */
export function encode(command) {
  return JSON.stringify(command) + '\n';
}

/**
 * Giai ma mot dong nhan duoc.
 *
 * Tra ve null cho moi dong khong phai ban tin hop le. Day KHONG phai la loi:
 * cong UART0 dung chung voi log bootloader cua ESP32, nen luc vua cam cap ta se
 * thay nhung dong nhu "rst:0x1 (POWERON_RESET)". Bo qua chung la dung.
 */
export function decode(line) {
  const text = line.trim();
  if (!text.startsWith('{')) return null;
  try {
    const message = JSON.parse(text);
    return typeof message?.t === 'string' ? message : null;
  } catch {
    return null;
  }
}

/**
 * Tach mot khoi byte/chuoi thanh cac dong hoan chinh.
 * Giu lai phan du chua co xuong dong cho lan goi sau.
 */
export class LineSplitter {
  #buffer = '';

  push(chunk) {
    this.#buffer += chunk;
    const parts = this.#buffer.split(/\r?\n/);
    this.#buffer = parts.pop() ?? '';
    // Chan bo dem phinh vo han khi gap luong khong bao gio xuong dong.
    if (this.#buffer.length > 4096) this.#buffer = '';
    return parts.filter((line) => line.length > 0);
  }

  reset() { this.#buffer = ''; }
}
