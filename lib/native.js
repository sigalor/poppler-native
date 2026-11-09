// Loads the compiled addon. Everything in the package goes through this module, so there is one place that
// decides where the binary comes from.
module.exports = require('bindings')('poppler.node');
