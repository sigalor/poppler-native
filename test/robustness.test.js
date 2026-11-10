const { execFileSync, spawnSync } = require('child_process');
const fs = require('fs');
const os = require('os');
const path = require('path');
const pdf = require('../');
const { deepOutlinePdf, sharedOutlinePdf } = require('./helpers/pdf');

const root = path.join(__dirname, '..');
const fixture = name => path.join(__dirname, 'fixtures', name);

// Runs the body in a fresh Node process and returns its stderr: what a user would see in their own logs.
function stderrOf(body) {
  const script = `const pdf = require(${JSON.stringify(root)}); (async () => { ${body} })()`;
  return spawnSync(process.execPath, ['-e', script], { encoding: 'utf8' }).stderr;
}

describe('quiet', () => {
  test('a damaged PDF rejects the promise without printing anything', () => {
    expect(stderrOf(`await pdf.info(${JSON.stringify(fixture('not-a-pdf.pdf'))}).catch(() => {});`)).toBe('');
  });

  test('a wrong password rejects the promise without printing anything', () => {
    const file = JSON.stringify(fixture('encrypted-user.pdf'));
    expect(stderrOf(`await pdf.info(${file}, { password: 'wrong' }).catch(() => {}); await pdf.text(${file}).catch(() => {});`)).toBe('');
  });
});

describe('outline', () => {
  // Every level used to cost a walk up to the top (quadratic time) and a level of recursion, here and when the result
  // is built on the JavaScript thread, where 20,000 levels exhausted the stack and V8 aborted the process.
  test('a deeply nested outline is cut at 256 levels', () => {
    const file = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'poppler-native-outline-')), 'deep.pdf');
    try {
      fs.writeFileSync(file, deepOutlinePdf(20000));
      const script = `require(${JSON.stringify(root)}).info(${JSON.stringify(file)}).then(d => {
        let depth = 0;
        for (let items = d.outline; items && items.length; items = items[0].children) depth++;
        process.stdout.write(String(depth));
      })`;
      const result = spawnSync(process.execPath, ['-e', script], { encoding: 'utf8', timeout: 60000 });
      expect(result.status).toBe(0);
      expect(result.stdout).toBe('256');
    } finally {
      fs.rmSync(path.dirname(file), { recursive: true, force: true });
    }
  }, 70000);

  // Items whose children are one shared list were read once for every parent: the work doubled with every level.
  test('items that share their children are read once', () => {
    const file = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'poppler-native-outline-')), 'shared.pdf');
    try {
      fs.writeFileSync(file, sharedOutlinePdf(40));
      const script = `require(${JSON.stringify(root)}).info(${JSON.stringify(file)}).then(d => {
        const count = items => (items || []).reduce((n, item) => n + 1 + count(item.children), 0);
        process.stdout.write(String(count(d.outline)));
      })`;
      const result = spawnSync(process.execPath, ['-e', script], { encoding: 'utf8', timeout: 10000 });
      expect(result.status).toBe(0);
      expect(result.stdout).toBe('80');
    } finally {
      fs.rmSync(path.dirname(file), { recursive: true, force: true });
    }
  }, 20000);

  test('an item whose destination cannot be resolved has no page', async () => {
    expect((await pdf.info(fixture('outline-unresolved.pdf'))).outline).toEqual([
      { title: 'Missing' },
      { title: 'Website' },
      { title: 'Page 1', page: 1 },
    ]);
  });
});

describe('concurrency', () => {
  test('48 calls at once on 16 threads give the same results as one at a time', () => {
    const script = `
      const pdf = require(${JSON.stringify(root)});
      const fs = require('fs');
      const files = ${JSON.stringify(
        ['minimal-text.pdf', 'three-pages.pdf', 'images.pdf', 'arial-gid-codes.pdf', 'arial-encoded.pdf', 'outline-unresolved.pdf'].map(fixture),
      )};
      const jobs = [];
      for (let i = 0; i < 48; i++) {
        const file = files[i % files.length];
        if (i % 3 === 0) jobs.push(() => pdf.text(file));
        else if (i % 3 === 1) jobs.push(() => pdf.info(file));
        else jobs.push(() => pdf.info(fs.readFileSync(file), { images: false }));
      }
      (async () => {
        const sequential = [];
        for (const job of jobs) sequential.push(JSON.stringify(await job()));
        const concurrent = (await Promise.all(jobs.map(job => job()))).map(r => JSON.stringify(r));
        const different = concurrent.map((r, i) => (r === sequential[i] ? null : i)).filter(i => i !== null);
        process.stdout.write(JSON.stringify(different));
      })();`;
    const out = execFileSync(process.execPath, ['-e', script], {
      env: { ...process.env, UV_THREADPOOL_SIZE: '16' },
      encoding: 'utf8',
    });
    expect(JSON.parse(out)).toEqual([]);
  }, 120000);
});

describe('worker threads', () => {
  // A worker terminated while a parse is running: when the parse finishes, the worker's environment is gone and the
  // result can't be delivered, which must be dropped quietly instead of aborting the whole process.
  test('terminating a worker in the middle of a parse leaves the process running', () => {
    const script = `
      const { Worker } = require('worker_threads');
      const code = \`
        const pdf = require(\${JSON.stringify(${JSON.stringify(root)})});
        (async () => { for (;;) await pdf.info(\${JSON.stringify(${JSON.stringify(fixture('images.pdf'))})}); })();\`;
      (async () => {
        for (let i = 0; i < 20; i++) {
          const worker = new Worker(code, { eval: true });
          await new Promise(resolve => setTimeout(resolve, 20 + ((i * 7) % 40)));
          await worker.terminate();
        }
        const d = await require(${JSON.stringify(root)}).info(${JSON.stringify(fixture('images.pdf'))});
        process.stdout.write('ok ' + d.pages[0].images.length);
      })();`;
    const result = spawnSync(process.execPath, ['-e', script], { encoding: 'utf8', timeout: 60000 });
    expect(result.stderr).toBe('');
    expect(result.status).toBe(0);
    expect(result.stdout).toBe('ok 5');
  }, 70000);
});

describe('damaged streams', () => {
  // A stream with an unknown filter makes Poppler dynamic_cast it, which crashed the process on Windows while the
  // build had RTTI turned off. In a child process, so that a crash fails this test instead of taking Jest down.
  test('an image with an unknown filter still parses', () => {
    const script = `
      const bytes = require('fs').readFileSync(${JSON.stringify(fixture('images.pdf'))}, 'latin1');
      const damaged = Buffer.from(bytes.replace('/DCTDecode', '/DCTDe\\x08ode'), 'latin1');
      require(${JSON.stringify(root)}).info(damaged).then(d => process.stdout.write(String(d.pages[0].images.length)));`;
    const result = spawnSync(process.execPath, ['-e', script], { encoding: 'utf8' });
    expect(result.status).toBe(0);
    expect(result.stdout).toBe('5');
  });
});
