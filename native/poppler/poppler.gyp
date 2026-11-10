# Poppler's core library (goo, fofi, poppler, splash), vendored pristine by scripts/update-poppler.mjs.
# The source list lives in sources.gypi, which that script generates from upstream's CMakeLists.txt.
{
  'includes': ['../common.gypi'],
  'targets': [
    {
      'target_name': 'libpoppler',
      'type': 'static_library',
      'includes': ['sources.gypi', '../no-node-headers.gypi'],
      'include_dirs': ['.', 'poppler'],
      'direct_dependent_settings': {
        'include_dirs': ['.', 'poppler'],
      },
      # Upstream code: build it as upstream does and keep the user's install log free of its warnings.
      'cflags': ['-w'],
      'xcode_settings': { 'WARNING_CFLAGS': ['-w'] },
      'msvs_settings': { 'VCCLCompilerTool': { 'WarningLevel': '0' } },
      'dependencies': [
        '../deps/deps.gyp:freetype',
        '../deps/deps.gyp:libjpeg',
        '../deps/deps.gyp:libpng',
        '../deps/deps.gyp:openjpeg',
        '../deps/deps.gyp:zlib',
      ],
      'export_dependent_settings': ['../deps/deps.gyp:freetype'],
    },
  ],
}
