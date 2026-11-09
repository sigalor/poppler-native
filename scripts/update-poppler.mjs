#!/usr/bin/env node
// Vendors a pristine Poppler release into native/poppler/.
//
//   node scripts/update-poppler.mjs <version> [--tarball <poppler-x.y.z.tar.xz>] [--skip-signature]
//
// What it does, in order:
//   1. downloads poppler-<version>.tar.xz and its .sig from poppler.freedesktop.org (or uses --tarball, whose
//      signature is expected next to it as <tarball>.sig),
//   2. verifies the signature with gpg against the release key committed in scripts/poppler-release-key.asc,
//      requiring the primary-key fingerprint pinned below,
//   3. reads the core library's source list from upstream's CMakeLists.txt (the `set(poppler_SRCS ...)` block,
//      the optional sources of the features poppler-native enables, and the pregenerated gperf tables),
//   4. replaces native/poppler/{goo,fofi,poppler,splash} with exactly those sources plus every file they
//      #include, transitively -- no file is edited, poppler-native carries no patches to Poppler,
//   5. generates native/poppler/sources.gypi, config.h, poppler-config.h and poppler_private_export.h from
//      upstream's templates and the FLAGS table below, and writes native/poppler/VERSION.
//
// A Poppler release that adds a configuration flag makes this script fail until the flag is added to FLAGS:
// every flag is a conscious decision, never a silent default.
import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

const ROOT = path.resolve(import.meta.dirname, '..');
const DEST = path.join(ROOT, 'native', 'poppler');
const VENDORED_DIRS = ['goo', 'fofi', 'poppler', 'splash'];
const RELEASE_KEY_FILE = path.join(ROOT, 'scripts', 'poppler-release-key.asc');
// Albert Astals Cid, Poppler's release manager, as published on https://poppler.freedesktop.org/
const RELEASE_KEY_FINGERPRINT = 'CA262C6C83DE4D2FB28A332A3A6A4DB839EAA6D7';

// Sources CMake adds only for enabled features. poppler-native decodes DCT (JPEG) through libjpeg and JPX
// (JPEG 2000) through OpenJPEG: since Poppler 26.07 there is no built-in decoder for either.
const OPTIONAL_SOURCES = ['poppler/DCTStream.cc', 'poppler/JPEG2000Stream.cc'];

// The value of every flag in config.h.cmake and poppler/poppler-config.h.cmake. A number is emitted as is; a
// string is a preprocessor condition, and the flag is 1 where it holds and 0 elsewhere -- this replaces the
// platform checks CMake would run, so one set of headers serves Linux (glibc and musl), macOS and Windows.
const FLAGS = {
  // features
  ENABLE_LIBJPEG: 1,
  ENABLE_LIBPNG: 1,
  ENABLE_LIBOPENJPEG: 1,
  ENABLE_LIBTIFF: 0,
  ENABLE_LIBCURL: 0,
  POPPLER_HAS_CURL_SUPPORT: 0,
  ENABLE_BROTLI: 0,
  ENABLE_HARFBUZZ: 0,
  ENABLE_NSS3: 0,
  ENABLE_GPGME: 0,
  ENABLE_PGP_SIGNATURES: 0,
  ENABLE_SIGNATURES: 0,
  ENABLE_ZLIB_UNCOMPRESS: 0,
  ENABLE_RELOCATABLE: 0,
  USE_CMS: 0,
  USE_BOOST_HEADERS: 0,
  // font lookup on the system is never needed: poppler-native extracts text, it does not render
  WITH_FONTCONFIGURATION_FONTCONFIG: 0,
  WITH_FONTCONFIGURATION_WIN32: 'defined(_WIN32)',
  WITH_FONTCONFIGURATION_ANDROID: 0,
  // platform functions
  HAVE_FSEEKO: '!defined(_WIN32)',
  HAVE_FSEEK64: 0,
  HAVE_PREAD64: 'defined(__GLIBC__)',
  HAVE_LSEEK64: 'defined(__GLIBC__)',
  HAVE_GMTIME_R: '!defined(_WIN32)',
  HAVE_TIMEGM: '!defined(_WIN32)',
  HAVE_LOCALTIME_R: '!defined(_WIN32)',
  HAVE_STRTOK_R: '!defined(_WIN32)',
  HAVE_TCGETATTR: '!defined(_WIN32)',
  WORDS_BIGENDIAN: 'defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__',
};

