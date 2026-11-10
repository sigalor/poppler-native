// Runs on `npm install`. Uses the prebuilt binary for this platform when the package ships one that loads here -- the
// usual case, nothing is compiled -- and compiles from source otherwise: when there is none, when it doesn't load on
// this system (a musl distribution other than Alpine, say), or when asked to with --build-from-source.
const { spawnSync } = require('child_process');
const os = require('os');
const path = require('path');

const root = path.join(__dirname, '..');

// Whether the binary node-gyp-build picks for this platform loads here.
function binaryLoads() {
  try {
    require(require('node-gyp-build').path(root));
    return true;
  } catch {
    return false;
  }
}

// How many compilers to run at once: one per core, but no more than memory allows, about 1 GiB each for Poppler's
// larger files. A container often sees all of the machine's cores and only part of its memory.
function compileJobs(cores, memory) {
  return Math.max(1, Math.min(cores, Math.floor(memory / 2 ** 30)));
}

function main() {
  const fromSource = /^(1|true)$/i.test(process.env.npm_config_build_from_source || '');
  if (!fromSource && binaryLoads()) return 0;

  // npm and pnpm tell their scripts where their own node-gyp is; Yarn puts a node-gyp shim on the PATH instead.
  const nodeGyp = process.env.npm_config_node_gyp;
  const [command, args] =
    nodeGyp && nodeGyp.endsWith('.js') ? [process.execPath, [nodeGyp, 'rebuild']] : [nodeGyp || 'node-gyp', ['rebuild']];
  // the machine's memory, or the process's limit when there is one (a container's)
  const memory = Math.min(os.totalmem(), process.constrainedMemory() || Infinity);
  const jobs = process.env.JOBS || String(compileJobs(os.availableParallelism(), memory));
  const result = spawnSync(command, args, {
    cwd: root,
    stdio: 'inherit',
    env: { ...process.env, JOBS: jobs },
    shell: process.platform === 'win32' && command !== process.execPath,
  });
  if (result.status === 0) return 0;

  let why;
  if (result.error) why = `couldn't run ${command}, which compiles it: ${result.error.message}.`;
  else if (fromSource) why = 'compiling it failed (see above).';
  else why = `no prebuilt binary for ${process.platform}-${process.arch} works here, and compiling one failed (see above).`;
  console.error(`
poppler-native: ${why}
Compiling needs Python 3 and a C++23 compiler: GCC 12 or newer, Clang 18 or newer, Xcode 16.4 or newer, or Visual
Studio 2022 or 2026 with the "Desktop development with C++" workload (Visual Studio 2026 needs npm 11 or newer).
More: https://github.com/sigalor/poppler-native#building-from-source
`);
  return 1;
}

if (require.main === module) process.exitCode = main();

module.exports = { compileJobs };
