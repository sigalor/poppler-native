const path = require('path');
const pdf = require('../');

const fixture = name => path.join(__dirname, 'fixtures', name);

describe('text()', () => {
  test('reads every page in reading order, each page ending with a form feed, like pdftotext', async () => {
    expect(await pdf.text(fixture('three-pages.pdf'))).toBe('Page 1\nVisit\nGo to page 3\n\n\fPage 2\n\n\fPage 3\n\n\f');
  });

  test('reads only the pages asked for', async () => {
    expect(await pdf.text(fixture('three-pages.pdf'), { firstPage: 2, lastPage: 2 })).toBe('Page 2\n\n\f');
  });

  test('keeps the physical layout when asked to', async () => {
    const text = await pdf.text(fixture('three-pages.pdf'), { layout: true, lastPage: 1 });
    expect(text.split('\n').map(line => line.trim()).filter(Boolean)).toEqual(['Page 1', 'Visit', 'Go to page 3']);
    expect(text.endsWith('\f')).toBe(true);
  });

  test('opens an encrypted document with its password', async () => {
    await expect(pdf.text(fixture('encrypted-user.pdf'))).rejects.toThrow('password');
    expect(await pdf.text(fixture('encrypted-user.pdf'), { password: 'secret', lastPage: 1 })).toMatch(/^Page 1\n/);
  });

  test('repairs Arial text the same way info() does', async () => {
    expect(await pdf.text(fixture('arial-gid-codes.pdf'))).toBe('Hello Wörld 123\n\nHello Wörld 123\n\n\f');
  });

  test('validates its arguments like info()', async () => {
    await expect(pdf.text()).rejects.toThrow('wrong number of arguments: expected 1-2, got 0');
    await expect(pdf.text(3)).rejects.toThrow('argument at position 1 has wrong type: expected string or Buffer, got number');
    await expect(pdf.text(fixture('three-pages.pdf'), { images: false })).rejects.toThrow('unknown option "images"');
  });
});
