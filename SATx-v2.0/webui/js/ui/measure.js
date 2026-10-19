// =============================================================================
//  measure.js — Ve man hinh do tu trang thai.
//
//  Nguyen tac: KHONG doc gia tri tu DOM. Moi thu di mot chieu tu state.js ra
//  man hinh; bieu mau benh nhan la cho duy nhat di nguoc lai (nguoi dung go).
// =============================================================================
import { state, update } from '../state.js';
import { num, timeOf, bmiClass, bmiMarkerPercent } from './format.js';

const $ = (id) => document.getElementById(id);

const STEP_TITLE = {
  ppg:    'Nhịp tim & nồng độ oxy',
  paused: 'Tạm dừng — hãy đặt lại ngón tay',
  height: 'Chiều cao',
  weight: 'Cân nặng',
  none:   'Đang chuẩn bị',
};

const STEP_INDEX = { ppg: 1, paused: 1, height: 2, weight: 3 };

/** Gan bieu mau benh nhan vao state (chieu duy nhat di tu DOM vao). */
export function bindPatientForm() {
  const fields = {
    fName:    'name',
    fGender:  'gender',
    fDob:     'dob',
    fId:      'id',
    fAddress: 'address',
  };
  for (const [elementId, key] of Object.entries(fields)) {
    $(elementId).addEventListener('input', (event) => {
      state.patient[key] = event.target.value;
    });
  }
}

/** Do thong tin benh nhan tu QR nguoc tro lai bieu mau. */
export function fillPatientForm(patient) {
  $('fName').value    = patient.name    ?? '';
  $('fGender').value  = patient.gender  ?? '';
  $('fDob').value     = patient.dob     ?? '';
  $('fId').value      = patient.id      ?? '';
  $('fAddress').value = patient.address ?? '';
  $('patientSource').hidden = !patient.name;
}

export function clearPatientForm() {
  fillPatientForm({});
  $('patientSource').hidden = true;
}

/** Khoa bieu mau trong luc do de tranh sua nham. */
function setFormLocked(locked) {
  for (const id of ['fName', 'fGender', 'fDob', 'fId', 'fAddress']) {
    $(id).disabled = locked;
  }
}

function setMetric(cardId, valueId, value, digits) {
  const card = $(cardId);
  const text = num(value, digits);
  $(valueId).textContent = text ?? '— —';
  card.classList.toggle('metric--empty', text === null);
}

function renderProgress(s) {
  const panel = $('progressPanel');
  const measuring = s.phase === 'measuring';
  panel.hidden = !measuring;
  if (!measuring) return;

  const index = STEP_INDEX[s.step] ?? 1;
  $('progressStepLabel').textContent = `Đang đo · bước ${index} trong 3`;
  $('progressTitle').textContent = STEP_TITLE[s.step] ?? STEP_TITLE.none;
  $('progressEta').innerHTML = s.etaSec > 0
    ? `${s.etaSec}<span style="font-size:18px; color:#8FA396;"> giây</span>`
    : '—';
  $('progressFill').style.width = `${Math.min(100, Math.max(0, s.percent))}%`;

  const steps = [
    { el: $('stepPpg'),    key: 'ppg',    order: 1 },
    { el: $('stepHeight'), key: 'height', order: 2 },
    { el: $('stepWeight'), key: 'weight', order: 3 },
  ];
  for (const step of steps) {
    const active = step.order === index;
    const done   = step.order < index;
    step.el.classList.toggle('step--active', active);
    step.el.classList.toggle('step--done', done);
    step.el.querySelector('.step__state').textContent =
      s.step === 'paused' && active ? 'tạm dừng' : active ? 'đang đo' : done ? 'xong' : 'chờ';
  }
}

function renderResultHead(s) {
  $('resultName').textContent = s.patient.name?.trim() || 'Chưa có bệnh nhân';

  const chip = $('resultStatus');
  const label = chip.querySelector('span:last-child');
  chip.className = 'chip';
  switch (s.phase) {
    case 'measuring': chip.classList.add('chip--warn'); label.textContent = 'Đang đo'; break;
    case 'done':      chip.classList.add('chip--ok');   label.textContent = 'Đo xong'; break;
    case 'error':     chip.classList.add('chip--warn'); label.textContent = `Lỗi: ${s.errorCode || 'không rõ'}`; break;
    default:          label.textContent = 'Chưa đo';
  }

  $('resultTime').textContent = s.reading?.receivedAt ? timeOf(s.reading.receivedAt) : '--:--:--';
}

