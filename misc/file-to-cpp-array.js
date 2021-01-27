const fs = require('fs-extra');

(async () => {
  if (process.argv.length < 5) {
    console.log('usage: node ' + process.argv[1] + ' [file] [namespace] [C identifier]');
    process.exit(0);
  }

  const namespace = process.argv[3];
  const identifier = process.argv[4];
  let outputC = 'namespace ' + namespace + ' {\n\nunsigned char ' + identifier + '[] = {';

  const buf = await fs.readFile(process.argv[2]);
  for (const [i, b] of buf.entries()) {
    if (i % 16 == 0) outputC += '\n  ';
    outputC += '0x' + b.toString(16).padStart(2, '0');
    if (i != buf.length - 1) outputC += ',';
  }
  outputC += '\n};\n\nunsigned int ' + identifier + '_length = ' + buf.length + ';\n\n}\n';

  const outputH =
    '#pragma once\n\nnamespace ' +
    namespace +
    ' {\n\nextern unsigned char ' +
    identifier +
    '[];\nextern unsigned int ' +
    identifier +
    '_length;\n\n}\n';

  await fs.writeFile(identifier + '.hpp', outputH);
  await fs.writeFile(identifier + '.cpp', outputC);
})();
