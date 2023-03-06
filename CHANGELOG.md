# 2.2.1 (2023-03-06)

- fix: do not output page numbers on stdout

# 2.2.0 (2023-02-17)

- update Poppler to 23.02
- update NPM dependencies

# 2.1.3 (2021-05-26)

- update Poppler to 21.05.0
- update NPM dependencies

# 2.1.2 (2021-05-01)

- small fix in binding C++ code
- added `misc/pdf-json-viewer.html` to visualize the parsing results

# 2.1.1 (2021-04-23)

- fix issues with filename input and font extraction

# 2.1.0 (2021-01-27)

- implement charmap reconstruction, such that text in the fonts Arial and Arial Bold can now be obtained even if no `ToUnicode` character map is present in the PDF

# 2.0.1 (2020-11-03)

- fix .npmignore to not contain build directory

# 2.0.0 (2020-11-03)

- update to Poppler 20.11.0 (the JS API of this package stayed the same, but the information returned by Poppler might be very slightly different now)

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
