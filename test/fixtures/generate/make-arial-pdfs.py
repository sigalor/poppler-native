"""Builds the text-repair fixtures: PDFs drawing text with an embedded Arial subset that has no ToUnicode map.

  arial-gid-codes.pdf  the cases the repair exists for. Line 1 uses a Type0 font with /Identity-H and a
                       CIDFontType2 descendant (CIDToGIDMap /Identity), as real bank statements do: its codes are
                       glyph indices, which Poppler can only decode as U+<glyph index>, i.e. control characters.
                       Line 2 uses a symbolic simple TrueType font without /Encoding whose (3,0) cmap maps
                       0xF000+gid to gid: Poppler finds no Unicode at all and drops the text.
  arial-encoded.pdf    the case the repair must leave alone: a WinAnsi-encoded subset that Poppler decodes correctly
                       on its own, including a Cyrillic A (via /Differences) whose outline is identical to Latin A.

Arial permits embedding (OS/2 fsType 0), which is all these documents do. Regenerating them needs an Arial TTF
(version 2.82, the one the fingerprint table was made from):

  python3 test/fixtures/generate/make-arial-pdfs.py /path/to/Arial.ttf   (needs fontTools)
"""
import io
import os
import sys

from fontTools import subset
from fontTools.ttLib import TTFont
from fontTools.ttLib.tables._c_m_a_p import cmap_format_4

from pdfwriter import Pdf

HERE = os.path.dirname(os.path.abspath(__file__))
TEXT = 'Hello Wörld 123'
DESCRIPTOR = b'/Ascent 905 /Descent -212 /CapHeight 716 /StemV 80 /ItalicAngle 0 /FontBBox [-665 -325 2000 1006]'


def make_subset(arial_path, glyph_names, cmap):
    """Subsets Arial to glyph_names (renumbered, no glyph names left). cmap: 'unicode' keeps the Unicode cmap,
    'symbol' replaces it with a (3,0) cmap mapping 0xF000+gid to each glyph."""
    font = TTFont(arial_path)
    options = subset.Options()
    options.retain_gids = False
    options.glyph_names = False  # post format 3: no names for Poppler to fall back on
    options.notdef_outline = True
    options.name_IDs = []
    options.layout_features = []
    subsetter = subset.Subsetter(options)
    subsetter.populate(glyphs=glyph_names)
    subsetter.subset(font)
    if cmap == 'symbol':
        table = cmap_format_4(4)
        table.platformID, table.platEncID, table.language = 3, 0, 0
        table.cmap = {0xF000 + gid: name for gid, name in enumerate(font.getGlyphOrder()) if gid > 0}
        font['cmap'].tables = [table]
    out = io.BytesIO()
    font.save(out)
    return out.getvalue(), font


def advance(font, glyph_name):
    return round(font['hmtx'][glyph_name][0] * 1000 / font['head'].unitsPerEm)


def write(doc, path, fonts, lines):
    """One A4 page drawing each (font resource, y, hex codes) line at 24 pt."""
    content = b'BT\n' + b''.join(
        b'/%s 24 Tf 1 0 0 1 72 %d Tm <%s> Tj\n' % (name.encode(), y, codes.encode()) for name, y, codes in lines
    ) + b'ET\n'
    contents = doc.stream(content)
    resources = b' '.join(b'/%s %d 0 R' % (name.encode(), num) for name, num in fonts.items())
    pages = doc.reserve()
    page = doc.add(b'<< /Type /Page /Parent %d 0 R /MediaBox [0 0 595 842] /Resources << /Font << %s >> >> /Contents %d 0 R >>'
                   % (pages, resources, contents))
    doc.set(pages, b'<< /Type /Pages /Kids [%d 0 R] /Count 1 >>' % page)
    doc.write(path, doc.add(b'<< /Type /Catalog /Pages %d 0 R >>' % pages))


