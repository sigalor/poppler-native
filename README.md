# Poppler for Node

[![GitHub license](https://img.shields.io/github/license/sigalor/poppler-native)](https://github.com/sigalor/poppler-native/blob/master/LICENSE) [![npm](https://img.shields.io/npm/v/poppler-native)](https://www.npmjs.com/package/poppler-native) [![Unit tests workflow status](https://github.com/sigalor/poppler-native/actions/workflows/tests.yaml/badge.svg)](https://github.com/sigalor/poppler-native/actions/workflows/tests.yaml)

Allows you to use the native Poppler C++ backend to efficiently parse PDF files from NodeJS. Outputs similar information to `pdftohtml -xml -stdout test.pdf` (with `pdftohtml` from the `poppler-utils` package), because it uses parts of the same codebase which have been rewritten to output N-API objects instead of XML code. All contained functions return JavaScript promises.

## Getting started

1. dependencies: `build-essential`; curl, PNG, JPEG and FreeType development headers; ghostscript, python, mupdf-tools, qpdf
2. `npm install poppler-native` (only tested on Ubuntu 16.04 and 20.04 so far)

```javascript
// allows filename...
const pdf = require('poppler-native');
pdf.info('test.pdf').then(res => console.log(res));

// ...or buffer with raw PDF bytes directly
const fs = require('fs-extra');
fs.readFile('test.pdf')
  .then(f => pdf.info(f))
  .then(res => console.log(res));

// you also have the option to convert the PDF to a PS and then reconvert it to PDF again via GhostScript before extracting data from it, which can sometimes help when wrong characters are extracted with the default method:
pdf.info('test.pdf', { reconvertThroughPS: true });
```

In order to visualize the parsed text boxes and images, you can also write the entire output from the `pdf.info` function into a JSON file, then open the file `misc/pdf-json-viewer.html` in any web browser and drag-and-drop the JSON file there.

## Contributing

### Updating Poppler

This is obviously only necessary when a new version of Poppler is released. According to their readme, the internal Poppler C++ API, which is the foundation of this project, might be subject to breaking changes, even in minor releases. Consequently, evaluate new Poppler versions thoroughly before updating.

1. Download the Poppler sources from [here](https://poppler.freedesktop.org/releases.html).
2. Put all `*.h`, `*.c` and `*.cc` files from `poppler-20.11.0/goo` into `native/poppler/goo`. The same for `fofi` and `poppler`. Do not change the existing two config files.
3. Remove the following files from the `native/poppler/poppler` directory: `CairoFontEngine.cc CairoFontEngine.h CairoOutputDev.cc CairoOutputDev.h CairoRescaleBox.cc CairoRescaleBox.h GlobalParamsWin.cc JPEG2000Stream.cc JPEG2000Stream.h SignatureHandler.cc SignatureHandler.h SplashOutputDev.cc SplashOutputDev.h`
4. Remove the line `#include "splash/SplashTypes.h"` from `native/poppler/poppler/GfxState.cc`.

## License

GPLv2 or later, because the Poppler source is bundled.