// ${...} substitutions in the templates (POPPLER_VERSION is filled in from the release).
const VALUES = {
  DEFAULT_SIGNATURE_BACKEND: 'None',
  ICONV_CONST: '',
  POPPLER_DATADIR: '/usr/share/poppler',
};

function fail(message) {
  console.error(`update-poppler: ${message}`);
  process.exit(1);
}

function parseArgs(argv) {
  const args = { version: null, tarball: null, skipSignature: false };
  for (let i = 0; i < argv.length; i++) {
    if (argv[i] === '--tarball') args.tarball = argv[++i];
    else if (argv[i] === '--skip-signature') args.skipSignature = true;
    else if (!args.version) args.version = argv[i];
    else fail(`unexpected argument ${argv[i]}`);
  }
  if (!args.version || !/^\d+\.\d+\.\d+$/.test(args.version)) {
    fail('usage: node scripts/update-poppler.mjs <version, e.g. 26.11.0> [--tarball <file>] [--skip-signature]');
  }
  return args;
}

async function download(url, file) {
  const response = await fetch(url);
  if (!response.ok) fail(`download of ${url} failed: HTTP ${response.status}`);
  fs.writeFileSync(file, Buffer.from(await response.arrayBuffer()));
}

function verifySignature(tarball, signature) {
  const gnupgHome = fs.mkdtempSync(path.join(os.tmpdir(), 'poppler-native-gpg-'));
  try {
    // The key's trust is established by the fingerprint check below, not by gpg's web of trust.
    const gpg = (...args) =>
      execFileSync('gpg', ['--batch', '--homedir', gnupgHome, '--trust-model', 'always', ...args], {
        encoding: 'utf8',
        stdio: ['ignore', 'pipe', 'pipe'],
      });
    gpg('--quiet', '--import', RELEASE_KEY_FILE);
    let status;
    try {
      status = gpg('--status-fd', '1', '--verify', signature, tarball);
    } catch (e) {
      fail(`signature verification failed:\n${e.stdout || ''}${e.stderr || e.message}`);
    }
    // VALIDSIG <key fpr> <date> <timestamp> <expiry> <version> <reserved> <pk algo> <hash algo> <class> <primary fpr>
    const validsig = status.split('\n').find(line => line.startsWith('[GNUPG:] VALIDSIG '));
    const primary = validsig && validsig.trim().split(/\s+/).pop();
    if (primary !== RELEASE_KEY_FINGERPRINT) {
      fail(`signature is not from the pinned release key ${RELEASE_KEY_FINGERPRINT} (got ${primary || 'none'})`);
    }
  } catch (e) {
    if (e.code === 'ENOENT') fail('gpg is required to verify the release (or pass --skip-signature)');
    throw e;
  } finally {
    fs.rmSync(gnupgHome, { recursive: true, force: true });
  }
}

// The core library's sources, as upstream's CMakeLists.txt lists them for poppler-native's configuration.
function coreSources(srcRoot) {
  const cmake = fs.readFileSync(path.join(srcRoot, 'CMakeLists.txt'), 'utf8');
  const block = cmake.match(/^set\(poppler_SRCS\n([\s\S]*?)^\)/m);
  if (!block) fail('could not find the set(poppler_SRCS ...) block in CMakeLists.txt');
  const listed = block[1]
    .split('\n')
    .map(line => line.trim())
    .filter(Boolean);
  for (const entry of listed) {
    if (!VENDORED_DIRS.some(dir => entry.startsWith(`${dir}/`))) {
      fail(`poppler_SRCS lists ${entry}, outside ${VENDORED_DIRS.join('/')}: decide how to vendor it`);
    }
  }
  const gperf = [...cmake.matchAll(/^ADD_GPERF_FILE\((\w+)\)/gm)].map(m => `poppler/${m[1]}.pregenerated.c`);
  if (gperf.length === 0) fail('found no ADD_GPERF_FILE(...) calls in CMakeLists.txt');
  const sources = [...listed, ...OPTIONAL_SOURCES, ...gperf];
  for (const source of sources) {
    if (!fs.existsSync(path.join(srcRoot, source))) fail(`${source} does not exist in the release`);
  }
  return sources;
}

