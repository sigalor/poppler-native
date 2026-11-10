const { execFileSync } = require('child_process');
const path = require('path');

// Runs in the package's own directory, where Node resolves the name "poppler-native" to the package itself
// through its "exports" map: this is what `import ... from 'poppler-native'` gets in a user's project.
const root = path.join(__dirname, '..');
const run = (args, script) => execFileSync(process.execPath, [...args, '-e', script], { cwd: root, encoding: 'utf8' });
const file = JSON.stringify(path.join(__dirname, 'fixtures', 'minimal-text.pdf'));

test('can be imported as an ES module, by default and by name', () => {
  const out = run(
    ['--input-type=module'],
    `import popplerNative, { info, text } from 'poppler-native';
     const result = await info(${file});
     process.stdout.write(JSON.stringify([typeof popplerNative.info, typeof text, popplerNative.info === info, result.pages[0].strings[0].text]));`,
  );
  expect(JSON.parse(out)).toEqual(['function', 'function', true, 'Hello']);
});

test('can be required as a CommonJS module', () => {
  const out = run([], `const { info, text } = require('poppler-native'); process.stdout.write(typeof info + ' ' + typeof text);`);
  expect(out).toBe('function function');
});
