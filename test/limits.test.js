const buffer = require('buffer');
const path = require('path');
const v8 = require('v8');
const zlib = require('zlib');
const pdf = require('../');
const native = require('../lib/native');
const { buildPdf } = require('./helpers/pdf');

const fixture = name => path.join(__dirname, 'fixtures', name);

// One page with one "JPEG" behind a Flate filter, drawn `draws` times: a few kilobytes that inflate to `size` bytes,
// which info() returns as they come out of Flate, once per drawing.
function flateJpegPdf(size, draws = 1) {
  const data = zlib.deflateSync(Buffer.alloc(size), { level: 9 }).toString('latin1');
  const content = 'q 100 0 0 100 0 0 cm /Im0 Do Q\n'.repeat(draws);
  return buildPdf([
    '<< /Type /Catalog /Pages 2 0 R >>',
    '<< /Type /Pages /Kids [3 0 R] /Count 1 >>',
    '<< /Type /Page /Parent 2 0 R /MediaBox [0 0 200 200] /Resources << /XObject << /Im0 5 0 R >> >> /Contents 4 0 R >>',
    `<< /Length ${content.length} >>\nstream\n${content}endstream`,
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

  // A data URL of n bytes is 23 + 4 * ceil(n / 3) characters long; with 1 MiB to fit in, n can be 786,414.
  test('an image exactly at the limit is returned, one byte more is not', async () => {
    const { pages } = await native.info(flateJpegPdf(786414), { _maxStringLength: megabyte });
    expect(pages[0].images[0].src).toHaveLength(megabyte - 1);
    await expect(native.info(flateJpegPdf(786415), { _maxStringLength: megabyte })).rejects.toThrow(
      'page 1 has an image too large to return as a data URL',
    );
  });

  // Each image within the limit, but all of them together too much for the JavaScript heap: index.js passes a budget
  // for the images of one document (a quarter of the heap), lowered here too.
  test('images too large together reject, naming the way out', async () => {
    const limits = { _maxStringLength: megabyte, _maxImageBytes: 4 * megabyte };
    await expect(native.info(flateJpegPdf(megabyte / 2, 10), limits)).rejects.toThrow(
      'the images are too large together to return as data URLs; read the document with { images: false }',
    );
    expect((await native.info(flateJpegPdf(megabyte / 2, 4), limits)).pages[0].images).toHaveLength(4);
  });

  test('text too long for a string rejects, naming the way out', async () => {
    await expect(native.text(fixture('three-pages.pdf'), { _maxStringLength: 10 })).rejects.toThrow(
      'the text is too long for a JavaScript string; read fewer pages at a time with firstPage and lastPage',
    );
  });

  test('a 16 MiB image comes back whole', async () => {
    const { pages } = await pdf.info(flateJpegPdf(16 * megabyte));
    expect(pages[0].images[0].src).toHaveLength('data:image/jpeg;base64,'.length + 4 * Math.ceil((16 * megabyte) / 3));
  });

  test('index.js passes the real limits on every call', async () => {
    const expected = {
      _maxStringLength: buffer.constants.MAX_STRING_LENGTH,
      _maxImageBytes: Math.floor(v8.getHeapStatistics().heap_size_limit / 4),
    };
    const info = jest.spyOn(native, 'info');
    const text = jest.spyOn(native, 'text');
    try {
      await pdf.info(fixture('minimal-text.pdf'));
      await pdf.text(fixture('minimal-text.pdf'));
      await pdf.info(fixture('minimal-text.pdf'), { reconvertThroughPS: true }).catch(() => {}); // with or without gs
      const calls = [...info.mock.calls, ...text.mock.calls];
      expect(calls.length).toBeGreaterThanOrEqual(3);
      for (const [, options] of calls) expect(options).toMatchObject(expected);
    } finally {
      info.mockRestore();
      text.mockRestore();
    }
  });
});
