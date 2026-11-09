// Decodes the PNG data URLs poppler-native emits (8-bit RGB, 1-bit gray; not interlaced) into raw pixels, so tests
// can assert on what an image shows rather than on how it happens to be compressed.
const zlib = require('zlib');

function decodePng(buffer) {
  if (buffer.readUInt32BE(0) !== 0x89504e47) throw new Error('not a PNG');
  let ihdr;
  const idat = [];
  for (let pos = 8; pos < buffer.length; ) {
    const length = buffer.readUInt32BE(pos);
    const type = buffer.toString('latin1', pos + 4, pos + 8);
    const body = buffer.subarray(pos + 8, pos + 8 + length);
    if (type === 'IHDR') {
      ihdr = { width: body.readUInt32BE(0), height: body.readUInt32BE(4), depth: body[8], colorType: body[9] };
    } else if (type === 'IDAT') {
      idat.push(body);
    }
    pos += 12 + length;
  }
  const channels = { 0: 1, 2: 3 }[ihdr.colorType];
  const bpp = Math.max(1, (channels * ihdr.depth) / 8);
  const stride = Math.ceil((ihdr.width * channels * ihdr.depth) / 8);
  const raw = zlib.inflateSync(Buffer.concat(idat));
  const pixels = Buffer.alloc(stride * ihdr.height);
  for (let y = 0; y < ihdr.height; y++) {
    const filter = raw[y * (stride + 1)];
    for (let x = 0; x < stride; x++) {
      const value = raw[y * (stride + 1) + 1 + x];
      const a = x >= bpp ? pixels[y * stride + x - bpp] : 0;
      const b = y > 0 ? pixels[(y - 1) * stride + x] : 0;
      const c = x >= bpp && y > 0 ? pixels[(y - 1) * stride + x - bpp] : 0;
      const p = a + b - c;
      const paeth = Math.abs(p - a) <= Math.abs(p - b) && Math.abs(p - a) <= Math.abs(p - c) ? a : Math.abs(p - b) <= Math.abs(p - c) ? b : c;
      pixels[y * stride + x] = (value + [0, a, b, (a + b) >> 1, paeth][filter]) & 0xff;
    }
  }
  return { ...ihdr, pixels };
}

function decodeDataUrl(src) {
  const [header, data] = src.split(',');
  return { type: header.slice('data:'.length, header.indexOf(';')), bytes: Buffer.from(data, 'base64') };
}

module.exports = { decodePng, decodeDataUrl };
