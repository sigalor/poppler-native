// reconvertThroughPS: runs the PDF through Ghostscript -- to PostScript and back to PDF -- which sometimes yields
// better text for PDFs whose fonts encode characters oddly. Ghostscript is an external program; it is only needed
// for this option.
const { execFile } = require('child_process');
const fs = require('fs/promises');
const os = require('os');
const path = require('path');

const COMMANDS = process.platform === 'win32' ? ['gswin64c', 'gswin32c', 'gs'] : ['gs'];

// Runs Ghostscript; resolves with what it printed, which can report a problem although it exits with 0.
function run(command, args, cwd) {
  return new Promise((resolve, reject) => {
    execFile(command, args, { cwd, windowsHide: true, maxBuffer: 64 * 1024 * 1024 }, (error, stdout, stderr) => {
      const output = `${stdout}${stderr}`.trim();
      if (error) {
        error.output = output;
        reject(error);
      } else {
        resolve(output);
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

// The password of an encrypted PDF goes to Ghostscript in an argument file, so that it doesn't show in the process
// list. There, a quoted argument holds any character but a line break or a null byte; inside it, \" stands for a
// quote and every other backslash for itself. So escaping the quotes writes any password -- except one that ends with
// a backslash, which would escape the closing quote.
function passwordArgument(password) {
  if (/[\r\n\0]/.test(password) || password.endsWith('\\')) {
    throw new Error(
      "reconvertThroughPS can't hand this password to Ghostscript: it holds a line break or a null byte, or ends with " +
        'a backslash',
    );
  }
  return `-sPDFPassword="${password.replace(/"/g, '\\"')}"\n`;
}

// PDF bytes in, reconverted PDF bytes out. Works in a private temporary directory that is always removed, with
// relative file names (Ghostscript reads a % in an output file name as a page number template).
async function reconvertThroughPostScript(pdf, password) {
  const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'poppler-native-'));
  try {
    await fs.writeFile(path.join(dir, 'input.pdf'), pdf);
    const args = ['-o', 'converted.ps', '-dNOCACHE', '-sDEVICE=ps2write', 'input.pdf'];
    if (password !== undefined) {
      await fs.writeFile(path.join(dir, 'password.txt'), passwordArgument(password), { mode: 0o600 });
      args.unshift('@password.txt');
    }
    const output = await ghostscript(args, dir);
    if (/requires a password/.test(output)) throw new Error('file was encrypted and password was incorrect or not supplied');
    await ghostscript(['-o', 'reconverted.pdf', '-sDEVICE=pdfwrite', 'converted.ps'], dir);
    return await fs.readFile(path.join(dir, 'reconverted.pdf'));
  } finally {
    await fs.rm(dir, { recursive: true, force: true });
  }
}

module.exports = { reconvertThroughPostScript, passwordArgument };
