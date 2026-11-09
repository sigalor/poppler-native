// Builds small PDFs for tests that need inputs too large or too odd to commit as fixtures.

// A PDF from object bodies (object 1 is the catalog), with a correct cross-reference table.
function buildPdf(bodies) {
  const parts = ['%PDF-1.4\n'];
  let length = parts[0].length;
  const offsets = [];
  bodies.forEach((body, i) => {
    offsets.push(length);
    const object = `${i + 1} 0 obj\n${body}\nendobj\n`;
    parts.push(object);
    length += Buffer.byteLength(object, 'latin1');
  });
  const xref = [`xref\n0 ${bodies.length + 1}\n`, '0000000000 65535 f \n'];
  for (const offset of offsets) xref.push(`${String(offset).padStart(10, '0')} 00000 n \n`);
  parts.push(...xref, `trailer\n<< /Size ${bodies.length + 1} /Root 1 0 R >>\nstartxref\n${length}\n%%EOF\n`);
  return Buffer.from(parts.join(''), 'latin1');
}

// One page and an outline nested `depth` levels deep: every item has a single child, and all point to the page.
function deepOutlinePdf(depth) {
  const bodies = [
    '<< /Type /Catalog /Pages 2 0 R /Outlines 4 0 R >>',
    '<< /Type /Pages /Kids [3 0 R] /Count 1 >>',
    '<< /Type /Page /Parent 2 0 R /MediaBox [0 0 200 200] >>',
    `<< /Type /Outlines /First 5 0 R /Last 5 0 R /Count ${depth} >>`,
  ];
  for (let level = 0; level < depth; level++) {
    const number = 5 + level;
    const parent = level === 0 ? 4 : number - 1;
    const kids = level < depth - 1 ? ` /First ${number + 1} 0 R /Last ${number + 1} 0 R /Count 1` : '';
    bodies.push(`<< /Title (Level ${level + 1}) /Parent ${parent} 0 R /Dest [3 0 R /Fit]${kids} >>`);
  }
  return buildPdf(bodies);
}

module.exports = { buildPdf, deepOutlinePdf };
