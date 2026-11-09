const { execFileSync } = require('child_process');
const path = require('path');

// The addon must be self-contained: Poppler and every C library it needs are linked statically, and nothing but
// Node-API's entry points is exported (so no symbol can clash with another copy of a library in the process).
const addon = process.env.POPPLER_NATIVE_ADDON || require('node-gyp-build').path(path.join(__dirname, '..'));
const readelf = (...args) => {
  try {
    return execFileSync('readelf', [...args, addon], { encoding: 'utf8' });
  } catch (e) {
    if (e.code === 'ENOENT') return null;
    throw e;
  }
};
// glibc: libc.so.6, libm.so.6, ld-linux-x86-64.so.2, libstdc++.so.6, libgcc_s.so.1; musl: libc.musl-x86_64.so.1
const RUNTIMES = /^(libc|libc\.musl-[\w-]+|libm|libstdc\+\+|libgcc_s|ld-linux[\w-]*|ld-musl[\w-]*)\.so/;
const describeOnLinux = process.platform === 'linux' && readelf('-h') !== null ? describe : describe.skip;

describeOnLinux('the compiled addon (Linux)', () => {
  test('needs no shared library but the C and C++ runtimes', () => {
    const needed = [...readelf('-d').matchAll(/\(NEEDED\)\s+Shared library: \[([^\]]+)\]/g)].map(m => m[1]);
    const unexpected = needed.filter(lib => !RUNTIMES.test(lib));
    expect(unexpected).toEqual([]);
  });

  test('exports nothing but the Node-API entry points', () => {
    const exported = readelf('--dyn-syms', '-W')
      .split('\n')
      .map(line => line.trim().split(/\s+/))
      .filter(cols => cols.length >= 8 && cols[6] !== 'UND' && ['GLOBAL', 'WEAK', 'UNIQUE'].includes(cols[4]))
      .map(cols => cols[7].replace(/@.*/, ''));
    expect(exported.sort()).toEqual(['napi_register_module_v1', 'node_api_module_get_api_version_v1']);
  });
});
