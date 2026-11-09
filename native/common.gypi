# Compiler settings shared by every target of poppler-native (the addon, Poppler and the vendored C libraries).
#
# Node's own common.gypi builds addons as C++20 without exceptions and RTTI. Poppler 26.x is C++23, and our
# output device runs inside Poppler's stack frames, so an exception thrown there (std::bad_alloc, for one) must
# be able to unwind through Poppler's code: everything is built with exceptions and RTTI enabled.
{
  'target_defaults': {
    'cflags!': ['-fno-exceptions'],
    'cflags_cc!': ['-fno-exceptions', '-fno-rtti', '-std=gnu++20'],
    'cflags': ['-fvisibility=hidden'],
    'cflags_cc': ['-std=gnu++23', '-fexceptions', '-frtti', '-fvisibility-inlines-hidden'],
    'configurations': {
      'Release': {
        # Poppler's own release builds define NDEBUG; without it an assert() on a malformed PDF would abort Node.
        'defines': ['NDEBUG'],
      },
    },
    'conditions': [
      ['OS=="linux"', {
        'defines': ['_DEFAULT_SOURCE'],
      }],
      ['OS=="mac"', {
        'xcode_settings': {
          'CLANG_CXX_LANGUAGE_STANDARD': 'gnu++23',
          'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
          'GCC_ENABLE_CPP_RTTI': 'YES',
          'GCC_SYMBOLS_PRIVATE_EXTERN': 'YES',
          'MACOSX_DEPLOYMENT_TARGET': '11.0',
        },
      }],
      ['OS=="win"', {
        'defines': ['WIN32_LEAN_AND_MEAN', 'NOMINMAX', '_CRT_SECURE_NO_WARNINGS', '_CRT_NONSTDC_NO_DEPRECATE'],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'LanguageStandard': 'stdcpplatest',
            'ExceptionHandling': 1,
            'RuntimeTypeInfo': 'true',
            'AdditionalOptions': ['/utf-8', '/Zc:__cplusplus', '/bigobj'],
          },
        },
      }],
    ],
  },
}
