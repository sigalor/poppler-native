// Shared by scripts/update-poppler.mjs and scripts/vendor-deps.mjs: downloading and verifying upstream releases,
// working out which of their files a build needs, and rendering their CMake configure_file() templates.
import { execFileSync } from 'node:child_process';
import crypto from 'node:crypto';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

// A failure that ends a script with its message and no stack trace. It is thrown rather than exiting on the spot, so
// that the script's `finally` blocks still remove its temporary directories.
export class Failure extends Error {}

export function fail(tool, message) {
  throw new Failure(`${tool}: ${message}`);
}

// Runs a script's main function: a Failure is printed and ends the script with exit code 1.
export async function runScript(main) {
  try {
    await main();
  } catch (e) {
    if (!(e instanceof Failure)) throw e;
    console.error(e.message);
    process.exitCode = 1;
  }
}

export async function download(url, file) {
  const response = await fetch(url);
  if (!response.ok) throw new Error(`download of ${url} failed: HTTP ${response.status}`);
  fs.writeFileSync(file, Buffer.from(await response.arrayBuffer()));
}

export function sha256(file) {
  return crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
}

// Verifies a detached signature against one committed public key, requiring that key's primary fingerprint.
// Throws with gpg's output on any failure; returns normally only for a good signature by that key.
export function verifySignature(file, signature, keyFile, fingerprint) {
  const gnupgHome = fs.mkdtempSync(path.join(os.tmpdir(), 'poppler-native-gpg-'));
  // The key's trust is established by the fingerprint check below, not by gpg's web of trust.
  const gpg = (...args) =>
    execFileSync('gpg', ['--batch', '--homedir', gnupgHome, '--trust-model', 'always', ...args], {
      encoding: 'utf8',
      stdio: ['ignore', 'pipe', 'pipe'],
    });
  try {
    gpg('--quiet', '--import', keyFile);
    let status;
    try {
      status = gpg('--status-fd', '1', '--verify', signature, file);
    } catch (e) {
      if (e.code === 'ENOENT') throw new Error('gpg is required to verify the release (or pass --skip-signature)');
      throw new Error(`signature verification failed:\n${e.stdout || ''}${e.stderr || e.message}`);
    }
    // VALIDSIG <key fpr> <date> <timestamp> <expiry> <version> <reserved> <pk algo> <hash algo> <class> <primary fpr>
    const validsig = status.split('\n').find(line => line.startsWith('[GNUPG:] VALIDSIG '));
    const primary = validsig && validsig.trim().split(/\s+/).pop();
    if (primary !== fingerprint) {
      throw new Error(`signature is not from the pinned key ${fingerprint} (got ${primary || 'none'})`);
    }
  } catch (e) {
    if (e.code === 'ENOENT') throw new Error('gpg is required to verify the release (or pass --skip-signature)');
    throw e;
  } finally {
    fs.rmSync(gnupgHome, { recursive: true, force: true });
  }
}

export function extract(archive, dest) {
  fs.mkdirSync(dest, { recursive: true });
  execFileSync('tar', ['-xf', archive, '-C', dest]);
}

// Every file the given sources #include, transitively, resolved the way the compiler resolves them: next to the
// including file first, then in each of includePaths (relative to srcRoot). Only files under one of `dirs` are
// followed; anything else (system headers, headers the build generates) is left out.
export function includeClosure(srcRoot, sources, { dirs, includePaths }) {
  const inTree = file => dirs.some(dir => dir === '.' || file.startsWith(`${dir}/`));
  const files = new Set();
  const queue = [...sources];
  while (queue.length > 0) {
    const file = queue.pop();
    if (files.has(file)) continue;
    files.add(file);
    const text = fs.readFileSync(path.join(srcRoot, file), 'latin1');
    for (const [, include] of text.matchAll(/^[ \t]*#[ \t]*include[ \t]*[<"]([^>"]+)[>"]/gm)) {
      const candidates = [path.posix.join(path.posix.dirname(file), include), ...includePaths.map(p => path.posix.join(p, include))];
      const hit = candidates
        .map(candidate => path.posix.normalize(candidate))
        .find(candidate => !candidate.startsWith('..') && inTree(candidate) && fs.statSync(path.join(srcRoot, candidate), { throwIfNoEntry: false })?.isFile());
      if (hit) queue.push(hit);
    }
  }
  return [...files].sort();
}

// Renders a CMake configure_file() template.
//   flags:  NAME -> value for `#cmakedefine01 NAME` (0/1, or a preprocessor condition string: 1 where it holds)
//           and for `#cmakedefine NAME [value]` (true/false, or a condition: defined only where it holds)
//   values: VAR -> replacement for @VAR@ and ${VAR}
// Unknown flags or variables are errors, so a new upstream option always gets a conscious decision.
export function renderCMakeTemplate(text, { flags, values, name }) {
  const used = new Set();
  const substitute = line =>
    line.replace(/\$\{(\w+)\}|@(\w+)@/g, (match, a, b) => {
      const variable = a || b;
      if (!(variable in values)) throw new Error(`${name}: no value for ${match}`);
      used.add(variable);
      return values[variable];
    });
  const conditional = (condition, body) => [`#if ${condition}`, ...body.map(line => `#    ${line.slice(1)}`), '#endif'];
  const out = [];
  for (const line of text.split('\n')) {
    let m;
    if (/^\s*\/\*\s*#cmakedefine/.test(line)) {
      out.push(line); // commented out upstream: not an option of this build
    } else if ((m = line.match(/^#cmakedefine01\s+(\w+)\s*$/))) {
      const flag = m[1];
      if (!(flag in flags)) throw new Error(`${name}: unknown flag ${flag}`);
      used.add(flag);
      const value = flags[flag];
      if (typeof value === 'number') out.push(`#define ${flag} ${value}`);
      else out.push(`#if ${value}`, `#    define ${flag} 1`, '#else', `#    define ${flag} 0`, '#endif');
    } else if ((m = line.match(/^#cmakedefine\s+(\w+)\s*(.*)$/))) {
      const flag = m[1];
      if (!(flag in flags)) throw new Error(`${name}: unknown flag ${flag}`);
      used.add(flag);
      const value = flags[flag];
      const definition = () => `#define ${flag}${m[2] ? ` ${substitute(m[2])}` : ''}`;
      if (value === true) out.push(definition());
      else if (value === false) out.push(`/* #undef ${flag} */`);
      else out.push(...conditional(value, [definition()]));
    } else if (/#cmakedefine/.test(line)) {
      throw new Error(`${name}: cannot render "${line}"`);
    } else {
      out.push(substitute(line));
    }
  }
  return { text: out.join('\n'), used };
}

// A gyp include file listing sources, relative to the directory the .gypi is in.
export function sourcesGypi(comment, sources) {
  const entries = sources.map(source => `      '${source}',`).join('\n');
  return `# ${comment}. Do not edit.\n{\n  'sources': [\n${entries}\n  ],\n}\n`;
}

export function listFiles(dir) {
  if (!fs.existsSync(dir)) return [];
  return fs
    .readdirSync(dir, { recursive: true, withFileTypes: true })
    .filter(entry => entry.isFile())
    .map(entry => path.relative(dir, path.join(entry.parentPath, entry.name)).split(path.sep).join('/'))
    .sort();
}
