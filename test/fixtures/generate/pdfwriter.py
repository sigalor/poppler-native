"""A tiny PDF writer for the test fixtures: numbered objects, streams and a classic xref table, nothing else, so
every byte of a fixture is something its generator decided."""
import io


class Pdf:
    def __init__(self):
        self.objects = []

    def reserve(self):
        """Allocates an object number to fill in later with set()."""
        self.objects.append(None)
        return len(self.objects)

    def set(self, number, body):
        self.objects[number - 1] = body

    def add(self, body):
        self.objects.append(body)
        return len(self.objects)

    def stream(self, data, extra=b''):
        return self.add(b'<< /Length %d%s >>\nstream\n' % (len(data), extra) + data + b'\nendstream')

    def write(self, path, root, info=None):
        out = io.BytesIO()
        out.write(b'%PDF-1.7\n%\xe2\xe3\xcf\xd3\n')
        offsets = []
        for number, body in enumerate(self.objects, start=1):
            assert body is not None, f'object {number} was reserved but never set'
            offsets.append(out.tell())
            out.write(b'%d 0 obj\n' % number + body + b'\nendobj\n')
        xref = out.tell()
        out.write(b'xref\n0 %d\n0000000000 65535 f \n' % (len(self.objects) + 1))
        for offset in offsets:
            out.write(b'%010d 00000 n \n' % offset)
        trailer = b'/Size %d /Root %d 0 R' % (len(self.objects) + 1, root)
        if info:
            trailer += b' /Info %d 0 R' % info
        out.write(b'trailer\n<< %s >>\nstartxref\n%d\n%%%%EOF\n' % (trailer, xref))
        with open(path, 'wb') as f:
            f.write(out.getvalue())


def utf16(text):
    """A PDF text string in UTF-16BE with a byte order mark, as a hex string."""
    return b'<FEFF' + text.encode('utf-16-be').hex().upper().encode() + b'>'
