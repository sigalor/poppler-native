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
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

import {
  download,
  extract,
  fail as failWith,
  includeClosure,
  listFiles,
  renderCMakeTemplate,
  sourcesGypi,
  verifySignature,
} from './lib/vendoring.mjs';

const ROOT = path.resolve(import.meta.dirname, '..');
const DEST = path.join(ROOT, 'native', 'poppler');
const VENDORED_DIRS = ['goo', 'fofi', 'poppler', 'splash'];
const RELEASE_KEY_FILE = path.join(ROOT, 'scripts', 'keys', 'poppler.asc');
// Albert Astals Cid, Poppler's release manager, as published on https://poppler.freedesktop.org/
const RELEASE_KEY_FINGERPRINT = 'CA262C6C83DE4D2FB28A332A3A6A4DB839EAA6D7';

// Sources CMake adds only for enabled features. poppler-native decodes DCT (JPEG) through libjpeg and JPX
// (JPEG 2000) through OpenJPEG: since Poppler 26.07 there is no built-in decoder for either.
const OPTIONAL_SOURCES = ['poppler/DCTStream.cc', 'poppler/JPEG2000Stream.cc'];

// The value of every flag in config.h.cmake and poppler/poppler-config.h.cmake. A number is emitted as is; a
// string is a preprocessor condition, and the flag is 1 where it holds and 0 elsewhere -- this replaces the
// platform checks CMake would run, so one set of headers serves Linux (glibc and musl), macOS and Windows.
// (true/false are for the plain `#cmakedefine NAME value` lines: defined or not.)
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
  DEFAULT_SIGNATURE_BACKEND: true,
  _FILE_OFFSET_BITS: false, // 64-bit targets only
};

// ${...} substitutions in the templates (POPPLER_VERSION is filled in from the release).
const VALUES = {
  DEFAULT_SIGNATURE_BACKEND: 'None',
  ICONV_CONST: '',
  POPPLER_DATADIR: '/usr/share/poppler',
};

const fail = message => failWith('update-poppler', message);

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

// Renders one of upstream's CMake templates with FLAGS and VALUES.
function renderTemplate(srcRoot, template, version, usedFlags) {
  try {
    const { text, used } = renderCMakeTemplate(fs.readFileSync(path.join(srcRoot, template), 'utf8'), {
      flags: FLAGS,
      values: { ...VALUES, POPPLER_VERSION: version },
      name: template,
    });
    for (const flag of used) usedFlags.add(flag);
    return BANNER(version, template) + text;
  } catch (e) {
    fail(`${e.message}; decide it in FLAGS or VALUES`);
  }
}

const BANNER = (version, template) =>
  `/* Generated by scripts/update-poppler.mjs from Poppler ${version}'s ${template}. Do not edit:\n` +
  ` * change FLAGS or VALUES in the script and re-run it. */\n`;

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

async function main() {
  const args = parseArgs(process.argv.slice(2));
  const work = fs.mkdtempSync(path.join(os.tmpdir(), 'poppler-native-update-'));
  try {
    let tarball = args.tarball && path.resolve(args.tarball);
    if (!tarball) {
      tarball = path.join(work, `poppler-${args.version}.tar.xz`);
      const url = `https://poppler.freedesktop.org/poppler-${args.version}.tar.xz`;
      console.log(`downloading ${url}`);
      try {
        await download(url, tarball);
        if (!args.skipSignature) await download(`${url}.sig`, `${tarball}.sig`);
      } catch (e) {
        fail(e.message);
      }
    }
    if (args.skipSignature) {
      console.warn('update-poppler: WARNING: signature check skipped');
    } else {
      try {
        verifySignature(tarball, `${tarball}.sig`, RELEASE_KEY_FILE, RELEASE_KEY_FINGERPRINT);
      } catch (e) {
        fail(e.message);
      }
      console.log(`signature OK (release key ${RELEASE_KEY_FINGERPRINT})`);
    }

    const extracted = path.join(work, 'src');
    extract(tarball, extracted);
    const srcRoot = path.join(extracted, `poppler-${args.version}`);
    if (!fs.existsSync(path.join(srcRoot, 'CMakeLists.txt'))) fail(`${tarball} is not poppler-${args.version}`);

    const sources = coreSources(srcRoot);
    const files = includeClosure(srcRoot, sources, { dirs: VENDORED_DIRS, includePaths: ['.', 'poppler'] });

    const listVendored = () => VENDORED_DIRS.flatMap(dir => listFiles(path.join(DEST, dir)).map(f => `${dir}/${f}`)).sort();
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
    fs.writeFileSync(
      path.join(DEST, 'sources.gypi'),
      sourcesGypi(
        `Generated by scripts/update-poppler.mjs from Poppler ${args.version}'s CMakeLists.txt`,
        sources.filter(source => /\.(cc|c)$/.test(source)),
      ),
    );
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
