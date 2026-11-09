// Type declarations of poppler-native. All coordinates and sizes are pixels at 108 DPI (1.5 x PDF points), measured
// from the top-left corner of the page, the same units `pdftohtml -xml` uses.

/** A PDF: the path of its file, or its bytes (a Buffer, any typed array or DataView, or an ArrayBuffer). */
export type PdfInput = string | ArrayBufferView | ArrayBuffer | SharedArrayBuffer;

export interface DocumentOptions {
  /**
   * Password of an encrypted PDF: its user password or its owner password. A PDF that only has an owner password
   * opens without one.
   */
  password?: string;
  /** First page to read, counting from 1. Default: 1. */
  firstPage?: number;
  /** Last page to read. Default: the last page; a number beyond the end reads up to the end. */
  lastPage?: number;
}

export interface InfoOptions extends DocumentOptions {
  /** Extract the images (as `data:` URLs). Default: true. `false` is faster and makes the result much smaller. */
  images?: boolean;
  /**
   * Run the PDF through Ghostscript -- to PostScript and back to PDF -- before reading it, which can help with PDFs
   * whose fonts map characters oddly. Needs Ghostscript (`gs`, or `gswin64c` on Windows) on the PATH, and the result
   * depends on its version. Default: false.
   */
  reconvertThroughPS?: boolean;
}

export interface TextOptions extends DocumentOptions {
  /** Keep the physical layout of the page, like `pdftotext -layout`, instead of reading order. Default: false. */
  layout?: boolean;
}

export interface PdfInfo {
  /** The text entries of the document information dictionary that are present and not empty. */
  meta: PdfMeta;
  /** The document outline (bookmarks), when the PDF has one. */
  outline?: PdfOutlineItem[];
  /** The fonts used on the pages read. A string's `font` is an index into this array. */
  fonts: PdfFont[];
  /** The number of pages in the document, whether or not they were read. */
  numPages: number;
  /** The pages read, in order. */
  pages: PdfPage[];
}

export interface PdfMeta {
  Title?: string;
  Author?: string;
  Subject?: string;
  Keywords?: string;
  Creator?: string;
  Producer?: string;
  /** A PDF date string, e.g. `D:20260101120000Z`. */
  CreationDate?: string;
  /** A PDF date string, e.g. `D:20260101120000Z`. */
  ModDate?: string;
}

export interface PdfOutlineItem {
  title: string;
  /** The page the item points to, counting from 1; missing when its destination can't be resolved. */
  page?: number;
  /** The items one level down. The outline is read up to 256 levels deep. */
  children?: PdfOutlineItem[];
}

export interface PdfFont {
  /** This font's index in `fonts`. */
  id: number;
  /** The font size in pixels, rounded down. */
  size: number;
  /** The PDF font's name without a style suffix such as `-Bold` or `-Italic`. */
  family: string;
  /** The text color, as `#rrggbb`. */
  color: string;
  bold: boolean;
  italic: boolean;
}

export interface PdfPage {
  /** The page number, counting from 1. */
  number: number;
  width: number;
  height: number;
  images: PdfImage[];
  /** The page's text, in content-stream order, as runs of characters in one font. */
  strings: PdfString[];
}

export interface PdfString {
  top: number;
  left: number;
  width: number;
  height: number;
  /** Index into `PdfInfo.fonts`. */
  font: number;
  text: string;
  /**
   * The target of a link covering this text: a URI, a file name, or `this#<page>` for a page of this document.
   */
  link?: string;
}

export interface PdfImage {
  top: number;
  left: number;
  width: number;
  height: number;
  /** The image as a `data:` URL: `image/jpeg` for a JPEG exactly as embedded, `image/png` for everything else. */
  src: string;
}

/** Reads a PDF: its text with positions and fonts, its images, links, outline and metadata. */
export function info(input: PdfInput, options?: InfoOptions): Promise<PdfInfo>;

/** Reads a PDF's text, like `pdftotext`: in reading order, each page ending with a form feed (`\f`). */
export function text(input: PdfInput, options?: TextOptions): Promise<string>;
