const path = require('path');
const zlib = require('zlib');
const pdf = require('../');
const native = require('../lib/native');
const { buildPdf } = require('./helpers/pdf');

const fixture = name => path.join(__dirname, 'fixtures', name);

// One page with one "JPEG" behind a Flate filter: a few kilobytes that inflate to `size` bytes, which info() returns
// as they come out of Flate.
function flateJpegPdf(size) {
  const data = zlib.deflateSync(Buffer.alloc(size), { level: 9 }).toString('latin1');
  return buildPdf([
    '<< /Type /Catalog /Pages 2 0 R >>',
    '<< /Type /Pages /Kids [3 0 R] /Count 1 >>',
    '<< /Type /Page /Parent 2 0 R /MediaBox [0 0 200 200] /Resources << /XObject << /Im0 5 0 R >> >> /Contents 4 0 R >>',
    '<< /Length 31 >>\nstream\nq 100 0 0 100 0 0 cm /Im0 Do Q\nendstream',
    '<< /Type /XObject /Subtype /Image /Width 8 /Height 8 /ColorSpace /DeviceRGB /BitsPerComponent 8 ' +
      `/Filter [/FlateDecode /DCTDecode] /Length ${data.length} >>\nstream\n${data}\nendstream`,
  ]);
}

// What can be returned is bounded by the longest string JavaScript holds (buffer.constants.MAX_STRING_LENGTH, about
// 512 MiB): index.js passes it down. Here it is set much lower, to test the limits without gigabytes of memory.
describe('results too large for a JavaScript string', () => {
  const megabyte = 1 << 20;

  test('an image whose data URL would be too long rejects, naming the way out', async () => {
    await expect(native.info(flateJpegPdf(16 * megabyte), { _maxStringLength: megabyte })).rejects.toThrow(
      'page 1 has an image too large to return as a data URL; read the document with { images: false }',
    );
    await expect(native.info(flateJpegPdf(16 * megabyte), { _maxStringLength: megabyte, images: false })).resolves.toMatchObject({
      numPages: 1,
    });
  });

  test('an image just within the limit is returned', async () => {
    const { pages } = await native.info(flateJpegPdf(megabyte / 2), { _maxStringLength: megabyte });
    expect(pages[0].images[0].src).toHaveLength('data:image/jpeg;base64,'.length + (4 * Math.ceil(megabyte / 2 / 3)));
  });

  test('text too long for a string rejects, naming the way out', async () => {
    await expect(native.text(fixture('three-pages.pdf'), { _maxStringLength: 10 })).rejects.toThrow(
      'the text is too long for a JavaScript string; read fewer pages at a time with firstPage and lastPage',
    );
  });

  test('the real limit is passed down: a 16 MiB image comes back whole', async () => {
    const { pages } = await pdf.info(flateJpegPdf(16 * megabyte));
    expect(pages[0].images[0].src).toHaveLength('data:image/jpeg;base64,'.length + 4 * Math.ceil((16 * megabyte) / 3));
  });
});
