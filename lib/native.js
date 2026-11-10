// Loads the compiled addon: the prebuilt binary for this platform when the package ships one, otherwise the one
// `npm install` compiled into build/. Everything in the package goes through this module.
const path = require('path');

const howToCompile =
  'Compile one for it with `npm rebuild poppler-native --build-from-source`, which needs Python 3 and a C++23 ' +
  'compiler, see https://github.com/sigalor/poppler-native#building-from-source';

try {
  module.exports = require('node-gyp-build')(path.join(__dirname, '..'));
} catch (e) {
  // A binary that doesn't load here: built for another C library (a musl system other than Alpine gets the glibc
  // one), or for a newer system than this one.
  if (e.code === 'ERR_DLOPEN_FAILED') {
    throw new Error(
      `poppler-native's binary for ${process.platform}-${process.arch} doesn't load on this system ` +
        `(${String(e.message).split('\n')[0]}). ${howToCompile}`,
      { cause: e },
    );
  }
  if (!String(e.message).includes('No native build was found')) throw e;
  // No prebuilt binary for this platform and nothing compiled: the install script, which compiles one, failed or
  // didn't run (pnpm 10+ and Bun run a dependency's install script only when it is allowed to).
  throw new Error(
    `poppler-native has no prebuilt binary for ${process.platform}-${process.arch}, and none was compiled when it ` +
      'was installed. Compile it with `npm rebuild poppler-native` (pnpm: `pnpm approve-builds`, Bun: ' +
      '`bun pm trust poppler-native`), which needs Python 3 and a C++23 compiler, see ' +
      'https://github.com/sigalor/poppler-native#building-from-source\n\n' +
      e.message,
  );
}
