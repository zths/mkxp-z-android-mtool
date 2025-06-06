#!/bin/bash

# This script downloads/git clones project dependencies
# such as libogg, SDL2, Ruby, etc.

GIT_ARGS="-q -c advice.detachedHead=false --single-branch --depth 1"

# Xiph libogg
if [[ ! -d "libogg" ]]; then
  echo "Downloading libogg..."
  git clone $GIT_ARGS -b v1.3.5 https://github.com/xiph/ogg libogg
fi

# Xiph libvorbis
if [[ ! -d "libvorbis" ]]; then
  echo "Downloading libvorbis..."
  git clone $GIT_ARGS -b v1.3.7 https://github.com/xiph/vorbis libvorbis
fi

# Xiph libtheora
if [[ ! -d "libtheora" ]]; then
  echo "Downloading libtheora..."
  wget -q https://ftp.osuosl.org/pub/xiph/releases/theora/libtheora-1.1.1.tar.gz
  tar -xzf libtheora-1.1.1.tar.gz
  mv libtheora-1.1.1 libtheora
  rm -f libtheora-1.1.1.tar.gz
fi

# GNU libiconv
if [[ ! -d "libiconv" ]]; then
  echo "Downloading libiconv..."
  wget -q https://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.17.tar.gz
  tar -xzf libiconv-1.17.tar.gz
  mv libiconv-1.17 libiconv
  rm -f libiconv-1.17.tar.gz
fi

# Freedesktop uchardet
if [[ ! -d "uchardet" ]]; then
  echo "Downloading uchardet..."
  git clone $GIT_ARGS -b v0.0.8 https://gitlab.freedesktop.org/uchardet/uchardet uchardet
fi

# Freedesktop Pixman
if [[ ! -d "pixman" ]]; then
  echo "Downloading Pixman..."
  git clone $GIT_ARGS -b pixman-0.42.2 https://gitlab.freedesktop.org/pixman/pixman pixman
fi

# PhysicsFS
if [[ ! -d "physfs" ]]; then
  echo "Downloading PhysicsFS..."
  git clone $GIT_ARGS -b release-3.2.0 https://github.com/icculus/physfs physfs
fi

# OpenAL Soft 1.23.0
if [[ ! -d "openal" ]]; then
  echo "Downloading OpenAL Soft..."
  git clone $GIT_ARGS -b 1.23.0 https://github.com/kcat/openal-soft openal
fi

# SDL2
if [[ ! -d "SDL2" ]]; then
  echo "Downloading SDL2..."
  git clone $GIT_ARGS -b release-2.28.1 https://github.com/libsdl-org/SDL SDL2
fi

# SDL2_image
if [[ ! -d "SDL2_image" ]]; then
  echo "Downloading SDL2_image..."
  git clone $GIT_ARGS --recurse-submodules -b release-2.6.3 https://github.com/libsdl-org/SDL_image SDL2_image
fi

# SDL2_ttf
if [[ ! -d "SDL2_ttf" ]]; then
  echo "Downloading SDL2_ttf..."
  git clone $GIT_ARGS --recurse-submodules -b release-2.20.2 https://github.com/libsdl-org/SDL_ttf SDL2_ttf
fi

# SDL2_sound
if [[ ! -d "SDL2_sound" ]]; then
  echo "Downloading SDL2_sound..."
  git clone $GIT_ARGS -b v2.0.1 https://github.com/icculus/SDL_sound SDL2_sound
fi

# OpenSSL 1.1.1t
if [[ ! -d "openssl" ]]; then
  echo "Downloading OpenSSL..."
  git clone $GIT_ARGS -b openssl-3.0.12 https://github.com/openssl/openssl openssl
fi

# Ruby 3.1.3 (patched for mkxp-z)
if [[ ! -d "ruby" ]]; then
  echo "Downloading Ruby 3.1.3 (patched for mkxp-z)..."
  git clone $GIT_ARGS -b mkxp-z-3.1.3 https://github.com/mkxp-z/ruby ruby
fi

# Ruby 1.8.7
if [[ ! -d "ruby_187" ]]; then
  echo "Downloading Ruby 1.8.7..."
  git clone $GIT_ARGS -b ruby_1_8_7 https://github.com/mkxp-z/ruby ruby187
fi
cd ruby187
patch -p1 < ../patches/ruby187-remove-inline.patch

# Ruby 1.9.3
if [[ ! -d "ruby_193" ]]; then
  echo "Downloading Ruby 1.9.3..."
  git clone $GIT_ARGS -b ruby_1_9_3 https://github.com/ruby/ruby ruby193
  #wget https://cache.ruby-lang.org/pub/ruby/1.9/ruby-1.9.3-p551.tar.gz -O ruby-1.9.3-p551.tar.gz
  #tar -xzf ruby-1.9.3-p551.tar.gz
  #mv ruby-1.9.3-p551 ruby193
fi

cd ruby193
patch -p1 < ../patches/ruby193files/GH-488.patch
patch -p1 < ../patches/ruby193files/CVE-2015-1855-p484.patch
patch -p1 < ../patches/ruby193files/update-autoconf.patch
#patch -p1 < ../patches/ruby193files/opensslv11x.patch
#patch -p1 < ../patches/ruby193files/opensslv30.patch
patch -p1 < ../patches/ruby193files/openssl3.patch
patch -p1 < ../patches/ruby193files/ruby193-dir-seekdir-fix.patch
cd ..
cp patches/ruby193files/parse.h ruby193/parse.h
cp patches/ruby193files/parse.c ruby193/parse.c
cp patches/ruby193files/ripper.c ruby193/ext/ripper/ripper.c
cp patches/ruby193files/zlib.c ruby193/zlib.c
cp patches/ruby193files/inits.c ruby193/inits.c
cp patches/ruby193files/common.mk ruby193/common.mk
cp patches/ruby193files/uncommon.mk ruby193/uncommon.mk
rm -rf ruby193/ext/zlib
rm -f ruby193/ext/ripper/ripper.y
echo "Done!"
