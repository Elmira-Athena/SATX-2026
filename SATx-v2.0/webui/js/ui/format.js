// =============================================================================
//  format.js — Dinh dang so va thoi gian theo quy uoc tieng Viet.
//
//  Dat rieng vi bang lich su, the chi so va ban PDF phai hien CUNG mot cach.
// =============================================================================

/** Dau phay thap phan kieu Viet Nam. */
export function num(value, digits) {
  if (value === null || value === undefined || Number.isNaN(value)) return null;
  return value.toFixed(digits).replace('.', ',');
}

/** Dau cham thap phan, dung cho CSV va PDF de tranh lech cot. */
export function numDot(value, digits) {
  if (value === null || value === undefined || Number.isNaN(value)) return '';
  return value.toFixed(digits);
}

const pad = (n) => String(n).padStart(2, '0');

export function timeOf(date) {
  return `${pad(date.getHours())}:${pad(date.getMinutes())}:${pad(date.getSeconds())}`;
}

export function dateOf(date) {
  return `${pad(date.getDate())}/${pad(date.getMonth() + 1)}/${date.getFullYear()}`;
}

export function clockLabel(date) {
  return `${pad(date.getHours())}:${pad(date.getMinutes())} · ${date.getDate()} tháng ${date.getMonth() + 1}`;
}

/**
 * Thoi diem cua mot ban ghi.
 *
 * tsValid = false nghia la thiet bi chua tung dong bo dong ho voi may tinh
 * (khong co RTC nuoi pin). Khi do ta hien thoi diem GIAO DIEN nhan duoc va noi
 * ro, thay vi in ra mot moc gio sai.
 */
export function readingTime(reading, receivedAt) {
  if (reading?.tsValid && reading.ts > 0) {
    return { date: new Date(reading.ts * 1000), approximate: false };
  }
  return { date: receivedAt, approximate: true };
}

/** Phan loai BMI theo nguong dung trong ban WinForm goc. */
export function bmiClass(bmi) {
  if (bmi === null || bmi === undefined || bmi <= 0) {
    return { label: 'Chưa đo', tone: 'none' };
  }
  if (bmi < 18.5) return { label: 'Thiếu cân',        tone: 'warn'  };
  if (bmi < 25)   return { label: 'Bình thường',      tone: 'ok'    };
  if (bmi < 30)   return { label: 'Thừa cân',         tone: 'warn'  };
  if (bmi < 35)   return { label: 'Béo phì độ I',     tone: 'alert' };
  return            { label: 'Béo phì độ II',    tone: 'alert' };
}

/**
 * Vi tri cua BMI tren thang phan loai, tra ve phan tram 0..100.
 *
 * Bon vung tren thanh co ty le 18.5 : 6.5 : 5 : 10 (tong 40), tuong ung cac
 * moc BMI 0-18.5, 18.5-25, 25-30, 30-40. Ham nay phai bam dung ty le do,
 * neu khong vach chi se lech so voi mau nen.
 */
export function bmiMarkerPercent(bmi) {
  if (!bmi || bmi <= 0) return null;
  const stops = [
    { from: 0,    to: 18.5, start: 0,      span: 18.5 / 40 },
    { from: 18.5, to: 25,   start: 18.5/40, span: 6.5 / 40 },
    { from: 25,   to: 30,   start: 25 / 40, span: 5 / 40   },
    { from: 30,   to: 40,   start: 30 / 40, span: 10 / 40  },
  ];
  const value = Math.min(Math.max(bmi, 0), 40);
  for (const stop of stops) {
    if (value <= stop.to) {
      const within = (value - stop.from) / (stop.to - stop.from);
      return (stop.start + within * stop.span) * 100;
    }
  }
  return 100;
}
