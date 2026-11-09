{
  'includes': ['native/common.gypi'],
  'targets': [
    {
      'target_name': 'poppler',
      'sources': [
        'native/binding/html/HtmlFonts.cc',
        'native/binding/html/HtmlLinks.cc',
        'native/binding/html/HtmlOutputDev.cc',
        'native/binding/binding.cpp',
        'native/binding/NodeUtilities.cpp',
        'native/binding/PDFUtilities.cpp',
        'native/binding/PngEncoder.cpp',
        'native/binding/ReadPDFOutputs.cpp',
        'native/binding/text-repair/GlyphFingerprint.cpp',
        'native/binding/text-repair/ReferenceFonts.cpp',
        'native/binding/text-repair/TextRepair.cpp',
      ],
      'include_dirs': [
        "<!@(node -p \"require('node-addon-api').include\")",
      ],
      'dependencies': [
        "<!(node -p \"require('node-addon-api').gyp\")",
        'native/poppler/poppler.gyp:libpoppler',
        'native/deps/deps.gyp:libpng',
      ],
      'defines': ['NAPI_CPP_EXCEPTIONS'],
      'conditions': [
        ['OS=="linux"', {
          # Poppler and the vendored libraries are linked statically; nothing but Node-API's entry points leaves
          # the addon, so no symbol can clash with another copy of the same library elsewhere in the process.
          'ldflags': ['-Wl,--exclude-libs,ALL', '-Wl,--version-script=<(module_root_dir)/native/exports.map'],
        }],
      ],
    },
  ],
}
