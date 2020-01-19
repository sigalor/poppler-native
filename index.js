const poppler = require('bindings')('poppler.node');

module.exports.readPDF = poppler.readPDF;
