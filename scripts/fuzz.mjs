#!/usr/bin/env node
// Feeds poppler-native mutated copies of the test fixtures, to find inputs that crash it rather than reject.
//
//   node scripts/fuzz.mjs [--cases <per PDF, default 100>] [--seed <n, default 1>] [--dir <folder of PDFs>]
//
// Without --dir it mutates the test fixtures; --dir fuzzes your own PDFs (any folder, searched recursively).
// Every case is parsed in a child process; a child that dies by a signal is a crash, reported with the seed and case
// needed to reproduce it (node scripts/fuzz.mjs --seed S --case C writes that case to fuzz-case.pdf). Prints a JSON
// summary on the last line of stdout and exits 1 if anything crashed.
import { spawnSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { parseArgs } from 'node:util';

const ROOT = path.resolve(import.meta.dirname, '..');
const FIXTURES = path.join(ROOT, 'test', 'fixtures');

// A small, fast, seedable PRNG (mulberry32), so a failing case can be reproduced from its seed.
function random(seed) {
  let a = seed >>> 0;
  return () => {
    a = (a + 0x6d2b79f5) >>> 0;
    let t = a;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

// One mutated copy of `bytes`: byte flips, a truncation, a deleted or a duplicated chunk.
function mutate(bytes, rand) {
  const int = n => Math.floor(rand() * n);
  let out = Buffer.from(bytes);
  switch (int(4)) {
    case 0:
      for (let i = 0, n = 1 + int(16); i < n; i++) out[int(out.length)] = int(256);
      break;
    case 1:
      out = out.subarray(0, int(out.length));
      break;
    case 2: {
      const start = int(out.length);
      out = Buffer.concat([out.subarray(0, start), out.subarray(Math.min(out.length, start + 1 + int(256)))]);
      break;
    }
    default: {
      const start = int(out.length);
      const chunk = out.subarray(start, start + 1 + int(256));
      const at = int(out.length);
      out = Buffer.concat([out.subarray(0, at), chunk, out.subarray(at)]);
    }
  }
  return out;
}

function cases(seed, perFixture, dir) {
  const rand = random(seed);
  const files = fs
    .readdirSync(dir, { recursive: true })
    .filter(f => f.endsWith('.pdf') && path.basename(f) !== 'not-a-pdf.pdf')
    .sort();
  const list = [];
  for (const file of files) {
    const bytes = fs.readFileSync(path.join(dir, file));
    for (let i = 0; i < perFixture; i++) list.push({ id: list.length, file, bytes: mutate(bytes, rand) });
  }
  return list;
}

const { values } = parseArgs({
  options: {
    cases: { type: 'string', default: '100' },
    seed: { type: 'string', default: '1' },
    case: { type: 'string' },
    dir: { type: 'string', default: FIXTURES },
  },
});
const seed = Number(values.seed);
const all = cases(seed, Number(values.cases), path.resolve(values.dir));

if (values.case !== undefined) {
  const c = all[Number(values.case)];
  fs.writeFileSync('fuzz-case.pdf', c.bytes);
  console.log(`wrote fuzz-case.pdf: case ${c.id} of seed ${seed} (a mutation of ${c.file})`);
  process.exit(0);
}

const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'poppler-native-fuzz-'));
const worker = `
  const pdf = require(${JSON.stringify(ROOT)});
  (async () => {
    const input = require('fs').readFileSync(process.argv[1]);
    await pdf.info(input).catch(() => {});
    await pdf.text(input).catch(() => {});
  })();`;
const crashes = [];
try {
  for (const c of all) {
    const file = path.join(dir, `${c.id}.pdf`);
    fs.writeFileSync(file, c.bytes);
    const result = spawnSync(process.execPath, ['-e', worker, file], { timeout: 60000, encoding: 'utf8' });
    if (result.signal || result.status !== 0) {
      crashes.push({ case: c.id, file: c.file, signal: result.signal, status: result.status, stderr: result.stderr.slice(0, 1500) });
    }
    fs.rmSync(file);
  }
} finally {
  fs.rmSync(dir, { recursive: true, force: true });
}
console.log(JSON.stringify({ seed, cases: all.length, crashes }));
process.exitCode = crashes.length ? 1 : 0;
