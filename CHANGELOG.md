# 1.2.0 (2020-01-28)

- fixed exception handling from C++ promise and improved Poppler error reporting
- modified `info` to accept `Buffer` input as well and added unit tests for that
- some internal refactorings

# 1.1.0 (2020-01-27)

- renamed `readPDF` to `info`, which is now promise-based (refactored internal N-API output object generation for that)
- added unit tests for minimal test PDF using Jest
- removed debug flags from node-gyp binding

# 1.0.0 (2020-01-19)

- read PDFs from files with Poppler 0.83
