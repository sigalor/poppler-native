require('./jest-setup');
const pdf = require('../');
const path = require('path');

test('wrong number of parameters throws an error', () => {
  expect(pdf.info()).rejects.toThrow('wrong number of arguments: expected 1, got 0');
  expect(pdf.info('a', 'b')).rejects.toThrow('wrong number of arguments: expected 1, got 2');
});

test('parameters of wrong type throws an error', () => {
  expect(pdf.info(3)).rejects.toThrow('argument at position 1 has wrong type: expected string, got number');
  expect(pdf.info({ a: 1 })).rejects.toThrow('argument at position 1 has wrong type: expected string, got object');
});

test('missing input filename throws an error', () => {
  expect(pdf.info('does-not-exist')).rejects.toThrow(
    "does-not-exist: couldn't open the PDF file: No such file or directory",
  );
});

test('invalid PDF file throws an error', () => {
  expect(pdf.info(path.join(__dirname, 'pdfs/not-a-pdf.pdf'))).rejects.toThrow(
    "pdfs/not-a-pdf.pdf: PDF file was damaged and couldn't be repaired",
  );
});

describe('for minimal-text.pdf', () => {
  let info;
  beforeAll(async () => (info = await pdf.info(path.join(__dirname, 'pdfs/minimal-text.pdf'))));

  test('correctly reads metadata', () => {
    expect(info.meta).toMatchObject({
      CreationDate: "D:20200127182541+00'00'",
      Creator:
        'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/79.0.3945.131 Safari/537.36',
      ModDate: "D:20200127182541+00'00'",
      Producer: 'Skia/PDF m79',
    });
  });

  test('detects lack of outline', () => {
    expect(info).not.toHaveProperty('outline');
  });

  test('correctly reads page info', () => {
    expect(info.pages).toBeType('array');
    expect(info.pages.length).toBe(1);
    expect(info.pages[0].number).toBe(1);
    expect(info.pages[0].width).toBe(893);
    expect(info.pages[0].height).toBe(1264);
  });

  test('correctly reads contained font', () => {
    expect(info.pages[0].fonts).toBeType('array');
    expect(info.pages[0].fonts.length).toBe(1);
    expect(info.pages[0].fonts[0]).toMatchObject({
      id: 0,
      size: 20,
      family: 'TimesNewRomanPSMT',
      color: '#000000',
      bold: false,
      italic: false,
    });
  });

  test('detects no images', () => {
    expect(info.pages[0].images).toBeType('array');
    expect(info.pages[0].images.length).toBe(0);
  });

  test('correctly reads contained string', () => {
    expect(info.pages[0].strings).toBeType('array');
    expect(info.pages[0].strings.length).toBe(1);
    expect(info.pages[0].strings[0].top).toBeCloseTo(54.213);
    expect(info.pages[0].strings[0].left).toBeCloseTo(54.57);
    expect(info.pages[0].strings[0].width).toBeCloseTo(49.188);
    expect(info.pages[0].strings[0].height).toBeCloseTo(24.518);
    expect(info.pages[0].strings[0]).toMatchObject({
      font: 0,
      text: 'Hello',
    });
  });
});
