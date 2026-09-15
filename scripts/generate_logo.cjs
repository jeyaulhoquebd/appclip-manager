const fs = require('fs');
const zlib = require('zlib');
const path = require('path');

const width = 128;
const height = 128;
const buffer = Buffer.alloc(height * (1 + width * 4));

function setPixel(x, y, r, g, b, a = 255) {
  if (x < 0 || x >= width || y < 0 || y >= height) return;
  const rowOffset = y * (1 + width * 4);
  const pxOffset = rowOffset + 1 + x * 4;
  buffer[pxOffset] = r;
  buffer[pxOffset + 1] = g;
  buffer[pxOffset + 2] = b;
  buffer[pxOffset + 3] = a;
}

// Draw rounded rect background and AppClip icon
for (let y = 0; y < height; y++) {
  // filter byte = 0 (None)
  buffer[y * (1 + width * 4)] = 0;
  for (let x = 0; x < width; x++) {
    // Rounded corner check (radius = 24)
    const r = 26;
    let inCard = true;
    const dx = x < r ? r - x : x > width - 1 - r ? x - (width - 1 - r) : 0;
    const dy = y < r ? r - y : y > height - 1 - r ? y - (height - 1 - r) : 0;
    const dist = Math.sqrt(dx * dx + dy * dy);
    
    if (dist > r) {
      setPixel(x, y, 0, 0, 0, 0);
      continue;
    }

    // Gradient background: from #3b82f6 (59, 130, 246) to #1d4ed8 (29, 78, 216)
    const t = (x + y) / (width + height);
    let red = Math.round(59 * (1 - t) + 29 * t);
    let green = Math.round(130 * (1 - t) + 78 * t);
    let blue = Math.round(246 * (1 - t) + 216 * t);
    let alpha = 255;
    if (dist > r - 1.5) {
      alpha = Math.round(Math.max(0, Math.min(255, (r - dist) * 170)));
    }

    // Inner subtle border
    if (x === 4 || x === width - 5 || y === 4 || y === height - 5) {
      red = Math.min(255, red + 30);
      green = Math.min(255, green + 30);
      blue = Math.min(255, blue + 30);
    }

    // Draw Package Box Icon in center (x: 34..94, y: 34..94)
    const cx = 64;
    const cy = 64;

    // Outer package shape isometric box
    const inBox = (x >= 32 && x <= 96 && y >= 40 && y <= 90);
    if (inBox) {
      // Box body
      if (y >= 54) {
        if (x < 64) {
          // Left face (lighter)
          red = 241; green = 245; blue = 249;
        } else {
          // Right face (slight shadow)
          red = 203; green = 213; blue = 225;
        }
        // Center seam
        if (x === 64) {
          red = 148; green = 163; blue = 184;
        }
      } else {
        // Top flaps
        if (y < 46) {
          red = 248; green = 250; blue = 252;
        } else {
          red = 226; green = 232; blue = 240;
        }
      }
      // Box border
      if (x === 32 || x === 96 || y === 40 || y === 90) {
        red = 100; green = 116; blue = 139;
      }
    }

    // Draw "Clip" accent badge in center (circle or clip ribbon)
    const clipDx = x - 64;
    const clipDy = y - 64;
    const clipDist = Math.sqrt(clipDx * clipDx + clipDy * clipDy);
    if (clipDist <= 16) {
      // Indigo accent badge
      red = 99; green = 102; blue = 241;
      if (clipDist <= 12) {
        // Checkmark / clip center
        if (Math.abs(clipDx + clipDy - 2) <= 2 && clipDx >= -6 && clipDx <= 6) {
          red = 255; green = 255; blue = 255;
        }
        if (Math.abs(clipDx - clipDy + 4) <= 2 && clipDx >= -6 && clipDx <= 0) {
          red = 255; green = 255; blue = 255;
        }
      }
    }

    setPixel(x, y, red, green, blue, alpha);
  }
}

// Build standard PNG
function createPng(width, height, rawData) {
  const compressed = zlib.deflateSync(rawData);
  const signature = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);

  function chunk(type, data) {
    const len = Buffer.alloc(4);
    len.writeUInt32BE(data.length, 0);
    const typeBuf = Buffer.from(type, 'ascii');
    const crcBuf = Buffer.alloc(4);
    const full = Buffer.concat([typeBuf, data]);
    const crc = calcCrc(full);
    crcBuf.writeInt32BE(crc, 0);
    return Buffer.concat([len, typeBuf, data, crcBuf]);
  }

  // CRC32 table
  const crcTable = [];
  for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) {
      c = (c & 1) ? (0xedb88320 ^ (c >>> 1)) : (c >>> 1);
    }
    crcTable[n] = c;
  }
  function calcCrc(buf) {
    let c = 0xffffffff;
    for (let i = 0; i < buf.length; i++) {
      c = crcTable[(c ^ buf[i]) & 0xff] ^ (c >>> 8);
    }
    return (c ^ 0xffffffff) | 0;
  }

  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(width, 0);
  ihdr.writeUInt32BE(height, 4);
  ihdr[8] = 8; // bit depth
  ihdr[9] = 6; // RGBA
  ihdr[10] = 0; // compression
  ihdr[11] = 0; // filter
  ihdr[12] = 0; // interlace

  return Buffer.concat([
    signature,
    chunk('IHDR', ihdr),
    chunk('IDAT', compressed),
    chunk('IEND', Buffer.alloc(0))
  ]);
}

const pngBuffer = createPng(width, height, buffer);
const outDir = path.join(__dirname, '../assets');
if (!fs.existsSync(outDir)) {
  fs.mkdirSync(outDir, { recursive: true });
}
fs.writeFileSync(path.join(outDir, 'logo.png'), pngBuffer);
console.log('Logo generated at assets/logo.png, size:', pngBuffer.length);
