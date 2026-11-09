const { execFileSync } = require('child_process');
const path = require('path');

test('the TypeScript declarations type-check real usage and reject misuse', () => {
  const typescript = path.dirname(require.resolve('typescript/package.json'));
  const tsc = path.join(typescript, require('typescript/package.json').bin.tsc);
  const run = () => execFileSync(process.execPath, [tsc, '-p', path.join(__dirname, 'types')], { encoding: 'utf8' });
  expect(run).not.toThrow();
}, 60000);
