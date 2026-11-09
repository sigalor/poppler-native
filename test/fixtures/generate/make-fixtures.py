"""Builds the general test fixtures (needs Pillow, for the JPEG and JPEG 2000 images, and qpdf, for encryption):

  three-pages.pdf      "Page 1".."Page 3" in Helvetica; Unicode metadata; a two-level outline; a URI link and an
                       internal link on page 1
  encrypted-user.pdf   three-pages.pdf, AES-256, user password "secret"
  encrypted-owner.pdf  three-pages.pdf, AES-256, owner password only (opens without a password)
  outline-unresolved.pdf  an outline whose items point to a missing named destination, to a website and to page 1
  images.pdf           one page with one image per way poppler-native emits images: a JPEG (passed through as is),
                       a Flate RGB image, a 1-bit image mask, a JPEG 2000 image and a CMYK JPEG (both re-encoded
                       as PNG)

Usage: python3 test/fixtures/generate/make-fixtures.py [three-pages | outline-unresolved | images ...]
(all of them by default; three-pages also writes the two encrypted copies, which come out different every time)
"""
import io
import os
import subprocess
import sys
import zlib

from PIL import Image

from pdfwriter import Pdf, utf16

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..')
A4 = b'[0 0 595 842]'


def three_pages():
    doc = Pdf()
    pages = doc.reserve()
    page_numbers = [doc.reserve() for _ in range(3)]
    font = doc.add(b'<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>')
    link_uri = doc.add(
        b'<< /Type /Annot /Subtype /Link /Rect [70 640 160 670] /Border [0 0 0] '
        b'/A << /S /URI /URI (https://example.com/poppler-native) >> >>'
    )
    link_goto = doc.add(
        b'<< /Type /Annot /Subtype /Link /Rect [70 590 220 620] /Border [0 0 0] '
        b'/Dest [%d 0 R /XYZ 0 842 0] >>' % page_numbers[2]
    )
    for index, number in enumerate(page_numbers, start=1):
        lines = [(700, b'Page %d' % index)]
        if index == 1:
            lines += [(650, b'Visit'), (600, b'Go to page 3')]
        content = b'BT /F1 24 Tf ' + b''.join(b'1 0 0 1 72 %d Tm (%s) Tj ' % (y, text) for y, text in lines) + b'ET\n'
        annots = b' /Annots [%d 0 R %d 0 R]' % (link_uri, link_goto) if index == 1 else b''
        doc.set(
            number,
            b'<< /Type /Page /Parent %d 0 R /MediaBox %s /Resources << /Font << /F1 %d 0 R >> >> /Contents %d 0 R%s >>'
            % (pages, A4, font, doc.stream(content), annots),
        )
    doc.set(pages, b'<< /Type /Pages /Kids [%s] /Count 3 >>' % b' '.join(b'%d 0 R' % n for n in page_numbers))

    outlines = doc.reserve()
    chapter1, section, chapter2 = doc.reserve(), doc.reserve(), doc.reserve()
    doc.set(outlines, b'<< /Type /Outlines /First %d 0 R /Last %d 0 R /Count 3 >>' % (chapter1, chapter2))
    doc.set(chapter1, b'<< /Title %s /Parent %d 0 R /Next %d 0 R /First %d 0 R /Last %d 0 R /Count 1 /Dest [%d 0 R /Fit] >>'
            % (utf16('Chapter 1 – Überblick'), outlines, chapter2, section, section, page_numbers[0]))
    doc.set(section, b'<< /Title (Section 1.1) /Parent %d 0 R /Dest [%d 0 R /Fit] >>' % (chapter1, page_numbers[1]))
    doc.set(chapter2, b'<< /Title (Chapter 2) /Parent %d 0 R /Prev %d 0 R /Dest [%d 0 R /Fit] >>'
            % (outlines, chapter1, page_numbers[2]))

    info = doc.add(
        b'<< /Title %s /Author (poppler-native) /Subject (Test fixture) /Creator (make-fixtures.py) '
        b'/CreationDate (D:20260101120000Z) >>' % utf16('Über ✓ Test')
    )
    catalog = doc.add(b'<< /Type /Catalog /Pages %d 0 R /Outlines %d 0 R /PageMode /UseOutlines >>' % (pages, outlines))
    path = os.path.join(OUT, 'three-pages.pdf')
    doc.write(path, catalog, info)
    return path


