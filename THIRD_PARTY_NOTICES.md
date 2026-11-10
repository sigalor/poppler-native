# Third-party software in poppler-native

poppler-native compiles the following software into its binary. Each is used unmodified, as released upstream; the
scripts that vendor them (`scripts/update-poppler.mjs`, `scripts/vendor-deps.mjs`) verify every release against its
pinned checksum and, where upstream signs releases, its signature.

| Software | Version | License | Full text |
| --- | --- | --- | --- |
| [Poppler](https://poppler.freedesktop.org/) (with code from [Xpdf](https://www.xpdfreader.com/)) | 26.11.0 | GPL-2.0-or-later; parts from Xpdf GPL-2.0 or GPL-3.0 | [`native/poppler/COPYING`](native/poppler/COPYING), [`COPYING3`](native/poppler/COPYING3), [`README-XPDF`](native/poppler/README-XPDF) |
| [zlib](https://zlib.net/) | 1.4 | Zlib | [`native/deps/zlib/LICENSE`](native/deps/zlib/LICENSE) |
| [libpng](http://www.libpng.org/pub/png/libpng.html) | 1.6.59 | PNG Reference Library License, version 2 (libpng-2.0) | [`native/deps/libpng/LICENSE`](native/deps/libpng/LICENSE) |
| [libjpeg-turbo](https://libjpeg-turbo.org/) | 3.2.1 | IJG, BSD-3-Clause and Zlib | [`native/deps/libjpeg-turbo/LICENSE.md`](native/deps/libjpeg-turbo/LICENSE.md), [`README.ijg`](native/deps/libjpeg-turbo/README.ijg) |
| [OpenJPEG](https://www.openjpeg.org/) | 2.5.4 | BSD-2-Clause | [`native/deps/openjpeg/LICENSE`](native/deps/openjpeg/LICENSE) |
| [FreeType](https://freetype.org/) | 2.15.0 | FTL or GPL-2.0-or-later (used here under the GPL) | [`native/deps/freetype/LICENSE.TXT`](native/deps/freetype/LICENSE.TXT), [`FTL.TXT`](native/deps/freetype/FTL.TXT), [`GPLv2.TXT`](native/deps/freetype/GPLv2.TXT) |

This software is based in part on the work of the Independent JPEG Group, and portions of it are copyright © The
FreeType Project (https://freetype.org). All rights reserved.

poppler-native as a whole is distributed under the GNU General Public License, version 2 or later (see
[`LICENSE`](LICENSE)), as Poppler requires.

## Glyph fingerprints

`native/binding/text-repair/reference-fingerprints.inc` holds hashes of glyph outlines of the Arial and Arial Bold
fonts (version 2.82), each mapped to the Unicode characters that glyph draws. It identifies characters drawn with
subsets of these fonts that PDFs embed; it contains no font program and no outline from which a glyph could be drawn.
