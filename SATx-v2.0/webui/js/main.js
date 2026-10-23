// =============================================================================
//  main.js — Noi cac module lai voi nhau.
//
//  Luong du lieu mot chieu:
//      thiet bi --(JSON-lines)--> link.js --> main.js --> state.js --> ui/*
//  Nguoi dung bam nut --> main.js --> link.js --> thiet bi
//
//  Khong module giao dien nao noi thang toi tang truyen, va khong module nao
//  giu ban sao du lieu rieng.
// =============================================================================
import { Cmd } from './protocol.js';
import { Link } from './transport/link.js';
import {
  state, subscribe, update, addHistory, log, resetReading, resetPatient,
} from './state.js';
import * as measure from './ui/measure.js';
import * as history from './ui/history.js';
import * as qr from './ui/qr.js';
import * as pdf from './ui/pdf.js';
import { readingTime, clockLabel } from './ui/format.js';

const $ = (id) => document.getElementById(id);

// -----------------------------------------------------------------------------
//  Thong bao noi
// -----------------------------------------------------------------------------

function toast(text, tone = '') {
  const box = document.createElement('div');
  box.className = tone ? `toast toast--${tone}` : 'toast';
  box.textContent = text;
  $('toasts').append(box);
  setTimeout(() => box.remove(), 6000);
}

// -----------------------------------------------------------------------------
//  Tang truyen
// -----------------------------------------------------------------------------

const link = new Link({
  onMessage: handleMessage,
  onStatus: handleStatus,
});

function handleStatus(status) {
  update({
    link: status.connected ? status.kind : 'none',
    linkLabel: status.connected
      ? (status.kind === 'uart' ? `Cáp USB · ${status.label}`
        : status.kind === 'wifi' ? `WiFi · ${status.label}`
        : 'Thiết bị mô phỏng')
      : 'Chưa kết nối',
  });

  if (status.message) {
    const tone = status.connected ? '' : status.failingOver ? 'warn' : 'error';
    toast(status.message, tone);
    log(status.message, status.connected ? 'ok' : 'warn');
  }
  renderSettingsChips();
}

function handleMessage(message) {
  switch (message.t) {
    case 'pong':
      update({ pending: message.pending ?? 0 });
      break;

    case 'info':
      update({
        pending: message.pending ?? 0,
        deviceIp: message.ip ?? '',
      });
      $('calScale').textContent = message.w_scale !== undefined ? String(message.w_scale) : '—';
      $('calMount').textContent = message.mount_cm !== undefined ? `${message.mount_cm} cm` : '—';
      $('wifiIp').textContent = message.ip || '—';
      if (message.ip && message.ip !== '0.0.0.0' &&
          ['', 'satx.local'].includes($('wifiHost').value.trim())) {
        $('wifiHost').value = message.ip;
        link.setWifiConfig(message.ip, 81);
      }
      break;

    case 'selftest': {
      // Bao ngay thiet bi nao khong tra loi tren bus I2C. Bat duoc luc nay re
      // hon nhieu so voi mo mam khi so do sai.
      const missing = [
        !message.lcd    && 'màn hình LCD',
        !message.ppg    && 'cảm biến nhịp tim',
        !message.eeprom && 'bộ nhớ đệm EEPROM',
      ].filter(Boolean);
      if (missing.length === 0) {
        log('Tự kiểm tra: đủ cả ba thiết bị trên bus I2C', 'ok');
      } else {
        const text = `Tự kiểm tra: không thấy ${missing.join(', ')} trên bus I2C`;
        log(text, 'error');
        toast(text, 'error');
      }
      break;
    }

    case 'state':
      update({
        phase: message.s === 'measuring' ? 'measuring'
             : message.s === 'done' ? 'done'
             : message.s === 'error' ? 'error' : 'idle',
        step: message.step ?? 'none',
        percent: message.pct ?? 0,
        etaSec: message.eta ?? 0,
      });
      break;

    case 'result':
      acceptResult(message);
      break;

    case 'error':
      update({ phase: 'error', errorCode: message.code ?? '' });
      log(`Phép đo lỗi: ${message.code} — ${message.msg ?? ''}`, 'error');
      toast(`Phép đo lỗi: ${message.code}`, 'warn');
      break;

    case 'log':
      log(message.msg ?? '', message.lvl === 'error' ? 'error' : 'info');
      break;

    default:
      break;
  }
}

