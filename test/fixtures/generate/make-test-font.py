"""Builds test/fixtures/fingerprint-test-font.ttf, a synthetic TrueType font for the glyph-fingerprint tests.

Every glyph is a plain polygon drawn here, so the font carries no third-party design or license. It exercises
the cases the fingerprint table must get right:
  - two codepoints on one glyph (U+0020/U+00A0 on 'space', U+0041/U+0391 on 'A'),
  - two glyphs with identical outlines (B/B.dup, C.alt/C), where the glyph with the lower index decides the
    canonical codepoint even when its codepoint is the higher one (C.alt -> U+0421 comes before C -> U+0043),
  - an empty outline ('space') and an unmapped glyph ('.notdef', which must not appear in the table).

Usage: python3 test/fixtures/generate/make-test-font.py   (needs fontTools)
"""
import os

from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen


def polygon(*points):
    pen = TTGlyphPen(None)
    pen.moveTo(points[0])
    for point in points[1:]:
        pen.lineTo(point)
    pen.closePath()
    return pen.glyph()


def empty():
    return TTGlyphPen(None).glyph()


GLYPH_ORDER = ['.notdef', 'space', 'C.alt', 'A', 'B', 'B.dup', 'C', 'D']
GLYPHS = {
    '.notdef': polygon((0, 0), (0, 700), (500, 700), (500, 0)),
    'space': empty(),
    'C.alt': polygon((100, 100), (100, 300), (300, 300), (300, 100)),
    'A': polygon((10, 0), (250, 700), (490, 0)),
    'B': polygon((20, 0), (20, 700), (480, 700), (480, 0)),
    'B.dup': polygon((20, 0), (20, 700), (480, 700), (480, 0)),
    'C': polygon((100, 100), (100, 300), (300, 300), (300, 100)),
    'D': polygon((30, 0), (30, 700), (300, 700), (470, 350), (300, 0)),
}
CMAP = {
    0x20: 'space',
    0xA0: 'space',
    0x41: 'A',
    0x391: 'A',
    0x42: 'B',
    0x412: 'B.dup',
    0x43: 'C',
    0x421: 'C.alt',
    0x44: 'D',
}


def main():
    builder = FontBuilder(1000, isTTF=True)
    builder.setupGlyphOrder(GLYPH_ORDER)
    builder.setupCharacterMap(CMAP)
    builder.setupGlyf(GLYPHS)
    builder.setupHorizontalMetrics({name: (600, 0) for name in GLYPH_ORDER})
    builder.setupHorizontalHeader(ascent=800, descent=-200)
    builder.setupNameTable({'familyName': 'PopplerNativeTest', 'styleName': 'Regular'})
    builder.setupOS2(sTypoAscender=800, sTypoDescender=-200, usWinAscent=800, usWinDescent=200)
    builder.setupPost()
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'fingerprint-test-font.ttf')
    builder.save(out)
    print('wrote', os.path.normpath(out))


main()
