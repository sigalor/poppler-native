{
  "targets": [
    {
      "target_name": "poppler",
      "sources": [
        "native/binding/binding.cpp",
        "native/binding/NodeUtilities.cpp",
        "native/binding/PDFUtilities.cpp",
        "native/binding/ReadPDFOutputs.cpp",
        "native/binding/html/HtmlFonts.cc",
        "native/binding/html/HtmlLinks.cc",
        "native/binding/html/HtmlOutputDev.cc",
        "native/binding/html/InMemoryFile.cc"
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "native/poppler"
      ],
      "dependencies": [
        "<!(node -p \"require('node-addon-api').gyp\")",
        "libpoppler"
      ],
      "cflags_cc!": [
        "-fno-exceptions",
        "-fno-rtti"
      ],
      "cflags_cc": [
        "-std=c++14"
      ],
      "defines": [
        "NAPI_CPP_EXCEPTIONS"
      ],
      "libraries": [
        "-ljpeg",
        "-lpng"
      ]
    },
    {
      "target_name": "libpoppler",
      "type": "static_library",
      "cflags_cc": [],
      "include_dirs": [
        "native/poppler",
        "native/poppler/goo",
        "native/poppler/poppler",
        "native/freetype/include/freetype2",
        "native/openjpeg/include/openjpeg-2.3"
      ],
      "cflags_cc!": [
        "-fno-rtti"
      ],
      "sources": [
        "native/poppler/fofi/FoFiBase.cc",
        "native/poppler/fofi/FoFiEncodings.cc",
        "native/poppler/fofi/FoFiIdentifier.cc",
        "native/poppler/fofi/FoFiTrueType.cc",
        "native/poppler/fofi/FoFiType1.cc",
        "native/poppler/fofi/FoFiType1C.cc",
        "native/poppler/goo/gbase64.cc",
        "native/poppler/goo/gbasename.cc",
        "native/poppler/goo/gfile.cc",
        "native/poppler/goo/glibc_strtok_r.cc",
        "native/poppler/goo/glibc.cc",
        "native/poppler/goo/GooString.cc",
        "native/poppler/goo/GooTimer.cc",
        "native/poppler/goo/grandom.cc",
        "native/poppler/goo/gstrtod.cc",
        "native/poppler/goo/ImgWriter.cc",
        "native/poppler/goo/JpegWriter.cc",
        "native/poppler/goo/NetPBMWriter.cc",
        "native/poppler/goo/PNGWriter.cc",
        "native/poppler/goo/TiffWriter.cc",
        "native/poppler/poppler/Annot.cc",
        "native/poppler/poppler/Array.cc",
        "native/poppler/poppler/BBoxOutputDev.cc",
        "native/poppler/poppler/CachedFile.cc",
        "native/poppler/poppler/Catalog.cc",
        "native/poppler/poppler/CertificateInfo.cc",
        "native/poppler/poppler/CharCodeToUnicode.cc",
        "native/poppler/poppler/CMap.cc",
        "native/poppler/poppler/CurlCachedFile.cc",
        "native/poppler/poppler/CurlPDFDocBuilder.cc",
        "native/poppler/poppler/DateInfo.cc",
        "native/poppler/poppler/DCTStream.cc",
        "native/poppler/poppler/Decrypt.cc",
        "native/poppler/poppler/Dict.cc",
        "native/poppler/poppler/Error.cc",
        "native/poppler/poppler/FileSpec.cc",
        "native/poppler/poppler/FlateEncoder.cc",
        "native/poppler/poppler/FlateStream.cc",
        "native/poppler/poppler/FontEncodingTables.cc",
        "native/poppler/poppler/FontInfo.cc",
        "native/poppler/poppler/Form.cc",
        "native/poppler/poppler/Function.cc",
        "native/poppler/poppler/Gfx.cc",
        "native/poppler/poppler/GfxFont.cc",
        "native/poppler/poppler/GfxState.cc",
        "native/poppler/poppler/GlobalParams.cc",
        "native/poppler/poppler/Hints.cc",
        "native/poppler/poppler/JArithmeticDecoder.cc",
        "native/poppler/poppler/JBIG2Stream.cc",
        "native/poppler/poppler/JPXStream.cc",
        "native/poppler/poppler/JSInfo.cc",
        "native/poppler/poppler/Lexer.cc",
        "native/poppler/poppler/Linearization.cc",
        "native/poppler/poppler/Link.cc",
        "native/poppler/poppler/LocalPDFDocBuilder.cc",
        "native/poppler/poppler/MarkedContentOutputDev.cc",
        "native/poppler/poppler/Movie.cc",
        "native/poppler/poppler/NameToCharCode.cc",
        "native/poppler/poppler/Object.cc",
        "native/poppler/poppler/OptionalContent.cc",
        "native/poppler/poppler/Outline.cc",
        "native/poppler/poppler/OutputDev.cc",
        "native/poppler/poppler/Page.cc",
        "native/poppler/poppler/PageLabelInfo.cc",
        "native/poppler/poppler/PageTransition.cc",
        "native/poppler/poppler/Parser.cc",
        "native/poppler/poppler/PDFDoc.cc",
        "native/poppler/poppler/PDFDocBuilder.cc",
        "native/poppler/poppler/PDFDocEncoding.cc",
        "native/poppler/poppler/PDFDocFactory.cc",
        "native/poppler/poppler/PreScanOutputDev.cc",
        "native/poppler/poppler/ProfileData.cc",
        "native/poppler/poppler/PSOutputDev.cc",
        "native/poppler/poppler/PSTokenizer.cc",
        "native/poppler/poppler/Rendition.cc",
        "native/poppler/poppler/SecurityHandler.cc",
        "native/poppler/poppler/SignatureInfo.cc",
        "native/poppler/poppler/Sound.cc",
        "native/poppler/poppler/StdinCachedFile.cc",
        "native/poppler/poppler/StdinPDFDocBuilder.cc",
        "native/poppler/poppler/Stream.cc",
        "native/poppler/poppler/StructElement.cc",
        "native/poppler/poppler/StructTreeRoot.cc",
        "native/poppler/poppler/TextOutputDev.cc",
        "native/poppler/poppler/UnicodeMap.cc",
        "native/poppler/poppler/UnicodeMapFuncs.cc",
        "native/poppler/poppler/UnicodeTypeTable.cc",
        "native/poppler/poppler/UTF.cc",
        "native/poppler/poppler/ViewerPreferences.cc",
        "native/poppler/poppler/XRef.cc"
      ]
    }
  ]
}
