// Runs on `npm install`. Uses the prebuilt binary for this platform when the package ships one -- the usual case,
// nothing is compiled -- and compiles from source otherwise, or when asked to with --build-from-source.
const { spawnSync } = require('child_process');
const path = require('path');

const root = path.join(__dirname, '..');

function hasBinary() {
  try {
    require('node-gyp-build').path(root);
    return true;
  } catch {
    return false;
  }
}

const fromSource = /^(1|true)$/i.test(process.env.npm_config_build_from_source || '');
if (!fromSource && hasBinary()) process.exit(0);

// npm and pnpm tell their scripts where their own node-gyp is; Yarn puts a node-gyp shim on the PATH instead.
const nodeGyp = process.env.npm_config_node_gyp;
const [command, args] =
  nodeGyp && nodeGyp.endsWith('.js') ? [process.execPath, [nodeGyp, 'rebuild']] : [nodeGyp || 'node-gyp', ['rebuild']];
const result = spawnSync(command, args, {
  cwd: root,
  stdio: 'inherit',
  env: { ...process.env, JOBS: process.env.JOBS || 'max' }, // compile on every core
  shell: process.platform === 'win32' && command !== process.execPath,
});
if (result.status === 0) process.exit(0);

const why = fromSource
  ? 'compiling it failed (see above).'
  : `there is no prebuilt binary for ${process.platform}-${process.arch}, and compiling one failed (see above).`;
console.error(`
poppler-native: ${why}
Compiling needs Python 3 and a C++23 compiler: GCC 12 or newer, Clang 18 or newer, Xcode 16.4 or newer, or Visual
Studio 2022 or 2026 with the "Desktop development with C++" workload (Visual Studio 2026 needs npm 11 or newer).
More: https://github.com/sigalor/poppler-native#building-from-source
`);
process.exit(1);