// Every file the sources #include, transitively, resolved the way the compiler resolves them: next to the
// including file first, then from the source root and from poppler/ (Poppler's include directories).
function includeClosure(srcRoot, sources) {
  const files = new Set();
  const queue = [...sources];
  while (queue.length > 0) {
    const file = queue.pop();
    if (files.has(file)) continue;
    files.add(file);
    const text = fs.readFileSync(path.join(srcRoot, file), 'latin1');
    for (const [, include] of text.matchAll(/^[ \t]*#[ \t]*include[ \t]*[<"]([^>"]+)[>"]/gm)) {
      const candidates = [path.posix.join(path.posix.dirname(file), include), include, `poppler/${include}`];
      const hit = candidates
        .map(candidate => path.posix.normalize(candidate))
        .find(
          candidate =>
            VENDORED_DIRS.some(dir => candidate.startsWith(`${dir}/`)) &&
            fs.statSync(path.join(srcRoot, candidate), { throwIfNoEntry: false })?.isFile(),
        );
      if (hit) queue.push(hit);
    }
  }
  return [...files].sort();
}

const BANNER = (version, template) =>
  `/* Generated by scripts/update-poppler.mjs from Poppler ${version}'s ${template}. Do not edit:\n` +
  ` * change FLAGS or VALUES in the script and re-run it. */\n`;

// Renders a CMake configure_file() template with FLAGS and VALUES.
function renderTemplate(srcRoot, template, version, usedFlags) {
  const values = { ...VALUES, POPPLER_VERSION: version };
  const substitute = text =>
    text.replace(/\$\{(\w+)\}|@(\w+)@/g, (match, a, b) => {
      const name = a || b;
      if (!(name in values)) fail(`${template}: no value for ${match}; add it to VALUES`);
      return values[name];
    });
  const lines = fs.readFileSync(path.join(srcRoot, template), 'utf8').split('\n');
  const out = [];
  for (const line of lines) {
    let m;
    if ((m = line.match(/^#cmakedefine01\s+(\w+)\s*$/))) {
      const name = m[1];
      if (!(name in FLAGS)) fail(`${template}: unknown flag ${name}; decide its value and add it to FLAGS`);
      usedFlags.add(name);
      const value = FLAGS[name];
      if (typeof value === 'number') {
        out.push(`#define ${name} ${value}`);
      } else {
        out.push(`#if ${value}`, `#    define ${name} 1`, '#else', `#    define ${name} 0`, '#endif');
      }
    } else if ((m = line.match(/^#cmakedefine\s+(\w+)\s+(.*)$/))) {
      const name = m[1];
      const expansion = m[2].match(/^@(\w+)@$/);
      if (expansion && !(expansion[1] in values)) {
        out.push(`/* #undef ${name} */`); // CMake's output for an unset variable
      } else {
        out.push(`#define ${name} ${substitute(m[2])}`);
      }
    } else if (/#cmakedefine/.test(line)) {
      fail(`${template}: cannot render "${line}"`);
    } else {
      out.push(substitute(line));
    }
  }
  return BANNER(version, template) + out.join('\n');
}

// CMake's generate_export_header() for a static library: poppler-native links Poppler statically and exports
// nothing but its Node-API entry point, so Poppler's symbols get no visibility attribute at all.
function exportHeader(version) {
  return `${BANNER(version, 'generate_export_header(poppler BASE_NAME poppler-private) for a static build')}
#ifndef POPPLER_PRIVATE_EXPORT_H
#define POPPLER_PRIVATE_EXPORT_H

#define POPPLER_PRIVATE_EXPORT
#define POPPLER_PRIVATE_NO_EXPORT

#ifndef POPPLER_PRIVATE_DEPRECATED
#    if defined(_MSC_VER)
#        define POPPLER_PRIVATE_DEPRECATED __declspec(deprecated)
#    else
#        define POPPLER_PRIVATE_DEPRECATED __attribute__((__deprecated__))
#    endif
#endif

#define POPPLER_PRIVATE_DEPRECATED_EXPORT POPPLER_PRIVATE_EXPORT POPPLER_PRIVATE_DEPRECATED
#define POPPLER_PRIVATE_DEPRECATED_NO_EXPORT POPPLER_PRIVATE_NO_EXPORT POPPLER_PRIVATE_DEPRECATED

#endif /* POPPLER_PRIVATE_EXPORT_H */
`;
}

function sourcesGypi(version, sources) {
  const entries = sources
    .filter(source => /\.(cc|c)$/.test(source))
    .map(source => `      '${source}',`)
    .join('\n');
  return (
    `# Generated by scripts/update-poppler.mjs from Poppler ${version}'s CMakeLists.txt. Do not edit.\n` +
    `{\n  'sources': [\n${entries}\n  ],\n}\n`
  );
}

function listVendored() {
  const files = [];
  for (const dir of VENDORED_DIRS) {
    const base = path.join(DEST, dir);
    if (!fs.existsSync(base)) continue;
    for (const entry of fs.readdirSync(base, { recursive: true, withFileTypes: true })) {
      if (entry.isFile()) {
        files.push(path.relative(DEST, path.join(entry.parentPath, entry.name)).split(path.sep).join('/'));
      }
    }
  }
  return files.sort();
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  const work = fs.mkdtempSync(path.join(os.tmpdir(), 'poppler-native-update-'));
  try {
    let tarball = args.tarball && path.resolve(args.tarball);
    if (!tarball) {
      tarball = path.join(work, `poppler-${args.version}.tar.xz`);
      const url = `https://poppler.freedesktop.org/poppler-${args.version}.tar.xz`;
      console.log(`downloading ${url}`);
      await download(url, tarball);
      if (!args.skipSignature) await download(`${url}.sig`, `${tarball}.sig`);
    }
    if (args.skipSignature) {
      console.warn('update-poppler: WARNING: signature check skipped');
    } else {
      verifySignature(tarball, `${tarball}.sig`);
      console.log(`signature OK (release key ${RELEASE_KEY_FINGERPRINT})`);
    }

    const extracted = path.join(work, 'src');
    fs.mkdirSync(extracted);
    execFileSync('tar', ['-xJf', tarball, '-C', extracted]);
    const srcRoot = path.join(extracted, `poppler-${args.version}`);
    if (!fs.existsSync(path.join(srcRoot, 'CMakeLists.txt'))) fail(`${tarball} is not poppler-${args.version}`);

    const sources = coreSources(srcRoot);
    const files = includeClosure(srcRoot, sources);

    const before = listVendored();
    for (const dir of VENDORED_DIRS) fs.rmSync(path.join(DEST, dir), { recursive: true, force: true });
    for (const file of files) {
      fs.mkdirSync(path.join(DEST, path.dirname(file)), { recursive: true });
      fs.copyFileSync(path.join(srcRoot, file), path.join(DEST, file));
    }
    for (const license of ['COPYING', 'COPYING3', 'README-XPDF']) {
      fs.copyFileSync(path.join(srcRoot, license), path.join(DEST, license));
    }

    const usedFlags = new Set();
    fs.writeFileSync(path.join(DEST, 'config.h'), renderTemplate(srcRoot, 'config.h.cmake', args.version, usedFlags));
    fs.writeFileSync(
      path.join(DEST, 'poppler-config.h'),
      renderTemplate(srcRoot, 'poppler/poppler-config.h.cmake', args.version, usedFlags),
    );
    fs.writeFileSync(path.join(DEST, 'poppler_private_export.h'), exportHeader(args.version));
    fs.writeFileSync(path.join(DEST, 'sources.gypi'), sourcesGypi(args.version, sources));
    fs.writeFileSync(path.join(DEST, 'VERSION'), `${args.version}\n`);

    const unused = Object.keys(FLAGS).filter(flag => !usedFlags.has(flag));
    const after = listVendored();
    const added = after.filter(file => !before.includes(file));
    const removed = before.filter(file => !after.includes(file));
    console.log(`vendored Poppler ${args.version}: ${sources.length} sources, ${files.length} files in total`);
    if (added.length) console.log(`added (${added.length}): ${added.join(' ')}`);
    if (removed.length) console.log(`removed (${removed.length}): ${removed.join(' ')}`);
    if (unused.length) console.warn(`update-poppler: flags no longer in the templates, remove them: ${unused.join(' ')}`);
  } finally {
    fs.rmSync(work, { recursive: true, force: true });
  }
}

await main();
