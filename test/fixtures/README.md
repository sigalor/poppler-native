# Test fixtures

| File | What it is | How it is made |
| --- | --- | --- |
| `minimal-text.pdf` | one page, one "Hello" in Times New Roman (Chrome "Save as PDF") | — |
| `not-a-pdf.pdf` | text that is not a PDF | — |
| `fingerprint-test-font.ttf` | synthetic TrueType font for the glyph-fingerprint tests | `python3 test/fixtures/generate/make-test-font.py` (needs fontTools) |

Every generated fixture is committed; regenerating one is only needed when its generator changes.