/** Ban tin ket qua — tu do truc tiep hoac tu bo dem cua thiet bi day len. */
function acceptResult(message) {
  const fromBuffer = message.src === 'eeprom';
  const receivedAt = new Date();
  const { date, approximate } = readingTime(message, receivedAt);

  const reading = {
    weightKg:  message.w    ?? null,
    heightCm:  message.h    ?? null,
    heartRate: message.hr   ?? null,
    spo2:      message.spo2 ?? null,
    bmi:       message.bmi  ?? null,
    ts:        message.ts   ?? 0,
    tsValid:   message.tsValid === true,
    ok:        message.ok === true,
    receivedAt,
  };

  // Ban ghi tu bo dem KHONG duoc gan vao benh nhan dang mo tren man hinh: luc
  // do giao dien mat ket noi nen chua he quet CCCD cho lan do ay.
  addHistory({
    source: fromBuffer ? 'eeprom' : 'live',
    at: date,
    approximate,
    patient: fromBuffer ? {} : { ...state.patient },
    reading,
  });

  if (fromBuffer) {
    log('Nhận một bản ghi từ bộ đệm của máy', 'warn');
  } else {
    update({ reading, phase: 'done' });
    log(
      reading.ok
        ? 'Nhận đủ bốn chỉ số, đã tính BMI'
        : 'Nhận kết quả nhưng thiếu một vài chỉ số',
      reading.ok ? 'ok' : 'warn',
    );
    if (!reading.tsValid) update({ clockSynced: false });
  }

  history.render();
}

// -----------------------------------------------------------------------------
//  Hanh dong nguoi dung
// -----------------------------------------------------------------------------

async function connect(kind) {
  try {
    await link.connect(kind);
    update({ transport: kind, clockSynced: true });
    if (kind === 'uart') $('uartPortLabel').value = link.kind === 'uart' ? 'Đã nối' : 'Chưa chọn';
    toast(kind === 'mock' ? 'Đang chạy ở chế độ mô phỏng' : 'Đã kết nối thiết bị');
    log('Đã kết nối thiết bị', 'ok');
  } catch (error) {
    toast(error.message, 'error');
    log(error.message, 'error');
  }
  renderSettingsChips();
}

function bindActions() {
  $('btnMeasure').addEventListener('click', () => {
    resetReading();
    update({ phase: 'measuring', step: 'ppg', percent: 0 });
    link.send(Cmd.measure());
    log('Gửi lệnh đo tới thiết bị');
  });

  $('btnReset').addEventListener('click', () => {
    link.send(Cmd.reset());
    resetReading();
    log('Đưa máy về trạng thái sẵn sàng');
  });

  $('btnClearReading').addEventListener('click', () => {
    resetReading();
    resetPatient();
    measure.clearPatientForm();
    log('Đã xoá lần đo hiện tại khỏi màn hình');
  });

  $('btnPdf').addEventListener('click', () => {
    if (!state.reading) return;
    try {
      const { date, approximate } = readingTime(state.reading, state.reading.receivedAt);
      pdf.exportReading({
        patient: state.patient,
        reading: state.reading,
        at: date,
        approximate,
      });
      log('Đã xuất phiếu kết quả PDF', 'ok');
    } catch (error) {
      toast(error.message, 'error');
    }
  });

  $('btnHistory').addEventListener('click', () => {
    history.render();
    measure.showView('history');
  });
  $('btnBackToMeasure').addEventListener('click', () => measure.showView('measure'));

  $('btnCsv').addEventListener('click', () => {
    if (state.history.length === 0) { toast('Chưa có dữ liệu để xuất'); return; }
    const blob = new Blob([history.toCsv()], { type: 'text/csv;charset=utf-8' });
    const url = URL.createObjectURL(blob);
    const anchor = document.createElement('a');
    anchor.href = url;
    anchor.download = `satx-lich-su-${Date.now()}.csv`;
    anchor.click();
    URL.revokeObjectURL(url);
  });

  $('btnScan').addEventListener('click', () => {
    qr.start({
      onResult: (patient) => {
        update({ patient });
        measure.fillPatientForm(patient);
        toast(`Đã đọc thẻ của ${patient.name || 'người đo'}`);
        log(`Quét QR CCCD thành công: ${patient.name || patient.id}`, 'ok');
      },
      onError: (text) => { toast(text, 'error'); log(text, 'error'); },
    });
  });
}

