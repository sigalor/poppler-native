# Test fixtures

| File | What it is | How it is made |
| --- | --- | --- |
| `minimal-text.pdf` | one page, one "Hello" in Times New Roman (Chrome "Save as PDF") | — |
| `not-a-pdf.pdf` | text that is not a PDF | — |
| `three-pages.pdf` | "Page 1" to "Page 3" in Helvetica; Unicode metadata; a two-level outline; a URI link and an internal link on page 1 | `python3 test/fixtures/generate/make-fixtures.py three-pages` |
| `encrypted-user.pdf` | `three-pages.pdf`, AES-256, user password `secret`, owner password `owner-secret` | same command (needs qpdf) |
| `encrypted-owner.pdf` | `three-pages.pdf`, AES-256, only an owner password (`owner-secret`), so it opens without one | same command (needs qpdf) |
| `encrypted-quoted.pdf` | `three-pages.pdf`, AES-256, user password `pa ss\"wo\rd` (a space, a backslash before a quote, and one before r) | same command (needs qpdf) |
| `outline-unresolved.pdf` | an outline whose items point to a missing named destination, to a website and to page 1 | `python3 test/fixtures/generate/make-fixtures.py outline-unresolved` |
| `arial-not-embedded.pdf` | text in a TrueType Arial that is not embedded, followed by a JPEG | `python3 test/fixtures/generate/make-fixtures.py arial-not-embedded` |
| `images.pdf` | one image per way poppler-native emits images: a JPEG (passed through), a Flate RGB image, a 1-bit image mask, a JPEG 2000 image and a CMYK JPEG (the last four re-encoded as PNG) | `python3 test/fixtures/generate/make-fixtures.py images` (needs Pillow) |
| `arial-gid-codes.pdf` | Arial subset without ToUnicode whose codes are glyph indices: a Type0 /Identity-H line (decodes to control characters) and a symbolic TrueType line (decodes to nothing) | `python3 test/fixtures/generate/make-arial-pdfs.py <Arial 2.82 .ttf>` |
| `arial-encoded.pdf` | WinAnsi-encoded Arial subset without ToUnicode that Poppler decodes correctly, incl. a Cyrillic A sharing Latin A's outline | same script |
| `fingerprint-test-font.ttf` | synthetic TrueType font for the glyph-fingerprint tests | `python3 test/fixtures/generate/make-test-font.py` (needs fontTools) |

Every generated fixture is committed; regenerating one is only needed when its generator changes. Regenerating
`three-pages.pdf` also rewrites both encrypted copies, which come out different every time (encryption is salted).
