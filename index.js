const { constants } = require('buffer');
const fs = require('fs/promises');
const v8 = require('v8');
const native = require('./lib/native');
const { checkArguments } = require('./lib/arguments');
const { reconvertThroughPostScript } = require('./lib/ghostscript');

// The C library's wording (strerror) for the errors a file read can run into, so a file that can't be read is
// reported the same way whether the addon reads it or reconvertThroughPS does.
const SYSTEM_ERRORS = {
  EACCES: 'Permission denied',
  EISDIR: 'Is a directory',
  ELOOP: 'Too many levels of symbolic links',
  EMFILE: 'Too many open files',
  ENAMETOOLONG: 'File name too long',
  ENOENT: 'No such file or directory',
  ENOTDIR: 'Not a directory',
  EPERM: 'Operation not permitted',
};

async function readPdfFile(filename) {
  try {
    return await fs.readFile(filename);
  } catch (e) {
    throw new Error(`${filename}: couldn't open the PDF file: ${SYSTEM_ERRORS[e.code] || e.message}`);
  }
}

// What a result can hold, which the addon needs to know: no string can be longer than the longest JavaScript holds,
// and all images of a document together may take a quarter of the heap (as data URLs, one byte per character).
const limits = {
  _maxStringLength: constants.MAX_STRING_LENGTH,
  _maxImageBytes: Math.floor(v8.getHeapStatistics().heap_size_limit / 4),
};

// Ghostscript handles a missing or wrong password badly (an empty document, or a crash), so Poppler checks it first,
// without reading any page, and refuses as info() does without reconvertThroughPS. Any other problem is left to
// Ghostscript, which can often convert a PDF that Poppler can't open: that is what reconvertThroughPS is for.
const ENCRYPTED = 'file was encrypted and password was incorrect or not supplied';
async function checkPassword(bytes, password) {
  try {
    await native.info(bytes, { password, images: false, firstPage: 2 ** 31 - 1, ...limits });
  } catch (e) {
    if (e.message === ENCRYPTED) throw e;
  }
}

async function info(...args) {
  const { pdf, options } = checkArguments('info', args);
  const { reconvertThroughPS, ...rest } = options;
  if (!reconvertThroughPS) return native.info(pdf, { ...rest, ...limits });

  const bytes = typeof pdf === 'string' ? await readPdfFile(pdf) : pdf;
  if (bytes.length === 0) throw new Error('input buffer is empty');
  await checkPassword(bytes, rest.password);
  return native.info(await reconvertThroughPostScript(bytes, rest.password), { ...rest, ...limits });
}

async function text(...args) {
  const { pdf, options } = checkArguments('text', args);
  return native.text(pdf, { ...options, ...limits });
}

module.exports = { info, text };
