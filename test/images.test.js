const path = require('path');
const pdf = require('../');
const { decodeDataUrl, decodePng } = require('./helpers/png');

// See test/fixtures/generate/make-fixtures.py, images(), for how each image is drawn.
const fixture = name => path.join(__dirname, 'fixtures', name);

describe('images.pdf', () => {
  let images;
  beforeAll(async () => {
    images = (await pdf.info(fixture('images.pdf'))).pages[0].images;
  });
  const png = index => {
    const { type, bytes } = decodeDataUrl(images[index].src);
    expect(type).toBe('image/png');
    return decodePng(bytes);
  };

  test('reads every image, in drawing order, positioned in pixels at 108 DPI', () => {
    expect(images).toHaveLength(5);
    expect(images.map(({ left, top, width, height }) => ({ left, top, width, height }))).toEqual(
      [0, 1, 2, 3, 4].map(i => ({ left: (60 + 100 * i) * 1.5, top: (842 - 660) * 1.5, width: 120, height: 90 })),
    );
  });

  test('passes a JPEG through unchanged', () => {
    const { type, bytes } = decodeDataUrl(images[0].src);
    expect(type).toBe('image/jpeg');
    expect(bytes.length).toBe(665);
    expect(bytes.subarray(0, 2)).toEqual(Buffer.from([0xff, 0xd8]));
    expect(bytes.subarray(-2)).toEqual(Buffer.from([0xff, 0xd9]));
  });

  test('re-encodes an RGB image as a PNG with its exact pixels', () => {
    const image = png(1);
    expect([image.width, image.height, image.depth, image.colorType]).toEqual([4, 3, 8, 2]);
    expect([...image.pixels]).toEqual([
      255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 0,
      0, 255, 255, 255, 0, 255, 0, 0, 0, 255, 255, 255,
      128, 128, 128, 64, 64, 64, 32, 32, 32, 16, 16, 16,
    ]);
  });

  test('re-encodes an image mask as a 1-bit PNG, opaque where the mask paints', () => {
    const image = png(2);
    expect([image.width, image.height, image.depth, image.colorType]).toEqual([8, 2, 1, 0]);
    expect([...image.pixels]).toEqual([0b01010101, 0b10101010]);
  });

  test('decodes a JPEG 2000 image', () => {
    const image = png(3);
    expect([image.width, image.height, image.depth, image.colorType]).toEqual([5, 4, 8, 2]);
    const expected = [];
    for (let y = 0; y < 4; y++) for (let x = 0; x < 5; x++) expected.push(x * 50, y * 60, 200);
    expect([...image.pixels]).toEqual(expected);
  });

  test('converts a CMYK JPEG to an RGB PNG', () => {
    const image = png(4);
    expect([image.width, image.height, image.depth, image.colorType]).toEqual([4, 4, 8, 2]);
    const first = [...image.pixels.subarray(0, 3)];
    for (let i = 0; i < 16; i++) expect([...image.pixels.subarray(3 * i, 3 * i + 3)]).toEqual(first);
    const [r, g, b] = first;
    expect(r).toBeGreaterThan(g); // the fill is orange: no cyan, half magenta, full yellow
    expect(g).toBeGreaterThan(b);
  });
});
