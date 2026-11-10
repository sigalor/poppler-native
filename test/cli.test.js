const { spawnSync } = require('child_process');
const fs = require('fs');
const path = require('path');

const cli = path.join(__dirname, '..', 'bin', 'poppler-native.js');
const fixture = name => path.join(__dirname, 'fixtures', name);
const run = (args, options = {}) => spawnSync(process.execPath, [cli, ...args], { encoding: 'utf8', ...options });

test('prints the document as JSON', () => {
  const { status, stdout } = run([fixture('three-pages.pdf')]);
  expect(status).toBe(0);
  expect(JSON.parse(stdout).numPages).toBe(3);
  expect(stdout).not.toContain('\n  ');
});

test('--pretty indents the JSON', () => {
  expect(run([fixture('minimal-text.pdf'), '--pretty']).stdout).toContain('\n  "meta": {');
});

test('--text prints the text like pdftotext', () => {
  expect(run([fixture('three-pages.pdf'), '--text']).stdout).toBe('Page 1\nVisit\nGo to page 3\n\n\fPage 2\n\n\fPage 3\n\n\f');
});

test('--first-page and --last-page select pages', () => {
  expect(run([fixture('three-pages.pdf'), '--text', '--first-page', '2', '--last-page', '2']).stdout).toBe('Page 2\n\n\f');
});

test('--password opens an encrypted document', () => {
  expect(run([fixture('encrypted-user.pdf'), '--text', '--password', 'secret', '--last-page', '1']).stdout).toMatch(/^Page 1\n/);
});

test('--no-images leaves images out', () => {
  expect(JSON.parse(run([fixture('images.pdf'), '--no-images']).stdout).pages[0].images).toEqual([]);
});

test('reads the PDF from standard input when the file is "-"', () => {
  const { stdout } = run(['-'], { input: fs.readFileSync(fixture('minimal-text.pdf')) });
  expect(JSON.parse(stdout).pages[0].strings[0].text).toBe('Hello');
});

test('reports an unreadable PDF on stderr with exit code 1', () => {
  const { status, stdout, stderr } = run([fixture('not-a-pdf.pdf')]);
  expect(status).toBe(1);
  expect(stdout).toBe('');
  expect(stderr).toContain("PDF file was damaged and couldn't be repaired");
});

test('--help explains the usage', () => {
  const { status, stdout } = run(['--help']);
  expect(status).toBe(0);
  expect(stdout).toContain('Usage: poppler-native <file.pdf | -> [options]');
});

test('--version prints the package version', () => {
  expect(run(['--version']).stdout).toBe(`${require('../package.json').version}\n`);
});

test('a missing file argument is a usage error', () => {
  const { status, stderr } = run([]);
  expect(status).toBe(2);
  expect(stderr).toContain('Usage: poppler-native');
});

// `poppler-native file.pdf --text | head` closes the pipe before the output is written: that is not an error.
test('stops quietly when its output pipe is closed', async () => {
  const { spawn } = require('child_process');
  const child = spawn(process.execPath, [cli, fixture('images.pdf')], { stdio: ['ignore', 'pipe', 'pipe'] });
  child.stdout.destroy(); // the reader goes away before anything is written
  let stderr = '';
  child.stderr.on('data', chunk => (stderr += chunk));
  const status = await new Promise(resolve => child.on('close', resolve));
  expect(stderr).toBe('');
  expect(status).toBe(0);
});
