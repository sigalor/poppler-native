# The C libraries Poppler and poppler-native need, vendored by scripts/vendor-deps.mjs and linked statically into the
# addon: building needs no system library or header, and the addon depends on nothing but the C/C++ runtime.
#
# Every library is built with hidden symbol visibility, and the addon links them with --exclude-libs on Linux, so
# none of their symbols can clash with another copy of the same library elsewhere in the Node process (Node's own
# zlib, or another addon's libpng or FreeType).
{
  'includes': ['../common.gypi'],
  'target_defaults': {
    'includes': ['../no-node-headers.gypi'],
    # Upstream code: built as upstream builds it, without its warnings in the user's install log.
    'cflags': ['-w'],
    'xcode_settings': { 'WARNING_CFLAGS': ['-w'] },
    'msvs_settings': { 'VCCLCompilerTool': { 'WarningLevel': '0' } },
  },
  'targets': [
    {
      'target_name': 'zlib',
      'type': 'static_library',  # node-gyp's defaults would make it a loadable module
      'includes': ['zlib.sources.gypi'],
      # Z_PREFIX renames every zlib symbol (deflate -> z_deflate) for zlib itself and everything compiled against it.
      'defines': ['Z_PREFIX'],
      'include_dirs': ['zlib'],
      'direct_dependent_settings': {
        'defines': ['Z_PREFIX'],
        'include_dirs': ['zlib'],
      },
    },
    {
      'target_name': 'libpng',
      'type': 'static_library',  # node-gyp's defaults would make it a loadable module
      'includes': ['libpng.sources.gypi'],
      'dependencies': ['zlib'],
      'export_dependent_settings': ['zlib'],
      # No hand-written SIMD: one build for every CPU, and libpng only writes PNGs here.
      'defines': [
        'PNG_ARM_NEON_OPT=0',
        'PNG_INTEL_SSE_OPT=0',
        'PNG_LOONGARCH_LSX_OPT=0',
        'PNG_MIPS_MMI_OPT=0',
        'PNG_MIPS_MSA_OPT=0',
        'PNG_POWERPC_VSX_OPT=0',
        'PNG_RISCV_RVV_OPT=0',
      ],
      'include_dirs': ['config/libpng', 'libpng'],
      'direct_dependent_settings': {
        'include_dirs': ['config/libpng', 'libpng'],
      },
    },
    {
      'target_name': 'libjpeg',
      'type': 'static_library',  # node-gyp's defaults would make it a loadable module
      'includes': ['libjpeg-turbo.sources.gypi'],
      'include_dirs': ['config/libjpeg-turbo', 'libjpeg-turbo/src'],
      'direct_dependent_settings': {
        'include_dirs': ['config/libjpeg-turbo', 'libjpeg-turbo/src'],
      },
    },
    {
      'target_name': 'openjpeg',
      'type': 'static_library',  # node-gyp's defaults would make it a loadable module
      'includes': ['openjpeg.sources.gypi'],
      # a static OpenJPEG: no dllimport on Windows, hidden visibility elsewhere (openjpeg.h's own OPJ_API logic)
      'defines': ['OPJ_STATIC'],
      'include_dirs': ['config/openjpeg', 'openjpeg'],
      'direct_dependent_settings': {
        'defines': ['OPJ_STATIC'],
        'include_dirs': ['config/openjpeg', 'openjpeg'],
      },
    },
    {
      'target_name': 'freetype',
      'type': 'static_library',  # node-gyp's defaults would make it a loadable module
      'includes': ['freetype.sources.gypi'],
      'defines': ['FT2_BUILD_LIBRARY'],
      # config/freetype holds our ftoption.h and ftmodule.h, which must shadow FreeType's own
      'include_dirs': ['config/freetype', 'freetype/include'],
      'direct_dependent_settings': {
        'include_dirs': ['config/freetype', 'freetype/include'],
      },
    },
  ],
}