def outline_unresolved():
    doc = Pdf()
    pages = doc.reserve()
    font = doc.add(b'<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>')
    page = doc.add(b'<< /Type /Page /Parent %d 0 R /MediaBox %s /Resources << /Font << /F1 %d 0 R >> >> /Contents %d 0 R >>'
                   % (pages, A4, font, doc.stream(b'BT /F1 24 Tf 72 700 Td (Only page) Tj ET\n')))
    doc.set(pages, b'<< /Type /Pages /Kids [%d 0 R] /Count 1 >>' % page)
    outlines, missing, website, first = doc.reserve(), doc.reserve(), doc.reserve(), doc.reserve()
    doc.set(outlines, b'<< /Type /Outlines /First %d 0 R /Last %d 0 R /Count 3 >>' % (missing, first))
    doc.set(missing, b'<< /Title (Missing) /Parent %d 0 R /Next %d 0 R /Dest (no-such-destination) >>' % (outlines, website))
    doc.set(website, b'<< /Title (Website) /Parent %d 0 R /Prev %d 0 R /Next %d 0 R /A << /S /URI /URI (https://example.com/) >> >>'
            % (outlines, missing, first))
    doc.set(first, b'<< /Title (Page 1) /Parent %d 0 R /Prev %d 0 R /Dest [%d 0 R /Fit] >>' % (outlines, website, page))
    catalog = doc.add(b'<< /Type /Catalog /Pages %d 0 R /Outlines %d 0 R >>' % (pages, outlines))
    doc.write(os.path.join(OUT, 'outline-unresolved.pdf'), catalog)


def jpeg(image, **options):
    out = io.BytesIO()
    image.save(out, 'JPEG', quality=90, **options)
    return out.getvalue()


def images():
    gradient = Image.new('RGB', (8, 6))
    gradient.putdata([(x * 32, y * 40, 128) for y in range(6) for x in range(8)])
    rgb_pixels = bytes([255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 0,
                        0, 255, 255, 255, 0, 255, 0, 0, 0, 255, 255, 255,
                        128, 128, 128, 64, 64, 64, 32, 32, 32, 16, 16, 16])  # 4 x 3
    mask_bits = bytes([0b10101010, 0b01010101])  # 8 x 2, 1 bit per pixel
    jpx_image = Image.new('RGB', (5, 4))
    jpx_image.putdata([(x * 50, y * 60, 200) for y in range(4) for x in range(5)])
    jpx = io.BytesIO()
    jpx_image.save(jpx, 'JPEG2000', irreversible=False)  # lossless
    cmyk = Image.new('CMYK', (4, 4), (0, 128, 255, 0))

    doc = Pdf()
    xobjects = [
        doc.stream(jpeg(gradient), b' /Type /XObject /Subtype /Image /Width 8 /Height 6 /ColorSpace /DeviceRGB '
                   b'/BitsPerComponent 8 /Filter /DCTDecode'),
        doc.stream(zlib.compress(rgb_pixels), b' /Type /XObject /Subtype /Image /Width 4 /Height 3 '
                   b'/ColorSpace /DeviceRGB /BitsPerComponent 8 /Filter /FlateDecode'),
        doc.stream(mask_bits, b' /Type /XObject /Subtype /Image /Width 8 /Height 2 /ImageMask true '
                   b'/BitsPerComponent 1'),
        doc.stream(jpx.getvalue(), b' /Type /XObject /Subtype /Image /Width 5 /Height 4 /Filter /JPXDecode'),
        doc.stream(jpeg(cmyk), b' /Type /XObject /Subtype /Image /Width 4 /Height 4 /ColorSpace /DeviceCMYK '
                   b'/BitsPerComponent 8 /Filter /DCTDecode /Decode [1 0 1 0 1 0 1 0]'),
    ]
    content = b''.join(b'q 80 0 0 60 %d 600 cm /Im%d Do Q\n' % (60 + 100 * i, i) for i in range(len(xobjects)))
    content = b'0.2 0.4 0.6 rg\n' + content
    resources = b'/XObject << %s >>' % b' '.join(b'/Im%d %d 0 R' % (i, n) for i, n in enumerate(xobjects))
    pages = doc.reserve()
    page = doc.add(b'<< /Type /Page /Parent %d 0 R /MediaBox %s /Resources << %s >> /Contents %d 0 R >>'
                   % (pages, A4, resources, doc.stream(content)))
    doc.set(pages, b'<< /Type /Pages /Kids [%d 0 R] /Count 1 >>' % page)
    doc.write(os.path.join(OUT, 'images.pdf'), doc.add(b'<< /Type /Catalog /Pages %d 0 R >>' % pages))


def encrypted(source):
    def qpdf(user, out):
        subprocess.run(['qpdf', '--encrypt', user, 'owner-secret', '256', '--', source,
                        os.path.join(OUT, out)], check=True)
    qpdf('secret', 'encrypted-user.pdf')
    qpdf('', 'encrypted-owner.pdf')


FIXTURES = {
    'three-pages': lambda: encrypted(three_pages()),
    'outline-unresolved': outline_unresolved,
    'images': images,
}


def main():
    for name in sys.argv[1:] or FIXTURES:
        FIXTURES[name]()
        print('wrote', name)


main()
