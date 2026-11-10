// The ES module entry point: everything index.js exports, as named exports and as the default export.
export * from './index.js';
import type * as popplerNative from './index.js';

declare const defaultExport: typeof popplerNative;
export default defaultExport;
