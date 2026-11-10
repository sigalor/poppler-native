const { spawnSync } = require('child_process');
const fs = require('fs');
const os = require('os');
const path = require('path');

const root = path.join(__dirname, '..');

// A throwaway copy of the package: scripts/install.js, a package.json, the real node_modules, and optionally a
// prebuilt binary. Its compiler is a fake node-gyp that only records that it ran.
function scratchPackage(binary) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'poppler-native-install-'));
  fs.mkdirSync(path.join(dir, 'scripts'));
  fs.copyFileSync(path.join(root, 'scripts', 'install.js'), path.join(dir, 'scripts', 'install.js'));
  fs.writeFileSync(path.join(dir, 'package.json'), JSON.stringify({ name: 'poppler-native' }));
  fs.symlinkSync(path.join(root, 'node_modules'), path.join(dir, 'node_modules'), 'junction');
  if (binary) {
    const prebuilds = path.join(dir, 'prebuilds', `${process.platform}-${process.arch}`);
    fs.mkdirSync(prebuilds, { recursive: true });
    fs.writeFileSync(path.join(prebuilds, 'poppler-native.node'), binary);
  }
  const fakeNodeGyp = path.join(dir, 'fake-node-gyp.js');
  fs.writeFileSync(fakeNodeGyp, `require('fs').writeFileSync(${JSON.stringify(path.join(dir, 'compiled'))}, process.env.JOBS);`);
  return { dir, fakeNodeGyp, compiled: () => fs.existsSync(path.join(dir, 'compiled')) };
}

function install(pkg, env) {
  return spawnSync(process.execPath, [path.join(pkg.dir, 'scripts', 'install.js')], {
    encoding: 'utf8',
    env: { ...process.env, npm_config_build_from_source: '', JOBS: '', npm_config_node_gyp: pkg.fakeNodeGyp, ...env },
  });
}

describe('npm install', () => {
  const built = require('node-gyp-build').path(root);
  let pkg;
  afterEach(() => fs.rmSync(pkg.dir, { recursive: true, force: true }));

  test('uses a prebuilt binary that loads, compiling nothing', () => {
    pkg = scratchPackage(fs.readFileSync(built));
    expect(install(pkg).status).toBe(0);
    expect(pkg.compiled()).toBe(false);
  });

  test("compiles when the prebuilt binary doesn't load on this system", () => {
    pkg = scratchPackage(Buffer.from('not a shared library'));
    expect(install(pkg).status).toBe(0);
    expect(pkg.compiled()).toBe(true);
  });

  test('compiles when asked to, also with a prebuilt binary', () => {
    pkg = scratchPackage(fs.readFileSync(built));
    expect(install(pkg, { npm_config_build_from_source: 'true' }).status).toBe(0);
    expect(pkg.compiled()).toBe(true);
  });

  test("says what couldn't be run when the compiler can't be started", () => {
    pkg = scratchPackage();
    const result = install(pkg, { npm_config_node_gyp: 'poppler-native-no-such-command' });
    expect(result.status).toBe(1);
    expect(result.stderr).toContain("couldn't run poppler-native-no-such-command");
  });
});

describe('compileJobs', () => {
  const { compileJobs } = require('../scripts/install.js');
  const GiB = 2 ** 30;

  test('one compiler per core', () => expect(compileJobs(8, 64 * GiB)).toBe(8));
  test('no more than memory allows, about 1 GiB each', () => expect(compileJobs(64, 8 * GiB)).toBe(8));
  test('at least one', () => expect(compileJobs(16, GiB / 2)).toBe(1));
});
