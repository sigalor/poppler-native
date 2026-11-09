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


class Pdf:
    def __init__(self):
        self.objects = []

    def add(self, body):
        self.objects.append(body)
        return len(self.objects)

    def stream(self, data, extra=b''):
        return self.add(b'<< /Length %d%s >>\nstream\n' % (len(data), extra) + data + b'\nendstream')

    def write(self, path, fonts, lines):
        """fonts: {resource name: font object number}; lines: [(font resource, y, hex string)]."""
        content = b'BT\n' + b''.join(
            b'/%s 24 Tf 1 0 0 1 72 %d Tm <%s> Tj\n' % (name.encode(), y, codes.encode()) for name, y, codes in lines
        ) + b'ET\n'
        contents = self.stream(content)
        resources = b' '.join(b'/%s %d 0 R' % (name.encode(), num) for name, num in fonts.items())
        page = self.add(
            b'<< /Type /Page /Parent %d 0 R /MediaBox [0 0 595 842] /Resources << /Font << %s >> >> /Contents %d 0 R >>'
            % (len(self.objects) + 2, resources, contents)
        )
        pages = self.add(b'<< /Type /Pages /Kids [%d 0 R] /Count 1 >>' % page)
        catalog = self.add(b'<< /Type /Catalog /Pages %d 0 R >>' % pages)
        out = io.BytesIO()
        out.write(b'%PDF-1.4\n%\xe2\xe3\xcf\xd3\n')
        offsets = []
        for number, body in enumerate(self.objects, start=1):
            offsets.append(out.tell())
            out.write(b'%d 0 obj\n' % number + body + b'\nendobj\n')
        xref = out.tell()
        out.write(b'xref\n0 %d\n0000000000 65535 f \n' % (len(self.objects) + 1))
        for offset in offsets:
            out.write(b'%010d 00000 n \n' % offset)
        out.write(b'trailer\n<< /Size %d /Root %d 0 R >>\nstartxref\n%d\n%%%%EOF\n' % (len(self.objects) + 1, catalog, xref))
        with open(path, 'wb') as f:
            f.write(out.getvalue())


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

    doc.write(os.path.join(HERE, '..', 'arial-gid-codes.pdf'), {'F1': type0, 'F2': simple}, [('F1', 700, line1), ('F2', 600, line2)])


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
    doc.write(os.path.join(HERE, '..', 'arial-encoded.pdf'), {'F1': simple}, lines)


def main(arial_path):
    unicode_cmap = TTFont(arial_path).getBestCmap()
    gid_codes(arial_path, unicode_cmap)
    encoded(arial_path, unicode_cmap)
    print('wrote arial-gid-codes.pdf and arial-encoded.pdf')


if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit('usage: python3 test/fixtures/generate/make-arial-pdfs.py /path/to/Arial.ttf')
    main(sys.argv[1])