def gid_codes(arial_path, unicode_cmap):
    names = sorted({unicode_cmap[ord(c)] for c in TEXT})
    doc = Pdf()

    # line 1: Type0 /Identity-H, CIDFontType2, CIDToGIDMap /Identity, 2-byte codes = glyph indices
    program, font = make_subset(arial_path, names, 'unicode')
    gid_of = {name: gid for gid, name in enumerate(font.getGlyphOrder())}
    file1 = doc.stream(program, b' /Length1 %d' % len(program))
    desc1 = doc.add(b'<< /Type /FontDescriptor /FontName /PNTCID+ArialMT /Flags 32 %s /FontFile2 %d 0 R >>' % (DESCRIPTOR, file1))
    widths = b' '.join(b'%d [%d]' % (gid, advance(font, name)) for name, gid in sorted(gid_of.items(), key=lambda x: x[1]))
    cid = doc.add(
        b'<< /Type /Font /Subtype /CIDFontType2 /BaseFont /PNTCID+ArialMT /CIDSystemInfo << /Registry (Adobe) '
        b'/Ordering (Identity) /Supplement 0 >> /CIDToGIDMap /Identity /DW 1000 /W [%s] /FontDescriptor %d 0 R >>' % (widths, desc1)
    )
    type0 = doc.add(b'<< /Type /Font /Subtype /Type0 /BaseFont /PNTCID+ArialMT /Encoding /Identity-H /DescendantFonts [%d 0 R] >>' % cid)
    line1 = ''.join('%04x' % gid_of[unicode_cmap[ord(c)]] for c in TEXT)

    # line 2: simple symbolic TrueType, no /Encoding, (3,0) cmap 0xF000+gid -> gid, 1-byte codes = glyph indices
    program, font = make_subset(arial_path, names, 'symbol')
    gid_of = {name: gid for gid, name in enumerate(font.getGlyphOrder())}
    assert max(gid_of.values()) < 256
    file2 = doc.stream(program, b' /Length1 %d' % len(program))
    desc2 = doc.add(b'<< /Type /FontDescriptor /FontName /PNTSYM+ArialMT /Flags 4 %s /FontFile2 %d 0 R >>' % (DESCRIPTOR, file2))
    widths = [0] * 256
    for name, gid in gid_of.items():
        widths[gid] = advance(font, name)
    simple = doc.add(
        b'<< /Type /Font /Subtype /TrueType /BaseFont /PNTSYM+ArialMT /FirstChar 0 /LastChar 255 /Widths [%s] '
        b'/FontDescriptor %d 0 R >>' % (b' '.join(b'%d' % w for w in widths), desc2)
    )
    line2 = ''.join('%02x' % gid_of[unicode_cmap[ord(c)]] for c in TEXT)

    write(doc, os.path.join(HERE, '..', 'arial-gid-codes.pdf'), {'F1': type0, 'F2': simple}, [('F1', 700, line1), ('F2', 600, line2)])


def encoded(arial_path, unicode_cmap):
    cyrillic_a = unicode_cmap[0x410]
    program, font = make_subset(arial_path, sorted({unicode_cmap[ord(c)] for c in TEXT} | {cyrillic_a}), 'unicode')
    doc = Pdf()
    winansi = {c: c.encode('cp1252')[0] for c in TEXT}
    widths = [0] * 256
    for c in TEXT:
        widths[winansi[c]] = advance(font, unicode_cmap[ord(c)])
    widths[192] = advance(font, cyrillic_a)
    file = doc.stream(program, b' /Length1 %d' % len(program))
    desc = doc.add(b'<< /Type /FontDescriptor /FontName /PNTENC+ArialMT /Flags 32 %s /FontFile2 %d 0 R >>' % (DESCRIPTOR, file))
    simple = doc.add(
        b'<< /Type /Font /Subtype /TrueType /BaseFont /PNTENC+ArialMT /FirstChar 0 /LastChar 255 /Widths [%s] '
        b'/FontDescriptor %d 0 R /Encoding << /Type /Encoding /BaseEncoding /WinAnsiEncoding /Differences [192 /afii10017] >> >>'
        % (b' '.join(b'%d' % w for w in widths), desc)
    )
    lines = [('F1', 700, ''.join('%02x' % winansi[c] for c in TEXT)), ('F1', 600, 'c0')]
    write(doc, os.path.join(HERE, '..', 'arial-encoded.pdf'), {'F1': simple}, lines)


def main(arial_path):
    unicode_cmap = TTFont(arial_path).getBestCmap()
    gid_codes(arial_path, unicode_cmap)
    encoded(arial_path, unicode_cmap)
    print('wrote arial-gid-codes.pdf and arial-encoded.pdf')


if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit('usage: python3 test/fixtures/generate/make-arial-pdfs.py /path/to/Arial.ttf')
    main(sys.argv[1])
