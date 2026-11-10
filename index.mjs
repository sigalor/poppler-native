import { createRequire } from 'node:module';

const popplerNative = createRequire(import.meta.url)('./index.js');

export const { info, text } = popplerNative;
export default popplerNative;
