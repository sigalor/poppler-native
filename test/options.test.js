const fs = require('fs');
const path = require('path');
const pdf = require('../');

const fixture = name => path.join(__dirname, 'fixtures', name);
const firstTexts = info => info.pages.map(page => page.strings[0].text);

describe('numPages', () => {
  test('is the number of pages in the document', async () => {
    expect((await pdf.info(fixture('three-pages.pdf'))).numPages).toBe(3);
  });
});

describe('firstPage and lastPage', () => {
  test('select the pages that are read', async () => {
    const info = await pdf.info(fixture('three-pages.pdf'), { firstPage: 2, lastPage: 2 });
    expect(info.pages.map(page => page.number)).toEqual([2]);
    expect(firstTexts(info)).toEqual(['Page 2']);
    expect(info.numPages).toBe(3);
  });

  test('firstPage alone reads to the end', async () => {
    expect(firstTexts(await pdf.info(fixture('three-pages.pdf'), { firstPage: 2 }))).toEqual(['Page 2', 'Page 3']);
  });

  test('lastPage alone reads from the start', async () => {
    expect(firstTexts(await pdf.info(fixture('three-pages.pdf'), { lastPage: 1 }))).toEqual(['Page 1']);
  });

  test('a lastPage beyond the end reads to the end', async () => {
    const info = await pdf.info(fixture('three-pages.pdf'), { firstPage: 3, lastPage: 99 });
    expect(info.pages.map(page => page.number)).toEqual([3]);
  });

  test('a firstPage beyond the end reads no pages', async () => {
    const info = await pdf.info(fixture('three-pages.pdf'), { firstPage: 4 });
    expect(info.pages).toEqual([]);
    expect(info.numPages).toBe(3);
  });

  test('only keep the fonts of the pages read', async () => {
    const info = await pdf.info(fixture('three-pages.pdf'), { firstPage: 2, lastPage: 2 });
    expect(info.fonts.map(font => font.family)).toEqual(['Helvetica']);
  });

  test('must be positive integers', async () => {
    const file = fixture('three-pages.pdf');
    await expect(pdf.info(file, { firstPage: 0 })).rejects.toThrow('firstPage must be a positive integer, got 0');
    await expect(pdf.info(file, { lastPage: 1.5 })).rejects.toThrow('lastPage must be a positive integer, got 1.5');
    await expect(pdf.info(file, { firstPage: '2' })).rejects.toThrow('firstPage must be a positive integer, got string');
  });

  test('must not be reversed', async () => {
    await expect(pdf.info(fixture('three-pages.pdf'), { firstPage: 3, lastPage: 2 })).rejects.toThrow(
      'firstPage (3) is after lastPage (2)',
    );
  });
});

describe('images: false', () => {
  test('leaves images out', async () => {
    const info = await pdf.info(fixture('images.pdf'), { images: false });
    expect(info.pages[0].images).toEqual([]);
  });

  test('images are read by default', async () => {
    expect((await pdf.info(fixture('images.pdf'))).pages[0].images).toHaveLength(5);
  });

  test('must be a boolean', async () => {
    await expect(pdf.info(fixture('images.pdf'), { images: 0 })).rejects.toThrow('images must be a boolean, got number');
  });
});

describe('password', () => {
  const encrypted = fixture('encrypted-user.pdf');

  test('a document with a user password is refused without it', async () => {
    await expect(pdf.info(encrypted)).rejects.toThrow('file was encrypted and password was incorrect or not supplied');
  });

  test('a document with a user password is refused with a wrong one', async () => {
    await expect(pdf.info(encrypted, { password: 'wrong' })).rejects.toThrow(
      'file was encrypted and password was incorrect or not supplied',
    );
  });

  test('opens with the user password', async () => {
    expect(firstTexts(await pdf.info(encrypted, { password: 'secret' }))).toEqual(['Page 1', 'Page 2', 'Page 3']);
  });

  test('opens with the owner password', async () => {
    expect(firstTexts(await pdf.info(encrypted, { password: 'owner-secret' }))).toEqual(['Page 1', 'Page 2', 'Page 3']);
  });

  test('a document with only an owner password opens without one', async () => {
    expect(firstTexts(await pdf.info(fixture('encrypted-owner.pdf')))).toEqual(['Page 1', 'Page 2', 'Page 3']);
  });

  test('must be a string', async () => {
    await expect(pdf.info(encrypted, { password: 1234 })).rejects.toThrow('password must be a string, got number');
  });
});

describe('inputs', () => {
  const bytes = fs.readFileSync(fixture('minimal-text.pdf'));
  const text = info => info.pages[0].strings[0].text;

  test('a Uint8Array', async () => {
    expect(text(await pdf.info(new Uint8Array(bytes)))).toBe('Hello');
  });

  test('a Uint8Array viewing part of a larger buffer', async () => {
    const larger = Buffer.concat([Buffer.alloc(100, 0x20), bytes, Buffer.alloc(50, 0x20)]);
    expect(text(await pdf.info(new Uint8Array(larger.buffer, larger.byteOffset + 100, bytes.length)))).toBe('Hello');
  });

  test('an ArrayBuffer', async () => {
    const arrayBuffer = bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.length);
    expect(text(await pdf.info(arrayBuffer))).toBe('Hello');
  });

  test('an unknown option is an error, not silently ignored', async () => {
    await expect(pdf.info(bytes, { firstpage: 2 })).rejects.toThrow('unknown option "firstpage"');
  });
});
