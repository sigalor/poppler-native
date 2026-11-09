const fs = require('fs/promises');
const native = require('./lib/native');
const { reconvertThroughPostScript } = require('./lib/ghostscript');

// The C library's wording (strerror) for the errors a file read can run into, so a file that can't be read is
// reported the same way whichever path reads it.
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

// The addon validates arguments and reads files itself; only reconvertThroughPS needs work in JavaScript first.
async function info(...args) {
  const [input, options] = args;
  const reconvert =
    args.length === 2 && options !== null && typeof options === 'object' && options.reconvertThroughPS === true;
  if (!reconvert || (typeof input !== 'string' && !ArrayBuffer.isView(input))) return native.info(...args);

  const pdf = typeof input === 'string' ? await readPdfFile(input) : input;
  if (pdf.length === 0) throw new Error('input buffer is empty');
  return native.info(await reconvertThroughPostScript(pdf), options);
}

module.exports.info = info;
