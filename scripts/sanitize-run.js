// Exercised by scripts/sanitize.sh under AddressSanitizer: every API path on every test fixture, plus any extra
// folders of PDFs given as arguments. Errors from damaged PDFs are expected; only sanitizer reports matter.
const fs = require('fs');
const path = require('path');
const pdf = require('..');

const fixtures = path.join(__dirname, '..', 'test', 'fixtures');
const pdfsIn = dir =>
  fs.readdirSync(dir, { recursive: true }).filter(f => f.endsWith('.pdf')).map(f => path.join(dir, f));

(async () => {
  const files = [fixtures, ...process.argv.slice(2)].flatMap(pdfsIn);
  let calls = 0;
  for (const file of files) {
    const bytes = fs.readFileSync(file);
    const runs = [
      () => pdf.info(file),
      () => pdf.info(bytes, { images: false, firstPage: 1, lastPage: 1 }),
      () => pdf.info(file, { password: 'secret' }),
      () => pdf.text(bytes),
      () => pdf.text(file, { layout: true, password: 'secret' }),
    ];
    for (const run of runs) {
      await run().catch(() => {});
      calls++;
    }
  }
  const concurrent = files.slice(0, 32);
  await Promise.all(concurrent.map(file => pdf.info(file).catch(() => {})));
  console.log(`sanitize-run: ${calls} calls over ${files.length} PDFs, then ${concurrent.length} at once`);
})();
