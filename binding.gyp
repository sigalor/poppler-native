{
  'includes': ['native/common.gypi'],
  'targets': [
    {
      'target_name': 'poppler',
      'sources': [
        'native/binding/html/HtmlFonts.cc',
        'native/binding/html/HtmlLinks.cc',
        'native/binding/html/HtmlOutputDev.cc',
        'native/binding/html/InMemoryFile.cc',
        'native/binding/binding.cpp',
        'native/binding/NodeUtilities.cpp',
        'native/binding/PDFUtilities.cpp',
        'native/binding/ReadPDFOutputs.cpp',
        'native/binding/text-repair/GlyphFingerprint.cpp',
        'native/binding/text-repair/ReferenceFonts.cpp',
        'native/binding/text-repair/TextRepair.cpp',
      ],
      'include_dirs': [
        "<!@(node -p \"require('node-addon-api').include\")",
        '/usr/include/freetype2',
      ],
      'dependencies': [
        "<!(node -p \"require('node-addon-api').gyp\")",
        'native/poppler/poppler.gyp:libpoppler',
      ],
      'defines': ['NAPI_CPP_EXCEPTIONS'],
      'libraries': ['-ljpeg', '-lpng', '-lfreetype', '-lopenjp2', '-lz'],
    },
  ],
}
