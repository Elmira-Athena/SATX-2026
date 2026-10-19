// =============================================================================
//  pdf.js — Xuat phieu ket qua.
//
//  Giu nguyen bo cuc va bang phan loai BMI cua ban WinForm goc (Form1.cs,
//  XuatFilePDF) de phieu in ra khong khac gi ban cu.
//
//  jsPDF dung font Helvetica khong co glyph tieng Viet co dau, nen nhan tren
//  phieu duoc viet KHONG DAU. Day la lua chon co y: nhung tep font Unicode day
//  du se lam trang nang them vai tram KB ma phong lab thuong in den trang.
// =============================================================================
import { numDot, timeOf, dateOf } from './format.js';
import { bmiClass } from './format.js';

const NO_ACCENT = {
  'Thiếu cân': 'Thieu can',
  'Bình thường': 'Binh thuong',
  'Thừa cân': 'Thua can',
  'Béo phì độ I': 'Beo phi do I',
  'Béo phì độ II': 'Beo phi do II',
  'Chưa đo': 'Chua do',
};

/** Bo dau tieng Viet de jsPDF in duoc bang font mac dinh. */
export function deaccent(text) {
  return String(text ?? '')
    .normalize('NFD')
    .replace(/[\u0300-\u036f]/g, '')
    .replace(/đ/g, 'd').replace(/Đ/g, 'D');
}

export function isSupported() {
  return Boolean(window.jspdf?.jsPDF);
}

export function exportReading({ patient, reading, at, approximate }) {
  if (!isSupported()) throw new Error('Thiếu thư viện jsPDF trong thư mục vendor/.');

  const { jsPDF } = window.jspdf;
  const doc = new jsPDF({ unit: 'mm', format: 'a4' });

  const left = 22;
  let y = 26;

  const line = (text, { size = 11, bold = false, gap = 7 } = {}) => {
    doc.setFont('helvetica', bold ? 'bold' : 'normal');
    doc.setFontSize(size);
    doc.text(deaccent(text), left, y);
    y += gap;
  };

  const rule = (gap = 6) => {
    doc.setDrawColor(200, 195, 185);
    doc.line(left, y - 3, 188, y - 3);
    y += gap - 3;
  };

  line('PHIEU KET QUA DO CHI SO SUC KHOE', { size: 16, bold: true, gap: 8 });
  line('Tram do SATx', { size: 10, gap: 8 });
  rule();

  line('THONG TIN NGUOI DO', { size: 11, bold: true, gap: 8 });
  line(`Ho va ten : ${patient.name || '(chua nhap)'}`);
  line(`Gioi tinh : ${patient.gender || '-'}`);
  line(`Ngay sinh : ${patient.dob || '-'}`);
  line(`So CCCD   : ${patient.id || '-'}`);
  line(`Dia chi   : ${patient.address || '-'}`, { gap: 10 });
  rule();

  line('CHI SO DO DUOC', { size: 11, bold: true, gap: 8 });
  const value = (v, digits, unit) => (v === null || v === undefined ? 'khong doc duoc' : `${numDot(v, digits)} ${unit}`);
  line(`Can nang  : ${value(reading.weightKg, 2, 'kg')}`);
  line(`Chieu cao : ${value(reading.heightCm, 1, 'cm')}`);
  line(`Nhip tim  : ${value(reading.heartRate, 0, 'nhip/phut')}`);
  line(`SpO2      : ${value(reading.spo2, 0, '%')}`);

  const cls = bmiClass(reading.bmi);
  line(`BMI       : ${value(reading.bmi, 1, 'kg/m2')}`, { bold: true });
  line(`Danh gia  : ${NO_ACCENT[cls.label] ?? deaccent(cls.label)}`, { gap: 10 });
  rule();

  const stamp = `${dateOf(at)} ${timeOf(at)}`;
  line(`Thoi diem do : ${stamp}${approximate ? ' (uoc luong theo gio may tinh)' : ''}`, { size: 10 });
  y += 4;
  doc.setFontSize(9);
  doc.setTextColor(120, 120, 115);
  doc.text(
    deaccent('Ket qua mang tinh sang loc, khong thay the chan doan cua bac si.'),
    left, y,
  );

  const safeName = deaccent(patient.name || 'ket-qua').replace(/[^\w\s-]/g, '').trim() || 'ket-qua';
  doc.save(`${safeName.replace(/\s+/g, '-')}-${dateOf(at).replace(/\//g, '')}.pdf`);
}
