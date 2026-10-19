// =============================================================================
//  qr.js — Quet ma QR tren the CCCD gan chip.
//
//  Dinh dang ma QR cua CCCD Viet Nam, cac truong ngan cach bang dau gach dung:
//    0: so CCCD  1: so CMND cu  2: ho ten  3: ngay sinh ddMMyyyy
//    4: gioi tinh  5: dia chi  6: ngay cap ddMMyyyy
//  Cach tach nay giu nguyen tu ban WinForm goc (Form1.cs, ParseCCCD).
//
//  getUserMedia doi hoi ngu canh bao mat: Chrome CHAN camera tren file://.
//  Do la ly do trang phai chay qua http://localhost (xem serve.sh).
// =============================================================================

const $ = (id) => document.getElementById(id);

let stream = null;
let rafId = null;
let scanning = false;

export function isSupported() {
  return Boolean(navigator.mediaDevices?.getUserMedia) && typeof window.jsQR === 'function';
}

function setStatus(text, tone = '') {
  const chip = $('camChip');
  chip.className = tone ? `chip chip--${tone}` : 'chip';
  $('camText').textContent = text;
}

/** Tach chuoi QR thanh thong tin benh nhan. Tra ve null neu khong dung dinh dang. */
export function parseCccd(raw) {
  const parts = String(raw).split('|');
  if (parts.length < 6) return null;

  const toDate = (yyyymmdd) => {
    if (!/^\d{8}$/.test(yyyymmdd)) return '';
    return `${yyyymmdd.slice(0, 2)}/${yyyymmdd.slice(2, 4)}/${yyyymmdd.slice(4)}`;
  };

  return {
    id:      parts[0]?.trim() ?? '',
    name:    parts[2]?.trim() ?? '',
    dob:     toDate(parts[3]?.trim() ?? ''),
    gender:  parts[4]?.trim() ?? '',
    address: parts[5]?.trim() ?? '',
  };
}

export async function start({ onResult, onError }) {
  if (scanning) { stop(); return; }

  if (!isSupported()) {
    onError?.('Trình duyệt không cho dùng camera ở đây. Hãy mở trang qua http://localhost (xem serve.sh).');
    return;
  }

  try {
    stream = await navigator.mediaDevices.getUserMedia({
      video: { facingMode: 'environment', width: { ideal: 1280 }, height: { ideal: 960 } },
    });
  } catch (error) {
    onError?.(`Không mở được camera: ${error.message}`);
    return;
  }

  const video = $('camVideo');
  video.srcObject = stream;
  video.hidden = false;
  $('camIdle').hidden = true;
  $('camScan').hidden = false;
  $('camHint').textContent = 'Đưa mặt sau thẻ vào khung';
  $('btnScan').textContent = 'Tắt camera';
  setStatus('Camera đang bật', 'ok');
  scanning = true;

  await video.play();

  const canvas = $('camCanvas');
  const ctx = canvas.getContext('2d', { willReadFrequently: true });

  const loop = () => {
    if (!scanning) return;
    rafId = requestAnimationFrame(loop);

    if (video.readyState !== video.HAVE_ENOUGH_DATA) return;

    // Chi quet vung giua khung — dung noi ve bon goc, va nhanh hon nhieu.
    const cropW = Math.floor(video.videoWidth * 0.62);
    const cropH = Math.floor(video.videoHeight * 0.70);
    canvas.width = cropW;
    canvas.height = cropH;
    ctx.drawImage(video,
      Math.floor((video.videoWidth - cropW) / 2),
      Math.floor((video.videoHeight - cropH) / 2),
      cropW, cropH, 0, 0, cropW, cropH);

    const image = ctx.getImageData(0, 0, cropW, cropH);
    const code = window.jsQR(image.data, image.width, image.height, {
      inversionAttempts: 'attemptBoth',
    });
    if (!code?.data) return;

    const patient = parseCccd(code.data);
    if (!patient) {
      $('camHint').textContent = 'Mã đọc được nhưng không đúng định dạng CCCD';
      return;
    }

    stop();
    setStatus('Đã đọc xong thẻ', 'ok');
    onResult?.(patient);
  };

  loop();
}

export function stop() {
  scanning = false;
  if (rafId !== null) cancelAnimationFrame(rafId);
  rafId = null;

  for (const track of stream?.getTracks() ?? []) track.stop();
  stream = null;

  const video = $('camVideo');
  video.srcObject = null;
  video.hidden = true;
  $('camIdle').hidden = false;
  $('camScan').hidden = true;
  $('btnScan').textContent = 'Bật camera quét';
  setStatus('Camera tắt');
}
