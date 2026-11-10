const fs = require('fs');
const os = require('os');
const path = require('path');
const pdf = require('../');

const fixture = name => path.join(__dirname, 'fixtures', name);

test('reads a file whose name is not ASCII', async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'poppler-native-test-'));
  const file = path.join(dir, 'Überweisung € 日本.pdf');
  fs.copyFileSync(fixture('minimal-text.pdf'), file);
  try {
    const info = await pdf.info(file);
    expect(info.pages[0].strings[0].text).toBe('Hello');
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
});

test('a directory is reported as a file that cannot be opened', async () => {
  await expect(pdf.info(__dirname)).rejects.toThrow(`${__dirname}: couldn't open the PDF file`);
});
