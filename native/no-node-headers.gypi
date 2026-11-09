# Included by the targets of Poppler and the vendored C libraries. They are plain C/C++ libraries and must not
# see Node's headers, which node-gyp puts on every target's include path: Node's include directory holds its own
# zlib.h (Chromium's zlib), which would otherwise shadow the vendored one in libpng and Poppler.
{
  'include_dirs!': [
    '<(node_root_dir)/include/node',
    '<(node_root_dir)/src',
    '<(node_root_dir)/deps/openssl/config',
    '<(node_root_dir)/deps/openssl/openssl/include',
    '<(node_root_dir)/deps/uv/include',
    '<(node_root_dir)/deps/zlib',
    '<(node_root_dir)/<(node_engine_include_dir)',
  ],
}
