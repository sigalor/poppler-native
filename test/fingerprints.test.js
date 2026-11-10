const fs = require('fs');
const path = require('path');
const native = require('./helpers/native');

// See test/fixtures/generate/make-test-font.py for what each glyph of this font is for.
const testFont = fs.readFileSync(path.join(__dirname, 'fixtures/fingerprint-test-font.ttf'));

describe('_referenceFingerprints', () => {
  let entries;
  let byCodepoints;
  beforeAll(() => {
    entries = native._referenceFingerprints(testFont);
    byCodepoints = new Map(entries.map(([fingerprint, codepoints]) => [codepoints.join(','), fingerprint]));
  });

  test('returns one entry per distinct outline reachable from the cmap', () => {
    expect(entries).toHaveLength(5);
    expect(new Set(entries.map(([fingerprint]) => fingerprint)).size).toBe(5);
    for (const [fingerprint] of entries) expect(fingerprint).toMatch(/^[0-9a-f]{16}$/);
  });

  test('codepoints mapped to the same glyph share an entry', () => {
    expect(byCodepoints.has('32,160')).toBe(true); // space, no-break space
    expect(byCodepoints.has('65,913')).toBe(true); // A, Greek capital alpha
    expect(byCodepoints.has('68')).toBe(true);
  });

  test('glyphs with identical outlines share an entry, the lower glyph index deciding the first codepoint', () => {
    expect(byCodepoints.has('66,1042')).toBe(true); // B (glyph 4) before B.dup (glyph 5)
    expect(byCodepoints.has('1057,67')).toBe(true); // C.alt (glyph 2, U+0421) before C (glyph 6, U+0043)
  });

  test('is deterministic', () => {
    expect(native._referenceFingerprints(testFont)).toEqual(entries);
  });

  test('rejects data that is not a font', () => {
    expect(() => native._referenceFingerprints(Buffer.from('not a font'))).toThrow(/FT_New_Memory_Face/);
  });
});
