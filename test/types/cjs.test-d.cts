// Compiled (never run) by test/types.test.js: the CommonJS entry, as require() sees it.
import popplerNative = require('poppler-native');
import { info, type PdfInfo } from 'poppler-native';

async function usage(): Promise<void> {
  const document: PdfInfo = await popplerNative.info('statement.pdf');
  const text: string = await popplerNative.text(Buffer.from([]));
  const same: PdfInfo = await info('statement.pdf', { images: false });
  void [document, text, same];
  // @ts-expect-error -- the CommonJS module has no default export
  void popplerNative.default;
}
void usage;
