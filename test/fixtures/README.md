# Test fixtures

| File | What it is | How it is made |
| --- | --- | --- |
| `minimal-text.pdf` | one page, one "Hello" in Times New Roman (Chrome "Save as PDF") | — |
| `not-a-pdf.pdf` | text that is not a PDF | — |
| `fingerprint-test-font.ttf` | synthetic TrueType font for the glyph-fingerprint tests | `python3 test/fixtures/generate/make-test-font.py` (needs fontTools) |

Every generated fixture is committed; regenerating one is only needed when its generator changes.
| `arial-gid-codes.pdf` | Arial subset without ToUnicode whose codes are glyph indices: a Type0 /Identity-H line (decodes to control characters) and a symbolic TrueType line (decodes to nothing) | `python3 test/fixtures/generate/make-arial-pdfs.py <Arial 2.82 .ttf>` |
| `arial-encoded.pdf` | WinAnsi-encoded Arial subset without ToUnicode that Poppler decodes correctly, incl. a Cyrillic A sharing Latin A's outline | same script |
