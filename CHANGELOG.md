# 3.0.0 (unreleased)

Poppler 26.11.0, prebuilt binaries, and everything a new user would expect from a PDF library in 2026.

**Installing**

- Prebuilt binaries for Linux (x64 and arm64, glibc and musl), macOS (Apple silicon and Intel) and Windows (x64 and
  arm64): `npm install` downloads nothing else and compiles nothing. No system packages are needed any more -- not
  libjpeg, libpng, FreeType, curl, nor mupdf-tools.
- Where no prebuilt binary fits, or the one that fits doesn't load (a musl system other than Alpine, say), the install
  compiles from source with nothing but Python 3 and a C++23 compiler: Poppler and every library it needs (zlib 1.4,
  libpng 1.6.59, libjpeg-turbo 3.2.1, OpenJPEG 2.5.4, FreeType 2.15.0) are bundled. It compiles on every core, but
  runs no more compilers at once than memory allows (a container sees all cores and part of the memory).
- Node.js 22 or newer.

**New**

- `text()` returns a PDF's text like `pdftotext`, optionally keeping the physical layout.
- Options for `info()`: `password` (encrypted PDFs), `firstPage` / `lastPage`, `images: false` (faster, much smaller
  results).
- `numPages` in the result of `info()`.
- Any typed array or `ArrayBuffer` is accepted besides a path or a `Buffer`.
- ES module entry point and TypeScript declarations.
- A command line: `npx poppler-native file.pdf [--text] [--pretty] ...`.
- JPEG 2000 images are decoded through OpenJPEG (Poppler 26.07 removed its built-in decoder).

**Changed**

- Poppler 21.05.0 → 26.11.0: 68 releases, 43 of them with crash fixes (33 for malformed or broken files). The output
  stays the same: on 100 real-world PDFs, the only differences are Type 3 fonts that now report their real name, text
  that the repair below now reads correctly, and PNG images that compress to different bytes with identical pixels.
- Faster: `info()` reads the 756-page PDF 1.7 specification in 2.0 s instead of 2.8 s.
- Text drawn with an embedded Arial subset without a ToUnicode map is read again: the repair has been silently off
  since mupdf 1.18 (October 2020) changed the file names of `mutool extract`. It now works inside the addon, without
  mutool, by the shape of the glyph actually drawn, and leaves text alone that Poppler decodes correctly.
- The Arial fonts are no longer shipped: a table of glyph-outline fingerprints replaces 2.9 MB of font data.
- Poppler no longer prints warnings to stderr; errors reject the promise.
- An unknown option or an invalid option value rejects with a `TypeError` that names it, instead of being ignored.
- An outline entry whose destination can't be resolved has no `page` (it was an uninitialized number).
- The outline is read up to 256 levels deep: a crafted PDF nesting thousands of levels took quadratic time and then
  crashed the process.
- A directory passed as the file rejects the promise; 2.3.0 aborted the Node process.
- A path with a null byte in it is refused, as `fs` does; 2.3.0 read the path cut at the null byte.
- Files are read off the JavaScript thread, and `info()` can be called concurrently from many places at once.
- Safe with worker threads: terminating a worker while it parses no longer aborts the process.
- A result JavaScript can't hold rejects with a message saying what to do, instead of crashing the process: an image
  whose data URL would be longer than the longest string, images too large together for a quarter of the JavaScript
  heap, or text longer than a string (a few kilobytes of compressed data can inflate to gigabytes). Reading stops at
  that point, so memory stays in proportion to those limits.
- `reconvertThroughPS` runs Ghostscript without a shell, works with `password` (handed over in a private file, not
  on the command line) and cleans up after itself; it still needs Ghostscript (`gs`).

# 2.3.0 (2023-09-21)

- add `reconvertThroughPS` parameter (Chris031196) [#11](https://github.com/sigalor/poppler-native/pull/11)

# 2.2.2 (2023-03-06)

- revert to Poppler 21.05.0

# 2.2.1 (2023-03-06)

- fix: do not output page numbers on stdout

# 2.2.0 (2023-02-17)

- update Poppler to 23.02
- update NPM dependencies

# 2.1.3 (2021-05-26)

- update Poppler to 21.05.0
- update NPM dependencies

# 2.1.2 (2021-05-01)

- small fix in binding C++ code
- added `misc/pdf-json-viewer.html` to visualize the parsing results

# 2.1.1 (2021-04-23)

- fix issues with filename input and font extraction

# 2.1.0 (2021-01-27)

- implement charmap reconstruction, such that text in the fonts Arial and Arial Bold can now be obtained even if no `ToUnicode` character map is present in the PDF

# 2.0.1 (2020-11-03)

- fix .npmignore to not contain build directory

# 2.0.0 (2020-11-03)

- update to Poppler 20.11.0 (the JS API of this package stayed the same, but the information returned by Poppler might be very slightly different now)

# 1.2.0 (2020-01-28)

- fixed exception handling from C++ promise and improved Poppler error reporting
- modified `info` to accept `Buffer` input as well and added unit tests for that
- some internal refactorings

# 1.1.0 (2020-01-27)

- renamed `readPDF` to `info`, which is now promise-based (refactored internal N-API output object generation for that)
- added unit tests for minimal test PDF using Jest
- removed debug flags from node-gyp binding

# 1.0.0 (2020-01-19)

- read PDFs from files with Poppler 0.83