function bindSettings() {
  const overlay = $('settingsOverlay');
  const open  = () => overlay.classList.add('overlay--open');
  const close = () => overlay.classList.remove('overlay--open');

  $('btnSettings').addEventListener('click', open);
  $('btnCloseSettings').addEventListener('click', close);
  $('btnCloseSettings2').addEventListener('click', close);
  overlay.addEventListener('click', (event) => { if (event.target === overlay) close(); });
  document.addEventListener('keydown', (event) => { if (event.key === 'Escape') close(); });

  $('btnConnectUart').addEventListener('click', () => connect('uart'));
  $('btnDisconnect').addEventListener('click', async () => {
    await link.disconnect();
    update({ link: 'none', linkLabel: 'Chưa kết nối' });
    toast('Đã ngắt tất cả kết nối. Bấm Chuyển sang WiFi để kết nối qua mạng.');
    renderSettingsChips();
  });

  $('btnSaveWifi').addEventListener('click', async () => {
    const ssid = $('wifiSsid').value.trim();
    const pass = $('wifiPass').value;
    link.setWifiConfig($('wifiHost').value.trim(), 81);
    if (!ssid) {
      toast('Nhập tên mạng WiFi cần lưu', 'warn');
      return;
    }
    if (!link.isOpen || link.kind === 'mock') {
      toast('Hãy bấm Chọn cổng và kết nối USB với ESP32 trước khi lưu WiFi', 'warn');
      return;
    }
    if (!await link.send(Cmd.wifi(ssid, pass))) {
      toast('Chưa gửi được cấu hình WiFi. Kiểm tra lại kết nối với ESP32.', 'error');
      return;
    }
    toast('Đã gửi cấu hình WiFi xuống ESP32');
    log('Đã gửi cấu hình WiFi xuống ESP32', 'ok');
  });
  $('btnConnectWifi').addEventListener('click', async () => {
    link.setWifiConfig($('wifiHost').value.trim(), 81);
    await connect('wifi');
  });

  $('btnTare').addEventListener('click', () => { link.send(Cmd.tare()); toast('Đã gửi lệnh trừ bì'); });
  $('btnCalWeight').addEventListener('click', () => {
    const kg = Number($('calKnownKg').value.replace(',', '.'));
    if (!(kg > 0)) { toast('Khối lượng quả cân phải lớn hơn 0', 'warn'); return; }
    link.send(Cmd.calWeight(kg));
    toast(`Đặt hệ số cân theo quả chuẩn ${kg} kg`);
  });
  $('btnCalHeight').addEventListener('click', () => {
    const cm = Number($('calMountCm').value.replace(',', '.'));
    if (!(cm > 0)) { toast('Chiều cao lắp phải lớn hơn 0', 'warn'); return; }
    link.send(Cmd.calHeight(cm));
    toast(`Đặt chiều cao lắp cảm biến ${cm} cm`);
  });

  $('btnErase').addEventListener('click', () => {
    link.send(Cmd.erase(true));
    toast('Đã gửi lệnh xoá sạch bộ đệm trên máy', 'warn');
  });

  $('transportPicker').addEventListener('click', async (event) => {
    const kind = event.target?.dataset?.transport;
    if (!kind) return;
    for (const button of $('transportPicker').querySelectorAll('button')) {
      button.setAttribute('aria-pressed', String(button.dataset.transport === kind));
    }
    $('mockTools').hidden = kind !== 'mock';
    update({ transport: kind });
    if (kind === 'mock') await connect('mock');
    else await link.disconnect();
  });

  $('btnMockDrop').addEventListener('click', () => {
    link.mock?.simulateDisconnect();
    toast('Mô phỏng: máy mất kết nối, kết quả sẽ vào bộ đệm', 'warn');
    log('Mô phỏng mất kết nối — máy chuyển sang lưu bộ đệm', 'warn');
  });
  $('btnMockRestore').addEventListener('click', () => {
    link.mock?.simulateReconnect();
    toast('Mô phỏng: nối lại, đang đẩy bộ đệm lên');
    log('Mô phỏng nối lại — đang đẩy bộ đệm', 'ok');
  });
}

function renderSettingsChips() {
  const uartOn = state.link === 'uart';
  const wifiOn = state.link === 'wifi';

  $('uartChip').className = uartOn ? 'chip chip--ok' : 'chip';
  $('uartChipText').textContent = uartOn ? 'Đang dùng' : 'Chưa nối';

  $('wifiChip').className = wifiOn ? 'chip chip--warn' : 'chip';
  $('wifiChipText').textContent = wifiOn ? 'Đang dùng' : 'Chờ sẵn';
}

// -----------------------------------------------------------------------------
//  Khoi dong
// -----------------------------------------------------------------------------

function startClock() {
  const tick = () => { $('clock').textContent = clockLabel(new Date()); };
  tick();
  setInterval(tick, 20000);
}

function checkEnvironment() {
  if (!window.isSecureContext) {
    toast(
      'Trang đang mở ngoài ngữ cảnh bảo mật nên trình duyệt sẽ chặn cổng USB và camera. ' +
      'Hãy chạy ./serve.sh rồi mở http://localhost:8080',
      'error',
    );
  }
}

function boot() {
  measure.bindPatientForm();
  bindActions();
  bindSettings();
  history.bind(() => history.render());

  subscribe(measure.render);
  history.render();
  renderSettingsChips();
  startClock();
  checkEnvironment();

  // Tay cam go loi. Chi de kiem thu thu cong tu Console cua trinh duyet,
  // khong module nao trong ung dung duoc phep dung den.
  //   satx.link.mock?.simulateMissingDevice('eeprom')
  //   satx.link.mock?.simulateDisconnect()
  //   satx.state.history
  window.satx = { link, state, log, toast };

  log('Giao diện sẵn sàng. Mở Cài đặt để chọn cổng hoặc bật chế độ mô phỏng.');
}

boot();
