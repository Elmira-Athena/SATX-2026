// =============================================================================
//  history.js — Bang lich su gop.
//
//  Yeu cau du an: gop lich su do truc tiep va lich su day len tu bo dem cua
//  thiet bi VAO CUNG MOT BANG, xep theo thoi gian thuc. Ban ghi tu bo dem duoc
//  to nen vang va khong co thong tin benh nhan — luc do giao dien dang mat ket
//  noi nen chua he quet CCCD.
// =============================================================================
import { state, removeHistory } from '../state.js';
import { num, numDot, timeOf, dateOf } from './format.js';
import { escapeHtml } from './measure.js';

const $ = (id) => document.getElementById(id);

let filter = 'all';

function cellNumber(value, digits, unit, strong = false) {
  const text = num(value, digits);
  if (text === null) return '<td class="num"><span class="cell-number cell-number--empty">—</span></td>';
  const cls = strong ? 'cell-number cell-number--strong' : 'cell-number';
  const suffix = unit ? `<span class="cell-unit"> ${unit}</span>` : '';
  return `<td class="num"><span class="${cls}">${text}</span>${suffix}</td>`;
}

function cellPerson(entry) {
  const name = entry.patient?.name?.trim();
  if (!name) {
    // Hai truong hop khac han nhau, khong duoc noi giong nhau:
    //  · tu bo dem  -> luc do giao dien mat ket noi nen KHONG THE co CCCD
    //  · truc tiep  -> co ket noi, chi la nguoi dung chua nhap thong tin
    const fromBuffer = entry.source === 'eeprom';
    return `<td><div class="cell-person">
      <span class="cell-person__none">${fromBuffer ? 'Chưa gắn thông tin' : 'Chưa nhập thông tin'}</span>
      <span class="cell-person__meta">${
        fromBuffer ? 'đo lúc giao diện đang mất kết nối' : 'đo trực tiếp, chưa quét CCCD'
      }</span>
    </div></td>`;
  }
  const bits = [entry.patient.id, entry.patient.gender].filter(Boolean).join(' · ');
  return `<td><div class="cell-person">
    <span class="cell-person__name">${escapeHtml(name)}</span>
    <span class="cell-person__meta">${escapeHtml(bits || '—')}</span>
  </div></td>`;
}

function visibleRows() {
  if (filter === 'all') return state.history;
  return state.history.filter((row) => row.source === filter);
}

export function render() {
  const rows = visibleRows();
  const body = $('historyBody');

  if (rows.length === 0) {
    body.innerHTML = `<tr class="empty-row"><td colspan="10">${
      state.history.length === 0
        ? 'Chưa có lần đo nào trong phiên này.'
        : 'Không có bản ghi nào khớp bộ lọc.'
    }</td></tr>`;
  } else {
    body.innerHTML = rows.map((entry, index) => {
      const r = entry.reading;
      const fromBuffer = entry.source === 'eeprom';
      const time = entry.approximate ? `~${timeOf(entry.at)}` : timeOf(entry.at);
      return `<tr class="${fromBuffer ? 'from-buffer' : ''}">
        <td class="cell-mono">${rows.length - index}</td>
        ${cellPerson(entry)}
        ${cellNumber(r.weightKg, 2, 'kg')}
        ${cellNumber(r.heightCm, 1, 'cm')}
        ${cellNumber(r.heartRate, 0, '')}
        ${cellNumber(r.spo2, 0, '%')}
        ${cellNumber(r.bmi, 1, '', true)}
        <td class="cell-mono" title="${escapeHtml(dateOf(entry.at))}">${time}</td>
        <td><span class="badge ${fromBuffer ? 'badge--buffer' : ''}">${
          fromBuffer ? 'Từ bộ đệm' : 'Trực tiếp'
        }</span></td>
        <td class="num"><button class="btn btn--tiny btn--danger" data-remove="${entry.id}">Xoá</button></td>
      </tr>`;
    }).join('');
  }

  const total = state.history.length;
  const buffered = state.history.filter((row) => row.source === 'eeprom').length;
  $('historySub').textContent = total === 0
    ? 'Chưa có lần đo nào'
    : `${total} lần đo, xếp theo thời gian thực${buffered > 0 ? ` — trong đó ${buffered} lần máy tự lưu khi mất kết nối` : ''}`;

  const note = $('bufferNote');
  note.hidden = buffered === 0;
  if (buffered > 0) {
    $('bufferNoteText').textContent =
      `${buffered} dòng nền vàng do máy tự lưu khi giao diện mất kết nối, được đẩy lên sau khi nối lại. ` +
      'Chúng không kèm thông tin bệnh nhân vì lúc đó chưa quét CCCD. Bộ đệm trên máy đã được xoá sau khi đẩy xong.';
  }
}

export function bind(onChange) {
  $('historyBody').addEventListener('click', (event) => {
    const id = event.target?.dataset?.remove;
    if (!id) return;
    removeHistory(Number(id));
    onChange?.();
  });

  $('historyFilter').addEventListener('click', (event) => {
    const next = event.target?.dataset?.filter;
    if (!next) return;
    filter = next;
    for (const button of $('historyFilter').querySelectorAll('button')) {
      button.setAttribute('aria-pressed', String(button.dataset.filter === filter));
    }
    render();
  });
}

/** Xuat CSV — dung dau cham thap phan de mo bang Excel khong lech cot. */
export function toCsv() {
  const header = [
    'STT', 'Ho ten', 'Gioi tinh', 'Ngay sinh', 'CCCD', 'Dia chi',
    'Can nang (kg)', 'Chieu cao (cm)', 'Nhip tim (bpm)', 'SpO2 (%)', 'BMI',
    'Thoi gian', 'Thoi gian uoc luong', 'Nguon',
  ];
  const escape = (value) => `"${String(value ?? '').replace(/"/g, '""')}"`;

  const lines = [header.join(',')];
  state.history.forEach((entry, index) => {
    const r = entry.reading;
    const p = entry.patient ?? {};
    lines.push([
      state.history.length - index,
      escape(p.name), escape(p.gender), escape(p.dob), escape(p.id), escape(p.address),
      numDot(r.weightKg, 2), numDot(r.heightCm, 1),
      numDot(r.heartRate, 0), numDot(r.spo2, 0), numDot(r.bmi, 1),
      escape(`${dateOf(entry.at)} ${timeOf(entry.at)}`),
      entry.approximate ? 'co' : 'khong',
      entry.source === 'eeprom' ? 'bo dem' : 'truc tiep',
    ].join(','));
  });

  // BOM de Excel tren Windows doc dung tieng Viet co dau.
  return '﻿' + lines.join('\r\n');
}
