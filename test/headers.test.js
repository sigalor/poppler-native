const fs = require('fs');
const path = require('path');

const root = path.join(__dirname, '..');

// Every standard C and C++ header a compiler might look up by name. On a case-insensitive file system (macOS and
// Windows by default) `#include <version>` also finds a file called VERSION, so no file in one of our include
// directories may be named like one of them, in any case.
const STANDARD_HEADERS = `algorithm any array atomic barrier bit bitset charconv chrono codecvt compare complex concepts
  condition_variable coroutine debugging deque exception execution expected filesystem flat_map flat_set format
  forward_list fstream functional future generator hazard_pointer initializer_list inplace_vector iomanip ios iosfwd
  iostream istream iterator latch limits linalg list locale map mdspan memory memory_resource mutex new numbers numeric
  optional ostream print queue random ranges ratio rcu regex scoped_allocator semaphore set shared_mutex simd
  source_location span spanstream sstream stack stacktrace stdexcept stdfloat stop_token streambuf string string_view
  strstream syncstream system_error text_encoding thread tuple type_traits typeindex typeinfo unordered_map
  unordered_set utility valarray variant vector version cassert cctype cerrno cfenv cfloat cinttypes climits clocale
  cmath csetjmp csignal cstdarg cstddef cstdint cstdio cstdlib cstring ctime cuchar cwchar cwctype assert.h ctype.h
  errno.h fenv.h float.h inttypes.h limits.h locale.h math.h setjmp.h signal.h stdarg.h stddef.h stdint.h stdio.h
  stdlib.h string.h time.h uchar.h wchar.h wctype.h`.split(/\s+/);

// The include directories of every target, from the gyp files themselves (plain paths; node-addon-api's directory
// comes from a command there, so it is added directly).
function includeDirs() {
  const dirs = [path.resolve(root, require('node-addon-api').include_dir)];
  for (const gyp of ['binding.gyp', 'native/poppler/poppler.gyp', 'native/deps/deps.gyp']) {
    const text = fs.readFileSync(path.join(root, gyp), 'utf8');
    for (const [, list] of text.matchAll(/'include_dirs':\s*\[([^\]]*)\]/g)) {
      for (const [, dir] of list.matchAll(/(?:^|,)\s*'([^'<]+)'/g)) dirs.push(path.join(root, path.dirname(gyp), dir));
    }
  }
  return [...new Set(dirs)];
}

test('no file in an include directory is named like a standard header', () => {
  const dirs = includeDirs();
  expect(dirs.length).toBeGreaterThan(8);
  const clashes = dirs.flatMap(dir =>
    fs
      .readdirSync(dir)
      .filter(name => STANDARD_HEADERS.includes(name.toLowerCase()))
      .map(name => path.relative(root, path.join(dir, name))),
  );
  expect(clashes).toEqual([]);
});
