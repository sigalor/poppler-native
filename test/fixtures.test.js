const { execFileSync } = require('child_process');
const fs = require('fs');
const path = require('path');

const fixtures = path.join(__dirname, 'fixtures');
const inGit = (() => {
  try {
    execFileSync('git', ['rev-parse', '--is-inside-work-tree'], { cwd: __dirname, stdio: 'ignore' });
    return true;
  } catch {
    return false;
  }
})();

// A checkout with core.autocrlf (common on Windows) would convert the line endings of a fixture Git takes for text.
(inGit ? test : test.skip)('every fixture PDF and font is checked out as bytes', () => {
  const files = fs.readdirSync(fixtures).filter(name => /\.(pdf|ttf)$/.test(name));
  const attributes = execFileSync('git', ['check-attr', 'binary', '--', ...files], { cwd: fixtures, encoding: 'utf8' });
  const notBinary = attributes.split('\n').filter(line => line && !line.endsWith(': binary: set'));
  expect(files.length).toBeGreaterThan(10);
  expect(notBinary).toEqual([]);
});
