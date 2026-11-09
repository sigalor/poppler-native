// Checks and normalizes the arguments of info() and text(). An unknown option or a wrong value is an error that says
// exactly what is wrong, instead of an option silently doing nothing.
const OPTIONS = {
  info: ['password', 'firstPage', 'lastPage', 'images', 'reconvertThroughPS'],
  text: ['password', 'firstPage', 'lastPage', 'layout'],
};

// The same wording the addon has always used for argument types ('object' for arrays, 'null' for null).
const typeName = value => (value === null ? 'null' : typeof value);

function toPdf(input) {
  if (typeof input === 'string' || Buffer.isBuffer(input)) return input;
  if (ArrayBuffer.isView(input)) return Buffer.from(input.buffer, input.byteOffset, input.byteLength);
  if (input instanceof ArrayBuffer || (typeof SharedArrayBuffer !== 'undefined' && input instanceof SharedArrayBuffer)) {
    return Buffer.from(input);
  }
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
  return { pdf, options };
}

module.exports = { checkArguments };
