// =============================================================================
//  state.js — Kho trang thai duy nhat cua giao dien.
//
//  Moi thay doi di qua day, cac module giao dien chi dang ky lang nghe. Nho vay
//  khong co hai cho nao cung giu mot ban sao du lieu roi lech nhau.
//
//  Luu y theo yeu cau du an: lich su chi nam trong bo nho. Dong tab la mat sach.
// =============================================================================

const listeners = new Set();

export const state = {
  // --- Ket noi ---
  link: 'none',          // 'uart' | 'wifi' | 'none' | 'mock'
  linkLabel: 'Chưa kết nối',
  transport: 'uart',     // kenh nguoi dung chon: 'uart' | 'wifi' | 'mock'
  deviceIp: '',
  pending: 0,            // so ban ghi con trong bo dem thiet bi
  clockSynced: false,

  // --- Chu trinh do ---
  phase: 'idle',         // 'idle' | 'measuring' | 'done' | 'error'
  step: 'none',          // 'ppg' | 'paused' | 'height' | 'weight' | 'none'
  percent: 0,
  etaSec: 0,
  errorCode: '',

  // --- Benh nhan (chi ton tai trong phien) ---
  patient: { name: '', gender: '', dob: '', id: '', address: '' },

  // --- Ket qua lan do hien tai ---
  reading: null,         // { weightKg, heightCm, heartRate, spo2, bmi, ts, tsValid, ok }

  // --- Lich su gop: do truc tiep va ban ghi day tu bo dem thiet bi ---
  history: [],           // [{ id, source, at, patient, reading }]

  // --- Nhat ky hien thi ---
  log: [],               // [{ at, text, kind }]
};

export function subscribe(listener) {
  listeners.add(listener);
  listener(state);
  return () => listeners.delete(listener);
}

export function update(patch) {
  Object.assign(state, patch);
  notify();
}

export function notify() {
  for (const listener of listeners) listener(state);
}

let nextId = 1;

export function addHistory(entry) {
  state.history.unshift({ id: nextId++, ...entry });
  notify();
}

export function removeHistory(id) {
  state.history = state.history.filter((row) => row.id !== id);
  notify();
}

export function log(text, kind = 'info') {
  state.log.unshift({ at: new Date(), text, kind });
  if (state.log.length > 40) state.log.length = 40;
  notify();
}

export function resetReading() {
  state.reading = null;
  state.phase = 'idle';
  state.step = 'none';
  state.percent = 0;
  state.etaSec = 0;
  state.errorCode = '';
  notify();
}

export function resetPatient() {
  state.patient = { name: '', gender: '', dob: '', id: '', address: '' };
  notify();
}