function renderMetrics(s) {
  const r = s.reading;
  setMetric('cardWeight', 'valWeight', r?.weightKg  ?? null, 2);
  setMetric('cardHeight', 'valHeight', r?.heightCm  ?? null, 1);
  setMetric('cardHeart',  'valHeart',  r?.heartRate ?? null, 0);
  setMetric('cardSpo2',   'valSpo2',   r?.spo2      ?? null, 0);

  const bmi = r?.bmi ?? null;
  $('valBmi').textContent = num(bmi, 1) ?? '— —';

  const cls = bmiClass(bmi);
  const badge = $('bmiBadge');
  badge.textContent = cls.label;
  badge.className = `bmi__badge bmi__badge--${cls.tone}`;

  const percent = bmiMarkerPercent(bmi);
  $('bmiMarker').style.left = percent === null ? '-10px' : `calc(${percent}% - 1.5px)`;
}

function renderTimeline(s) {
  const box = $('timeline');
  if (s.log.length === 0) {
    box.innerHTML =
      '<div class="timeline__row"><span class="timeline__time">--:--:--</span>' +
      '<span class="timeline__dot"></span>' +
      '<span class="timeline__text">Chưa có hoạt động nào.</span></div>';
    return;
  }
  box.innerHTML = s.log.slice(0, 6).map((entry) => `
    <div class="timeline__row">
      <span class="timeline__time">${timeOf(entry.at)}</span>
      <span class="timeline__dot timeline__dot--${entry.kind}"></span>
      <span class="timeline__text">${escapeHtml(entry.text)}</span>
    </div>`).join('');
}

function renderLamps(s) {
  const red = $('lampRed');
  const green = $('lampGreen');
  red.className = 'lamp';
  green.className = 'lamp';

  if (s.phase === 'measuring' || s.phase === 'error') red.classList.add('lamp--red-on');
  if (s.phase === 'done') green.classList.add('lamp--green-on');

  $('lampLabel').textContent =
    s.link === 'none' ? 'Chưa kết nối'
    : s.phase === 'measuring' ? 'Đang đo'
    : s.phase === 'done'      ? 'Đo xong'
    : s.phase === 'error'     ? 'Lỗi phép đo'
    : 'Máy rảnh';
}

function renderControls(s) {
  const connected = s.link !== 'none';
  $('btnMeasure').disabled = !connected || s.phase === 'measuring';
  $('btnMeasure').textContent = s.phase === 'measuring' ? 'Đang đo…' : 'Bắt đầu đo';
  $('btnReset').disabled = !connected;
  $('btnClearReading').disabled = s.reading === null;
  $('btnPdf').disabled = s.reading === null;
  setFormLocked(s.phase === 'measuring');

  const parts = [];
  if (s.pending > 0) parts.push(`Bộ đệm thiết bị: ${s.pending} bản ghi chờ`);
  else if (connected) parts.push('Bộ đệm thiết bị trống');
  if (!s.clockSynced && connected) parts.push('đồng hồ chưa đồng bộ');
  parts.push('dữ liệu chỉ lưu trong phiên này');
  $('footerNote').textContent = parts.join(' · ');
}

function renderLink(s) {
  const chip = $('linkChip');
  const text = $('linkText');
  chip.className = 'chip';
  if (s.link === 'none') {
    text.textContent = 'Chưa kết nối';
  } else {
    chip.classList.add(s.link === 'wifi' ? 'chip--warn' : 'chip--ok');
    text.textContent = s.linkLabel;
  }
}

export function render(s) {
  renderLink(s);
  renderLamps(s);
  renderProgress(s);
  renderResultHead(s);
  renderMetrics(s);
  renderTimeline(s);
  renderControls(s);
}

export function escapeHtml(text) {
  return String(text).replace(/[&<>"']/g, (ch) => ({
    '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;',
  }[ch]));
}

export function showView(name) {
  $('measureView').classList.toggle('view--active', name === 'measure');
  $('historyView').classList.toggle('view--active', name === 'history');
  update({});
}
