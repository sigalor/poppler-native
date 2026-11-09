const { execFileSync } = require('child_process');
const path = require('path');
const pdf = require('../');

// Both fixtures embed an Arial subset without a ToUnicode map; see test/fixtures/generate/make-arial-pdfs.py.
const fixture = name => path.join(__dirname, 'fixtures', name);
const texts = info => info.pages[0].strings.map(s => s.text);

describe('text drawn with an embedded Arial subset that has no ToUnicode map', () => {
  test('glyph-index codes of a Type0 /Identity-H font are read by the shape of the glyph drawn', async () => {
    const info = await pdf.info(fixture('arial-gid-codes.pdf'));
    expect(texts(info)[0]).toBe('Hello Wörld 123');
  });

  test('glyph-index codes Poppler finds no Unicode for at all are read too', async () => {
    const info = await pdf.info(fixture('arial-gid-codes.pdf'));
    expect(texts(info)).toEqual(['Hello Wörld 123', 'Hello Wörld 123']);
  });

  test('text Poppler decodes correctly on its own is left alone', async () => {
    const info = await pdf.info(fixture('arial-encoded.pdf'));
    expect(texts(info)).toEqual(['Hello Wörld 123', 'А']);
  });

  test('a decoded letter that shares its outline with another script keeps its own codepoint', async () => {
    const info = await pdf.info(fixture('arial-encoded.pdf'));
    expect(texts(info)[1].codePointAt(0)).toBe(0x410); // CYRILLIC CAPITAL LETTER A, not LATIN A (same outline)
  });

  test('needs no external program', () => {
    const script = `require(${JSON.stringify(path.join(__dirname, '..'))}).info(${JSON.stringify(
      fixture('arial-gid-codes.pdf'),
    )}).then(i => process.stdout.write(i.pages[0].strings[0].text))`;
    const out = execFileSync(process.execPath, ['-e', script], { env: { PATH: '' }, encoding: 'utf8' });
    expect(out).toBe('Hello Wörld 123');
  });
});
