#!/usr/bin/env node
// The command line: `npx poppler-native file.pdf` prints what info() returns, as JSON; --text prints the text.
const { parseArgs } = require('node:util');
const { info, text } = require('..');
const { version } = require('../package.json');

const USAGE = `Usage: poppler-native <file.pdf | -> [options]

Prints the PDF's text with positions and fonts, its images, links, outline and metadata as JSON,
or with --text only its text, like pdftotext. "-" reads the PDF from standard input.

Options:
  --text              print the text instead of JSON
  --layout            with --text: keep the physical layout of the page
  --password <pw>     password of an encrypted PDF
  --first-page <n>    first page to read (default: 1)
  --last-page <n>     last page to read (default: the last)
  --no-images         leave images out of the JSON
  --pretty            indent the JSON
  -h, --help          show this help
  -v, --version       show the version
`;

class UsageError extends Error {}

// Output into a pipe that was closed (`poppler-native file.pdf --text | head`) is not an error: stop quietly.
process.stdout.on('error', e => {
  if (e.code !== 'EPIPE') throw e;
  process.exit(0);
});

async function readStdin() {
  const chunks = [];
  for await (const chunk of process.stdin) chunks.push(chunk);
  return Buffer.concat(chunks);
}

function pageNumber(values, name) {
  const value = values[name];
  if (value === undefined) return undefined;
  if (!/^[1-9][0-9]*$/.test(value)) throw new UsageError(`--${name} expects a page number, got "${value}"`);
  return Number(value);
}

async function main() {
  let values;
  let positionals;
  try {
    ({ values, positionals } = parseArgs({
      allowPositionals: true,
      options: {
        text: { type: 'boolean' },
        layout: { type: 'boolean' },
        password: { type: 'string' },
        'first-page': { type: 'string' },
        'last-page': { type: 'string' },
        'no-images': { type: 'boolean' },
        pretty: { type: 'boolean' },
        help: { type: 'boolean', short: 'h' },
        version: { type: 'boolean', short: 'v' },
      },
    }));
  } catch (e) {
    throw new UsageError(e.message);
  }
  if (values.help) return process.stdout.write(USAGE);
  if (values.version) return process.stdout.write(`${version}\n`);
  if (positionals.length !== 1) throw new UsageError('expected exactly one PDF file');
  if (values.layout && !values.text) throw new UsageError('--layout only applies to --text');

  const options = {
    password: values.password,
    firstPage: pageNumber(values, 'first-page'),
    lastPage: pageNumber(values, 'last-page'),
  };
  const input = positionals[0] === '-' ? await readStdin() : positionals[0];
  if (values.text) {
    process.stdout.write(await text(input, { ...options, layout: values.layout }));
  } else {
    const result = await info(input, { ...options, images: !values['no-images'] });
    process.stdout.write(`${JSON.stringify(result, null, values.pretty ? 2 : undefined)}\n`);
  }
}

main().catch(e => {
  if (e instanceof UsageError) {
    process.stderr.write(`poppler-native: ${e.message}\n\n${USAGE}`);
    process.exitCode = 2;
  } else {
    process.stderr.write(`poppler-native: ${e.message}\n`);
    process.exitCode = 1;
  }
});
