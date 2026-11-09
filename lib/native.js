// Loads the compiled addon: the prebuilt binary for this platform when the package ships one, otherwise the one
// `npm install` compiled into build/. Everything in the package goes through this module.
const path = require('path');

module.exports = require('node-gyp-build')(path.join(__dirname, '..'));
