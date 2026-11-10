// Checks and normalizes the arguments of info() and text(). An unknown option or a wrong value is an error that says
// exactly what is wrong, instead of an option silently doing nothing.
const { types } = require('util');

const OPTIONS = {
  info: ['password', 'firstPage', 'lastPage', 'images', 'reconvertThroughPS'],
  text: ['password', 'firstPage', 'lastPage', 'layout'],
};

const MAX_PAGE = 2 ** 31 - 1;

// The same wording the addon has always used for argument types ('object' for arrays, 'null' for null).
const typeName = value => (value === null ? 'null' : typeof value);

function toPdf(input) {
  if (typeof input === 'string') {
    // as fs does: the addon would read a C string, which ends at the first null byte
    if (input.includes('\0')) throw new TypeError('argument at position 1 is a path with a null byte in it');
    return input;
  }
  if (Buffer.isBuffer(input)) return input;
  if (ArrayBuffer.isView(input)) return Buffer.from(input.buffer, input.byteOffset, input.byteLength);
  // also an ArrayBuffer or SharedArrayBuffer from another realm (a vm context, a worker), unlike instanceof
  if (types.isAnyArrayBuffer(input)) return Buffer.from(input);
  throw new TypeError(`argument at position 1 has wrong type: expected string or Buffer, got ${typeName(input)}`);
}

function checkOption(key, value) {
  switch (key) {
    case 'password':
      if (typeof value !== 'string') throw new TypeError(`password must be a string, got ${typeName(value)}`);
      break;
    case 'firstPage':
    case 'lastPage':
      if (!Number.isSafeInteger(value) || value < 1) {
        throw new TypeError(`${key} must be a positive integer, got ${typeof value === 'number' ? value : typeName(value)}`);
      }
      break;
    default:
      if (typeof value !== 'boolean') throw new TypeError(`${key} must be a boolean, got ${typeName(value)}`);
  }
}

// Returns { pdf, options }: pdf a path or a Buffer (a view on the caller's memory, not a copy), options with every
// value checked and undefined values dropped.
function checkArguments(fn, args) {
  if (args.length < 1 || args.length > 2) {
    throw new TypeError(`wrong number of arguments: expected 1-2, got ${args.length}`);
  }
  const pdf = toPdf(args[0]);
  const given = args[1] === undefined ? {} : args[1];
  if (typeof given !== 'object' || given === null) {
    throw new TypeError(`argument at position 2 has wrong type: expected object, got ${typeName(given)}`);
  }
  const options = {};
  for (const [key, value] of Object.entries(given)) {
    if (!OPTIONS[fn].includes(key)) {
      throw new TypeError(`unknown option "${key}"; ${fn}() accepts ${OPTIONS[fn].join(', ')}`);
    }
    if (value === undefined) continue;
    checkOption(key, value);
    options[key] = value;
  }
  if (options.firstPage !== undefined && options.lastPage !== undefined && options.firstPage > options.lastPage) {
    throw new RangeError(`firstPage (${options.firstPage}) is after lastPage (${options.lastPage})`);
  }
  // The addon counts pages in 32 bits. A larger number means past the end just the same; it must not wrap around.
  for (const key of ['firstPage', 'lastPage']) {
    if (options[key] > MAX_PAGE) options[key] = MAX_PAGE;
  }
  return { pdf, options };
}

module.exports = { checkArguments };
