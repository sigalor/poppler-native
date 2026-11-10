// Compiled (never run) by test/types.test.js: the ES module entry, imported by default and by name.
import popplerNative, { info, text, type InfoOptions, type PdfInfo, type PdfString } from 'poppler-native';

async function usage(bytes: Uint8Array, buffer: ArrayBuffer): Promise<void> {
  const document: PdfInfo = await info('statement.pdf');
  const numPages: number = document.numPages;
  const title: string | undefined = document.meta.Title;
  const strings: PdfString[] = document.pages[0].strings;
  const link: string | undefined = strings[0].link;
  const size: number = document.fonts[strings[0].font].size;
  const outlinePage: number | undefined = document.outline?.[0].page;
  const src: string = document.pages[0].images[0].src;
  void [numPages, title, link, size, outlinePage, src];

  const options: InfoOptions = { password: 'secret', firstPage: 2, lastPage: 3, images: false, reconvertThroughPS: true };
  await info(bytes, options);
  await info(buffer);
  const plain: string = await text('statement.pdf', { layout: true, password: 'x', firstPage: 1, lastPage: 2 });
  void plain;
  const viaDefault: PdfInfo = await popplerNative.info('statement.pdf');
  void viaDefault;

  // @ts-expect-error -- a number is not a PDF
  await info(123);
  // @ts-expect-error -- unknown option
  await info('statement.pdf', { firstpage: 2 });
  // @ts-expect-error -- text() has no images option
  await text('statement.pdf', { images: false });
  // @ts-expect-error -- pages are numbers
  await info('statement.pdf', { firstPage: '2' });
}
void usage;
