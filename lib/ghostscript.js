// reconvertThroughPS: runs the PDF through Ghostscript -- to PostScript and back to PDF -- which sometimes yields
// better text for PDFs whose fonts encode characters oddly. Ghostscript is an external program; it is only needed
// for this option.
const { execFile } = require('child_process');
const fs = require('fs/promises');
const os = require('os');
const path = require('path');

const COMMANDS = process.platform === 'win32' ? ['gswin64c', 'gswin32c', 'gs'] : ['gs'];

function run(command, args, cwd) {
  return new Promise((resolve, reject) => {
    execFile(command, args, { cwd, windowsHide: true, maxBuffer: 64 * 1024 * 1024 }, (error, stdout, stderr) => {
      if (error) {
        error.output = `${stdout}${stderr}`.trim();
        reject(error);
      } else {
        resolve();
      }
    });
  });
}

async function ghostscript(args, cwd) {
  for (const command of COMMANDS) {
    try {
      return await run(command, args, cwd);
    } catch (e) {
      if (e.code === 'ENOENT') continue;
      throw new Error(`Ghostscript could not convert the PDF: ${e.output || e.message}`);
    }
  }
  throw new Error(
    `reconvertThroughPS requires Ghostscript, but ${COMMANDS.join(' or ')} was not found on the PATH; install it ` +
      '(e.g. apt install ghostscript, brew install ghostscript, choco install ghostscript)',
  );
}

// PDF bytes in, reconverted PDF bytes out. Works in a private temporary directory that is always removed.
async function reconvertThroughPostScript(pdf) {
  const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'poppler-native-'));
  try {
    const input = path.join(dir, 'input.pdf');
    const postscript = path.join(dir, 'converted.ps');
    const output = path.join(dir, 'reconverted.pdf');
    await fs.writeFile(input, pdf);
    await ghostscript(['-o', postscript, '-dNOCACHE', '-sDEVICE=ps2write', input], dir);
    await ghostscript(['-o', output, '-sDEVICE=pdfwrite', postscript], dir);
    return await fs.readFile(output);
  } finally {
    await fs.rm(dir, { recursive: true, force: true });
  }
}

module.exports = { reconvertThroughPostScript };
