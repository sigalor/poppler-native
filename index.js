const { constants } = require('buffer');
const fs = require('fs/promises');
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

// The addon needs to know the longest string JavaScript holds: a result that needs a longer one can't be returned.
const limits = { _maxStringLength: constants.MAX_STRING_LENGTH };

async function info(...args) {
  const { pdf, options } = checkArguments('info', args);
  const { reconvertThroughPS, ...rest } = options;
  if (!reconvertThroughPS) return native.info(pdf, { ...rest, ...limits });

  const bytes = typeof pdf === 'string' ? await readPdfFile(pdf) : pdf;
  if (bytes.length === 0) throw new Error('input buffer is empty');
  return native.info(await reconvertThroughPostScript(bytes), { ...rest, ...limits });
}

async function text(...args) {
  const { pdf, options } = checkArguments('text', args);
  return native.text(pdf, { ...options, ...limits });
}

module.exports = { info, text };
