const { execFileSync } = require('child_process');
const fs = require('fs');
const os = require('os');
const path = require('path');
const pdf = require('../');

const fixture = name => path.join(__dirname, 'fixtures', name);
const root = path.join(__dirname, '..');
const ghostscript = process.platform === 'win32' ? 'gswin64c' : 'gs';
const hasGhostscript = (() => {
  try {
    execFileSync(ghostscript, ['--version'], { stdio: 'ignore' });
    return true;
  } catch {
    return false;
  }
})();
const describeWithGhostscript = hasGhostscript ? describe : describe.skip;
const texts = info => info.pages.flatMap(page => page.strings.map(s => s.text));

// Runs `body` (JS source using `pdf`) in a fresh Node process with the given environment; returns its stdout.
function inChild(body, env) {
  const script = `const pdf = require(${JSON.stringify(root)}); ${body}`;
  return execFileSync(process.execPath, ['-e', script], { env, encoding: 'utf8' });
}

// What Ghostscript itself makes of a PDF with the two commands reconvertThroughPS runs.
function ghostscriptRoundTrip(file) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'poppler-native-test-'));
  try {
    execFileSync(ghostscript, ['-o', path.join(dir, 'converted.ps'), '-dNOCACHE', '-sDEVICE=ps2write', file], { stdio: 'ignore' });
    execFileSync(ghostscript, ['-o', path.join(dir, 'reconverted.pdf'), '-sDEVICE=pdfwrite', path.join(dir, 'converted.ps')], { stdio: 'ignore' });
    return fs.readFileSync(path.join(dir, 'reconverted.pdf'));
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
}
// The document without its metadata, which Ghostscript stamps with the time of the conversion.
const content = ({ meta, ...rest }) => rest;

describeWithGhostscript('reconvertThroughPS', () => {
  let expected;
  beforeAll(async () => {
    expected = content(await pdf.info(ghostscriptRoundTrip(fixture('minimal-text.pdf'))));
  });

  test('parses exactly what Ghostscript makes of the file', async () => {
    const reconverted = await pdf.info(fixture('minimal-text.pdf'), { reconvertThroughPS: true });
    expect(content(reconverted)).toEqual(expected);
  });

  test('parses exactly what Ghostscript makes of a Buffer', async () => {
    const reconverted = await pdf.info(fs.readFileSync(fixture('minimal-text.pdf')), { reconvertThroughPS: true });
    expect(content(reconverted)).toEqual(expected);
  });

  test("rejects a file that isn't a PDF with Ghostscript's reason", async () => {
    await expect(pdf.info(fixture('not-a-pdf.pdf'), { reconvertThroughPS: true })).rejects.toThrow(/Ghostscript/);
  });

  test('leaves no temporary files behind', () => {
    const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'poppler-native-tmpdir-'));
    try {
      const env = { ...process.env, TMPDIR: tmp, TEMP: tmp, TMP: tmp };
      const out = inChild(
        `pdf.info(${JSON.stringify(fixture('minimal-text.pdf'))}, { reconvertThroughPS: true }).then(i => process.stdout.write(String(i.pages.length)))`,
        env,
      );
      expect(out).toBe('1');
      expect(fs.readdirSync(tmp)).toEqual([]);
    } finally {
      fs.rmSync(tmp, { recursive: true, force: true });
    }
  });
});

test('reconvertThroughPS without Ghostscript installed says what is missing', () => {
  const out = inChild(
    `pdf.info(${JSON.stringify(fixture('minimal-text.pdf'))}, { reconvertThroughPS: true }).then(() => process.stdout.write('resolved'), e => process.stdout.write(e.message))`,
    { PATH: '' },
  );
  expect(out).toMatch(/reconvertThroughPS requires Ghostscript/);
});
