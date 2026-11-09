const { execFileSync, spawnSync } = require('child_process');
const path = require('path');
const pdf = require('../');

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
