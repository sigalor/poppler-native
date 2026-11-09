# Compiler settings shared by every target of poppler-native (the addon, Poppler and the vendored C libraries).
#
# Node's own common.gypi builds addons as C++17 (Node 22) or C++20 (Node 24 and newer), without exceptions and RTTI.
# Poppler 26.x is C++23, and our output device runs inside Poppler's stack frames, so an exception thrown there
# (std::bad_alloc, for one) must be able to unwind through Poppler's code: everything is built as C++23, with
# exceptions and RTTI enabled. C++23 is spelled gnu++2b, which GCC 11+ and Clang 15+ all accept.
{
  'target_defaults': {
    'cflags!': ['-fno-exceptions'],
    'cflags_cc!': ['-fno-exceptions', '-fno-rtti', '-std=gnu++17', '-std=gnu++20'],
    'cflags': ['-fvisibility=hidden'],
    'cflags_cc': ['-std=gnu++2b', '-fexceptions', '-frtti', '-fvisibility-inlines-hidden'],
    'configurations': {
      'Release': {
        # Poppler's own release builds define NDEBUG; without it an assert() on a malformed PDF would abort Node.
        'defines': ['NDEBUG'],
        # Node's release settings for MSVC take precedence over a target's own, so these two go here:
        'msvs_settings': {
          'VCCLCompilerTool': {
            # Node turns RTTI off; Poppler needs it (dynamic_cast).
            'RuntimeTypeInfo': 'true',
            # Node builds with /fp:strict on Windows arm64, where MSVC then rejects floating-point constant
            # expressions in C initializers (OpenJPEG has some). /fp:precise is MSVC's default, as on x64. Node sets
            # its value in a condition, so ours is one too: conditions are applied in order, after plain settings.
            'conditions': [['target_arch=="arm64"', {'FloatingPointModel': 0}]],
          },
        },
      },
    },
    'conditions': [
      # The addon uses a fraction of Poppler (no forms, annotations, signing or saving): every function and variable
      # goes into its own section and the linker drops the ones nothing calls, a quarter of the binary. On Windows,
      # Node's release settings already do this (/Gy, /OPT:REF).
      ['OS=="linux"', {
        'defines': ['_DEFAULT_SOURCE'],
        'cflags': ['-ffunction-sections', '-fdata-sections'],
        'ldflags': ['-Wl,--gc-sections'],
      }],
      ['OS=="mac"', {
        'xcode_settings': {
          'CLANG_CXX_LANGUAGE_STANDARD': 'gnu++2b',
          'DEAD_CODE_STRIPPING': 'YES',
          'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
          'GCC_ENABLE_CPP_RTTI': 'YES',
          'GCC_SYMBOLS_PRIVATE_EXTERN': 'YES',
          'MACOSX_DEPLOYMENT_TARGET': '11.0',
          # Node also sets CLANG_CXX_LANGUAGE_STANDARD, and which of the two wins depends on the order gyp merges
          # them; these flags come last on the command line, so C++23 wins either way.
          'OTHER_CPLUSPLUSFLAGS': ['$(inherited)', '-std=gnu++2b'],
        },
      }],
      ['OS=="win"', {
        # Node defines _HAS_EXCEPTIONS=0, which puts Microsoft's standard library in its no-exceptions mode: there,
        # std::runtime_error keeps only a pointer to its message, so what() of an exception thrown with a temporary
        # string reads freed memory. Every target uses the standard library with exceptions, consistently.
        'defines!': ['_HAS_EXCEPTIONS=0'],
        'defines': ['_HAS_EXCEPTIONS=1', 'WIN32_LEAN_AND_MEAN', 'NOMINMAX', '_CRT_SECURE_NO_WARNINGS', '_CRT_NONSTDC_NO_DEPRECATE'],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'LanguageStandard': 'stdcpplatest',
            'ExceptionHandling': 1,
            'RuntimeTypeInfo': 'true',
            # Node passes -std:c++20 among the options, which come after LanguageStandard and would win.
            'AdditionalOptions!': ['-std:c++20'],
            'AdditionalOptions': ['/std:c++latest', '/utf-8', '/Zc:__cplusplus', '/bigobj'],
          },
        },
      }],
    ],
  },
}
