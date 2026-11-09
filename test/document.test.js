const path = require('path');
const pdf = require('../');

const fixture = name => path.join(__dirname, 'fixtures', name);

describe('three-pages.pdf', () => {
  let info;
  beforeAll(async () => {
    info = await pdf.info(fixture('three-pages.pdf'));
  });
  const string = text => info.pages.flatMap(page => page.strings).find(s => s.text === text);

  test('reads the metadata, decoding UTF-16 and PDFDocEncoding strings', () => {
    expect(info.meta).toEqual({
      Title: 'Über ✓ Test',
      Author: 'poppler-native',
      Subject: 'Test fixture',
      Creator: 'make-fixtures.py',
      CreationDate: 'D:20260101120000Z',
    });
  });

  test('reads the outline with its titles, target pages and nesting', () => {
    expect(info.outline).toEqual([
      { title: 'Chapter 1 – Überblick', page: 1, children: [{ title: 'Section 1.1', page: 2 }] },
      { title: 'Chapter 2', page: 3 },
    ]);
  });

  test('attaches a URI link to the text it covers', () => {
    expect(string('Visit').link).toBe('https://example.com/poppler-native');
  });

  test('attaches an internal link as "this#<page>"', () => {
    expect(string('Go to page 3').link).toBe('this#3');
  });

  test('leaves text without a link without a link property', () => {
    expect(string('Page 2')).not.toHaveProperty('link');
  });

  test('numbers the pages and measures them in pixels at 108 DPI (1.5 x PDF points)', () => {
    expect(info.pages.map(({ number, width, height }) => ({ number, width, height }))).toEqual([
      { number: 1, width: 892, height: 1263 },
      { number: 2, width: 892, height: 1263 },
      { number: 3, width: 892, height: 1263 },
    ]);
  });
});
