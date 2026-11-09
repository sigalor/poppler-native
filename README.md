# poppler-native

[![npm](https://img.shields.io/npm/v/poppler-native)](https://www.npmjs.com/package/poppler-native)
[![CI](https://github.com/sigalor/poppler-native/actions/workflows/ci.yml/badge.svg)](https://github.com/sigalor/poppler-native/actions/workflows/ci.yml)
[![license](https://img.shields.io/github/license/sigalor/poppler-native)](LICENSE)

**Read PDFs in Node.js with [Poppler](https://poppler.freedesktop.org/)** — the engine behind `pdftotext`, Evince and
Okular — compiled into a native addon. One call gives you the text with its exact position, font and color, the
images, links, outline and metadata. Or just the text.

```js
import { info, text } from 'poppler-native';

console.log(await text('report.pdf')); // the text, like pdftotext

const { pages, fonts } = await info('report.pdf');
for (const s of pages[0].strings) {
  console.log(s.left, s.top, fonts[s.font].family, s.text); // 108 187.152 Helvetica Page 1
}
```

- **Installs in seconds, everywhere.** Prebuilt for Linux (x64 and arm64, glibc and musl/Alpine), macOS (Apple
  silicon and Intel) and Windows (x64 and arm64). No system libraries, no compiler, nothing to `apt install`.
- **Poppler 26.11**, unmodified, with zlib, libpng, libjpeg-turbo, OpenJPEG and FreeType built in. Fast: the 756 pages
  of the PDF 1.7 specification in two seconds, and as many PDFs at once as you have cores.
- **Everything in one pass**: positioned text runs with font, size and color; images as `data:` URLs; links; the
  outline; metadata. Encrypted PDFs, page ranges.
- **Typed** (TypeScript declarations included), **ES modules and CommonJS**, a **command line**, and errors that say
  what went wrong. Never writes to your console, safe to call concurrently.

## Install

```sh
npm install poppler-native
```

Node.js 22 or newer. Prebuilt binaries are included for:

| | x64 | arm64 |
| --- | :-: | :-: |
| Linux with glibc 2.28 or newer (Debian 10+, Ubuntu 20.04+, RHEL/Rocky/Alma 8+, Amazon Linux 2023) | ✓ | ✓ |
| Linux with musl (Alpine) | ✓ | ✓ |
| macOS 11 or newer | ✓ | ✓ |
| Windows 10 or newer | ✓ | ✓ |

Anywhere else, the install compiles poppler-native from source automatically, which needs Python 3 and a C++23
compiler (see [Building from source](#building-from-source)).

## Usage

### `text(pdf, options?)` → `Promise<string>`

The text of the PDF in reading order, every page followed by a form feed (`\f`): what Poppler's `pdftotext` prints,
character for character, except where the [text repair](#how-it-works) reads characters that `pdftotext` can't.

```js
const { text } = require('poppler-native');

const all = await text('report.pdf');
const pages = all.split('\f').slice(0, -1); // one string per page
const table = await text('report.pdf', { firstPage: 3, lastPage: 3, layout: true }); // keep columns aligned
```

| Option | | |
| --- | --- | --- |
| `password` | string | The user or owner password of an encrypted PDF. |
| `firstPage`, `lastPage` | number | The pages to read, counting from 1. A `lastPage` beyond the end reads to the end. |
| `layout` | boolean | Keep the physical layout of the page, like `pdftotext -layout`. Default `false`. |

### `info(pdf, options?)` → `Promise<PdfInfo>`

Everything on the pages, with positions. `pdf` is a path or the PDF's bytes (a `Buffer`, any typed array or an
`ArrayBuffer`); the same goes for `text()`.

```js
const doc = await info(await fs.readFile('statement.pdf'), { images: false });
```

| Option | | |
| --- | --- | --- |
| `password` | string | The user or owner password of an encrypted PDF. |
| `firstPage`, `lastPage` | number | The pages to read, counting from 1. |
| `images` | boolean | Extract images. Default `true`; `false` is faster and makes the result much smaller. |
| `reconvertThroughPS` | boolean | Run the PDF through Ghostscript (PDF → PostScript → PDF) first, which can help with PDFs whose fonts map characters oddly. Needs Ghostscript on the `PATH`. |

The result for [`test/fixtures/three-pages.pdf`](test/fixtures/three-pages.pdf):

```js
{
  meta: {
    Author: 'poppler-native',
    CreationDate: 'D:20260101120000Z',
    Creator: 'make-fixtures.py',
    Subject: 'Test fixture',
    Title: 'Über ✓ Test',
  },
  outline: [
    { page: 1, title: 'Chapter 1 – Überblick', children: [{ page: 2, title: 'Section 1.1' }] },
    { page: 3, title: 'Chapter 2' },
  ],
  fonts: [{ id: 0, size: 36, family: 'Helvetica', color: '#000000', bold: false, italic: false }],
  numPages: 3,
  pages: [
    {
      number: 1,
      width: 892,
      height: 1263,
      images: [],
      strings: [
        { top: 187.152, left: 108, width: 114.08399999999997, height: 33.30000000000001, font: 0, text: 'Page 1' },
        { top: 262.152, left: 108, width: 68.00400000000005, height: 33.30000000000001, font: 0, text: 'Visit',
          link: 'https://example.com/poppler-native' },
        { top: 337.152, left: 108, width: 208.15199999999993, height: 33.30000000000001, font: 0, text: 'Go to page 3',
          link: 'this#3' },
      ],
    },
    // pages 2 and 3
  ],
}
```

And an image, from [`test/fixtures/images.pdf`](test/fixtures/images.pdf):

```js
{ top: 273, left: 90, width: 120, height: 90, src: 'data:image/jpeg;base64,/9j/4AAQSkZJR...' }
```

- **Units**: all positions and sizes are pixels at 108 DPI, measured from the top-left corner of the page — 1.5 × PDF
  points, the same as `pdftohtml -xml`. Divide by 1.5 for points.
- **`strings`** are runs of text in one font, in the order the PDF draws them. `font` indexes into `fonts`, which lists
  every distinct font, size and color used on the pages read.
- **`images`**: a grayscale or RGB JPEG comes out exactly as embedded (`image/jpeg`); everything else — CMYK JPEGs,
  Flate, CCITT, JBIG2, JPEG 2000, image masks — is decoded and re-encoded as PNG.
- **`link`**: a URI, a file name, or `this#<page>` for a page of the same document.
- **`outline`** items have no `page` when their destination can't be resolved, e.g. a link to a website. The outline
  is read up to 256 levels deep.
- Every field is documented in [`index.d.ts`](index.d.ts).

### Errors

Every problem rejects the promise with an `Error` that says what's wrong: `does-not-exist.pdf: couldn't open the PDF
file: No such file or directory`, `PDF file was damaged and couldn't be repaired`, `file was encrypted and password was
incorrect or not supplied`. Invalid arguments reject with a `TypeError` that names them: `firstPage must be a positive
integer, got 0`, `unknown option "firstpage"; info() accepts password, firstPage, lastPage, images,
reconvertThroughPS`. Poppler's own warnings about broken PDFs are not printed; a damaged PDF that can be repaired simply
parses.

### Command line

```sh
npx poppler-native statement.pdf --pretty            # info() as JSON
npx poppler-native statement.pdf --text --layout     # text()
cat statement.pdf | npx poppler-native - --no-images --first-page 2
```

`--password`, `--first-page`, `--last-page`, `--no-images` and `--layout` work like the options above; `--help` lists
them.

### Seeing what was parsed

Open [`misc/pdf-json-viewer.html`](misc/pdf-json-viewer.html) in a browser and drop the JSON from `info()` onto it: it
draws every string and image where the PDF puts it.

## Performance

The [PDF 1.7 specification](https://opensource.adobe.com/dc-acrobat-sdk-docs/pdfstandards/PDF32000_2008.pdf) (756
pages, 22 MB), on one core of a 2.1 GHz Intel Xeon, Linux, Node.js 24:

| | Time |
| --- | --- |
| `text()` | 1.4 s |
| `info()` (85,578 text runs, 841 images) | 2.0 s |
| `info({ images: false })` | 1.7 s |
| `info()` with poppler-native 2.3.0 | 2.8 s |

Every call runs on a thread of Node's thread pool, so calls at once run in parallel: four `info({ images: false })`
of that PDF take 2.2 s together. Node's thread pool has four threads unless you set `UV_THREADPOOL_SIZE`.

## How it works

Poppler parses the PDF and draws every page into an output device derived from Poppler's own `pdftohtml`, which
records each run of text with its font and position, every image and every link, instead of writing HTML. `text()`
uses `pdftotext`'s output device, with the same settings. Poppler and the libraries it needs are compiled into the
addon from unmodified upstream releases, so it depends on nothing on the system.

One addition sits on top: some PDFs draw text with an embedded subset of Arial whose characters cannot be decoded (no
`ToUnicode` map, glyph numbers instead of character codes), which `pdftotext` turns into control characters.
poppler-native then recognizes each glyph by the shape of its outline and reads the character it shows — and leaves
the text alone wherever Poppler's own decoding agrees with the shape.

## Upgrading from 2.x

- `npm install` now gets a prebuilt binary; remove `libjpeg-dev`, `libpng-dev`, `libfreetype6-dev` and `mupdf-tools`
  from your images if nothing else needs them. Ghostscript is still needed for `reconvertThroughPS`.
- Node.js 22 or newer.
- The result has a new `numPages` field and is otherwise the same; poppler-native 3 was checked against 2.3.0 on 100
  real-world PDFs (see [CHANGELOG.md](CHANGELOG.md) for the few differences, all fixes).
- Unknown options and invalid values now reject with a `TypeError` instead of being ignored.

## Building from source

`npm install` compiles from source when no prebuilt binary matches, and `npm install --build-from-source` always does.
It needs Python 3 and a C++23 compiler: GCC 12 or newer, Clang 18 or newer, Xcode 16.4 or newer, or Visual Studio 2022
or 2026 with the "Desktop development with C++" workload. Visual Studio 2026 needs npm 11, which comes with Node.js 24
and newer (on Node.js 22: `npm install -g npm@11`). Nothing else: Poppler and its libraries are part of the package.

Working on poppler-native itself:

```sh
npm install          # compiles the addon
npm test             # Jest: API, output, images, encryption, text repair, CLI, types, concurrency, fuzzing
npm run build        # recompiles after a change to the C++ code
scripts/sanitize.sh  # ASan/UBSan/LSan build: every fixture through every API path, plus a fuzz round (Linux)
node scripts/fuzz.mjs --cases 500 [--dir my-pdfs/]   # feeds mutated PDFs, reports anything that crashes
```

### Updating Poppler

Poppler is vendored unmodified, and its build configuration is generated, so an update is one command:

```sh
node scripts/update-poppler.mjs 26.12.0
```

It downloads the release, verifies its signature against the release key in `scripts/keys/`, copies exactly the core
library's sources and everything they include into `native/poppler/`, takes the list of sources to compile from
upstream's `CMakeLists.txt`, and generates `config.h` and `poppler-config.h` from upstream's templates. If Poppler adds
a configuration option, the script stops and names it, so the new option gets a deliberate value in its `FLAGS`
table. Poppler's internal C++ API changes between releases, so then rebuild (`npm run build`), fix what no longer
compiles in `native/binding/`, and run the tests.

### Updating the bundled libraries

Change `version`, `url` and `sha256` of the library in `scripts/vendor-deps.mjs` and run
`node scripts/vendor-deps.mjs <library>`: it verifies the release, replaces `native/deps/<library>/` and regenerates the
library's configuration headers from its own templates.

## License

GPL-2.0-or-later, because Poppler is. The bundled libraries and their licenses are listed in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
