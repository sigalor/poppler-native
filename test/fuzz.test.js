const { spawnSync } = require('child_process');
const path = require('path');

// A quick pass of scripts/fuzz.mjs (run it with more --cases for a deeper one): mutated PDFs must reject or parse,
// never crash Node.
test('mutated PDFs never crash the process', () => {
  const script = path.join(__dirname, '..', 'scripts', 'fuzz.mjs');
  const result = spawnSync(process.execPath, [script, '--cases', '25', '--seed', '20261109'], { encoding: 'utf8' });
  const summary = JSON.parse(result.stdout.trim().split('\n').pop());
  expect(summary.crashes).toEqual([]);
  expect(summary.cases).toBeGreaterThan(100);
}, 600000);
