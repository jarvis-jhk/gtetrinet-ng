# Third-party components

- GTetrinet (1999–2015): original authors listed in AUTHORS; GPL-2.0-or-later, COPYING.
  Full source/history: https://gitlab.gnome.org/Archive/gtetrinet (retained in legacy/).
- font8x8: public-domain glyph data, notices preserved in third_party/font8x8_basic.h and
  font8x8_ext_latin.h (Latin-1 glyphs for German; declared static const here).
  https://github.com/dhepper/font8x8
- SDL2: zlib licence, included in binary distribution. Source/releases:
  https://github.com/libsdl-org/SDL/releases ; Linux package source:
  https://packages.ubuntu.com/jammy/libsdl2-2.0-0
- libsamplerate: BSD licence included in Linux archive; source
  https://github.com/libsndfile/libsamplerate and Ubuntu source package libsamplerate.
- Windows libiconv/libcharset: LGPL-2.1-or-later libraries, dynamically linked and
  replaceable. Matching MSYS2 source archive and licence texts accompany the DLLs.
  https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-libiconv
- Windows GCC runtime libraries: GPL with GCC Runtime Library Exception;
  https://gcc.gnu.org/onlinedocs/libstdc++/manual/license.html and
  https://www.msys2.org/ provide licences, source packages and toolchain build recipes.
  MinGW-w64 winpthreads: MIT-style licence, source https://github.com/mingw-w64/mingw-w64.
- Linux packaging/Steam shortcut helpers adapted from s25coop (GPL-2.0-or-later),
  originally based on ArnoldSmith86/minecraft-splitscreen (MIT). Attribution retained.
