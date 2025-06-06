ARCH     := linux-x86_64
NDK       := $(ANDROID_NDK_HOME)
TOOLCHAIN := $(NDK)/toolchains/llvm/prebuilt/$(ARCH)
NPROC     := $(shell nproc)

# Check if NDK path is set
#ifeq ($(NDK),)
#$(error ANDROID_NDK_HOME is not set. Please set it to your NDK installation path)
#endif

# Build directory
BUILD_ROOT := $(PWD)
BUILD_PREFIX := $(BUILD_ROOT)/build-$(ABI)

# Android platform API version to build:
# (Default: 21 - Android 5.0 Lollipop)
# See https://apilevels.com/ for Android API versions.
# Note: If you building for 64-bit, you should target 21 and higher.
API ?= 21

# Android CPU specific variables to build:
# (Default: ARMv7-A 32-bit)
# For ARMv7-A 32-bit:
# - HOST:   armv7a-linux-androideabi
# - TARGET: arm-linux-androideabi
# - ABI:    armeabi-v7a
# For ARMv8-A 64-bit:
# - HOST:   aarch64-linux-android
# - TARGET: aarch64-linux-android
# - ABI:    arm64-v8a
# For x86 32-bit:
# - HOST:   i686-linux-android
# - TARGET: i686-linux-android
# - ABI:    x86
# For x86_64 64-bit:
# - HOST:   x86_64-linux-android
# - TARGET: x86_64-linux-android
# - ABI:    x86_64
HOST   ?= aarch64-linux-android
TARGET ?= aarch64-linux-android
ABI    ?= arm64-v8a

# Define directories
PREFIX            := $(HOST)$(API)
BUILD_PREFIX      := ${PWD}/build-$(ABI)
INCDIR            := $(BUILD_PREFIX)/include
LIBDIR            := $(BUILD_PREFIX)/lib
PKG_CONFIG_LIBDIR := $(BUILD_PREFIX)/lib/pkgconfig

ifneq (, ${MSYSTEM})
	RUBY_PREFIX := $(shell cygpath -m ${BUILD_PREFIX})
else ifneq (, $(findstring /cygdrive/,$(PATH)))
	RUBY_PREFIX := $(shell cygpath -m ${BUILD_PREFIX})
else
	RUBY_PREFIX := $(BUILD_PREFIX)
endif

# Toolchain tools
CC      := $(TOOLCHAIN)/bin/$(PREFIX)-clang
CXX     := $(TOOLCHAIN)/bin/$(PREFIX)-clang++
AS      := $(CC)
LD      := $(TOOLCHAIN)/bin/ld
AR      := $(TOOLCHAIN)/bin/llvm-ar
NM      := $(TOOLCHAIN)/bin/llvm-nm
OBJCOPY := $(TOOLCHAIN)/bin/llvm-objcopy
OBJDUMP := $(TOOLCHAIN)/bin/llvm-objdump
RANLIB  := $(TOOLCHAIN)/bin/llvm-ranlib
STRIP   := $(TOOLCHAIN)/bin/llvm-strip

# OpenSSL configure has own targets for Android building
ifeq ($(ABI), armeabi-v7a)
	OPENSSL_TARGET := android-arm
else ifeq ($(ABI), arm64-v8a)
	OPENSSL_TARGET := android-arm64
else ifeq ($(ABI), x86)
	OPENSSL_TARGET := android-x86
else ifeq ($(ABI), x86_64)
	OPENSSL_TARGET := android-x86_64
endif

# Some flags for ARM arch
ifeq ($(ABI), armeabi-v7a)
	CROSSFLAGS := -D__ARM_ARCH_7A__ -D__arm__ -march=armv7-a -mtune=cortex-a7 -mfpu=neon -mfloat-abi=softfp
else ifeq ($(ABI), arm64-v8a)
	CROSSFLAGS := -D__ARM_ARCH_8A__ -D__aarch64__ -march=armv8-a -mtune=cortex-a53
endif

# Compiler/linker flags
CFLAGS      := -O0 $(CROSSFLAGS) --sysroot=$(TOOLCHAIN)/sysroot -I$(TOOLCHAIN)/sysroot/usr/include -I$(INCDIR)
LDFLAGS     := -L$(LIBDIR) -lm -lz
CLANG_FLAGS := -target $(TARGET) --gcc-toolchain $(TOOLCHAIN)

# CMake arguments
CMAKE_ARGS := \
	-DCMAKE_TOOLCHAIN_FILE="$(NDK)/build/cmake/android.toolchain.cmake" \
	-DCMAKE_SYSTEM_NAME=Android \
	-DCMAKE_SYSTEM_VERSION=$(API) \
	-DANDROID_PLATFORM=$(API) \
	-DANDROID_ABI=$(ABI) \
	-DCMAKE_ANDROID_ARCH_ABI=$(ABI) \
	-DCMAKE_ANDROID_STL_TYPE=c++_shared \
	-DCMAKE_INSTALL_PREFIX="$(BUILD_PREFIX)" \
	-DCMAKE_PREFIX_PATH="$(BUILD_PREFIX)" \
	-DCMAKE_BUILD_TYPE=RelWithDebInfo \
	-DCMAKE_C_FLAGS="$(CFLAGS)" \
	-G "Unix Makefiles"

# RVM Ruby version management - use bash explicitly
SHELL := /bin/bash
RVM_SCRIPT := source /usr/share/rvm/scripts/rvm
RUBY_187_ENV := $(RVM_SCRIPT) && rvm use 1.8.7
RUBY_193_ENV := $(RVM_SCRIPT) && rvm use 1.9.3 && \
	export CC="$(CC)" && \
	export CFLAGS="$(CFLAGS)" && \
	export LDFLAGS="$(LDFLAGS)" && \
	export LDSHARED="$(CC) -shared"
RUBY_31_ENV := $(RVM_SCRIPT) && rvm use 3.1.3

# Host configurations for different Ruby versions
# Ruby 1.8.7 and 1.9.3 don't support Android, use eabi instead
ifeq ($(ABI), armeabi-v7a)
	HOST_187 := arm-linux-eabi
	HOST_193 := arm-linux-eabi
else ifeq ($(ABI), arm64-v8a)
	HOST_187 := arm-linux-eabi
	HOST_193 := arm-linux-eabi
else ifeq ($(ABI), x86)
	HOST_187 := i686-linux-gnu
	HOST_193 := i686-linux-gnu
else ifeq ($(ABI), x86_64)
	HOST_187 := x86_64-linux-gnu
	HOST_193 := x86_64-linux-gnu
endif

# Autoconf environment variables
CONFIGURE_ENV := \
	CC="$(CC)" CXX="$(CXX)" AS="$(AS)" LD="$(LD)" AR="$(AR)" NM="$(NM)" \
	RANLIB="$(RANLIB)" STRIP="$(STRIP)" OBJCOPY="$(OBJCOPY)" OBJDUMP="$(OBJDUMP)" \
	CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" CLANG_FLAGS="$(CLANG_FLAGS)" \
	PKG_CONFIG_LIBDIR="$(PKG_CONFIG_LIBDIR)"

# Autoconf arguments
CONFIGURE_ARGS := --host=$(HOST) --prefix="$(BUILD_PREFIX)"

# Only Android 8.0 Oreo and higher can have some POSIX functions like:
# getpwent(), setpwent(), endpwent(), nl_langinfo(), and nl_langinfo_l()
ifeq ($(shell test $(API) -ge 26; echo $$?), 1)
	CONFIGURE_ARGS += ac_cv_header_pwd_h=no ac_cv_header_langinfo_h=no
endif

# OpenAL CMake arguments to build
OPENAL_CONFIGURE_ARGS := \
	-DALSOFT_REQUIRE_OPENSL=ON \
	-DALSOFT_EXAMPLES=OFF \
	-DALSOFT_UTILS=OFF

# Ruby configure arguments to build
RUBY_CONFIGURE_ARGS := \
	--prefix="$(RUBY_PREFIX)" \
	--enable-shared \
	--enable-static \
	--enable-install-static-library \
	--disable-install-doc \
	--disable-rubygems \
	--with-static-linked-ext \
	--with-out-ext=readline,dbm,gdbm,win32,win32ole,fiddle \
	--without-gmp

CONFIGURE := $(CONFIGURE_ENV) ./configure $(CONFIGURE_ARGS)
CMAKE     := $(CONFIGURE_ENV) cmake .. $(CMAKE_ARGS)


.PHONY: all init clean iconv openal openssl ruby ruby187 ruby193 clean-iconv clean-openal clean-openssl clean-ruby clean-ruby187 clean-ruby193 bison clean-bison

default: all


# -------------------- Bison 2.x --------------------
bison: init $(BUILD_PREFIX)/bin/bison

$(BUILD_PREFIX)/bin/bison: bison-2.7.1/Makefile
	cd bison-2.7.1; make -j$(NPROC); make install

bison-2.7.1/Makefile: bison-2.7.1/configure
	cd bison-2.7.1; ./configure --prefix="$(BUILD_PREFIX)" --disable-nls

# -------------------- iconv --------------------
iconv: init $(LIBDIR)/libiconv.a

$(LIBDIR)/libiconv.a: libiconv/Makefile
	cd libiconv; make -j$(NPROC); make install

libiconv/Makefile: libiconv/configure
	cd libiconv; $(CONFIGURE) --enable-static=true --enable-shared=false

# -------------------- OpenAL --------------------
openal: init $(LIBDIR)/libopenal.so

$(LIBDIR)/libopenal.so: openal/cmakebuild/Makefile
	cd openal/cmakebuild; make -j$(NPROC); make install

openal/cmakebuild/Makefile: openal/CMakeLists.txt
	cd openal; mkdir cmakebuild; cd cmakebuild; \
	$(CMAKE) -DLIBTYPE=SHARED $(OPENAL_CONFIGURE_ARGS)

# -------------------- OpenSSL --------------------
openssl: init $(LIBDIR)/libssl.a

$(LIBDIR)/libssl.a: openssl/Makefile
	cd openssl; PATH="$(TOOLCHAIN)/bin:$$PATH" \
	CC="$(PREFIX)-clang" CXX="$(PREFIX)-clang++" \
	AR="llvm-ar" RANLIB="llvm-ranlib" STRIP="llvm-strip" \
	make -j$(NPROC); \
	PATH="$(TOOLCHAIN)/bin:$$PATH" \
	CC="$(PREFIX)-clang" CXX="$(PREFIX)-clang++" \
	AR="llvm-ar" RANLIB="llvm-ranlib" STRIP="llvm-strip" \
	make install_sw

openssl/Makefile: openssl/Configure
	cd openssl; PATH="$(TOOLCHAIN)/bin:$$PATH" \
	CC="$(PREFIX)-clang" CXX="$(PREFIX)-clang++" \
	AR="llvm-ar" RANLIB="llvm-ranlib" STRIP="llvm-strip" \
	CFLAGS="-Wno-macro-redefined" ./Configure $(OPENSSL_TARGET) \
	no-shared -D__ANDROID_API__=$(API) \
	--prefix="$(BUILD_PREFIX)" --openssldir="$(BUILD_PREFIX)"

# -------------------- Ruby --------------------
ruby: init openssl $(LIBDIR)/libruby.so.3.1.0

$(LIBDIR)/libruby.so.3.1.0: ruby/Makefile
	cd ruby; $(RUBY_31_ENV) && make -j$(NPROC); $(RUBY_31_ENV) && make install
	cd $(LIBDIR); ln -sf libruby.so.3.1.3 libruby313.so


ruby/Makefile: ruby/configure
	cd ruby; DLDFLAGS="-Wl,-soname,libruby.so" \
	$(CONFIGURE) $(RUBY_CONFIGURE_ARGS)

ruby/configure: ruby/configure.ac
	cd ruby; $(RUBY_31_ENV) && autoreconf -i

# -------------------- Ruby 1.8.7 --------------------
ruby187: init openssl $(LIBDIR)/libruby187.so

$(LIBDIR)/libruby187.so: ruby187/Makefile
	cd ruby187; $(RUBY_187_ENV) && make -j$(NPROC); $(RUBY_187_ENV) && make install
	cd $(LIBDIR); ln -sf libruby.so.1.8.7 libruby187.so

ruby187/Makefile: ruby187/configure
	cd ruby187; DLDFLAGS="-Wl,-soname,libruby187.so" \
	CC="$(CC)" CXX="$(CXX)" AS="$(AS)" LD="$(LD)" AR="$(AR)" NM="$(NM)" \
	RANLIB="$(RANLIB)" STRIP="$(STRIP)" OBJCOPY="$(OBJCOPY)" OBJDUMP="$(OBJDUMP)" \
	CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" CLANG_FLAGS="$(CLANG_FLAGS)" \
	PKG_CONFIG_LIBDIR="$(PKG_CONFIG_LIBDIR)" \
	LDSHARED="$(CC) -shared" \
	./configure --host=$(HOST_187) --prefix="$(BUILD_PREFIX)" \
	--includedir="$(BUILD_PREFIX)/include/ruby-1.8.7" \
	ac_cv_header_pwd_h=no ac_cv_header_langinfo_h=no \
	$(RUBY_CONFIGURE_ARGS) --program-suffix=187

ruby187/configure: ruby187/configure.in
	cd ruby187; $(RUBY_187_ENV) && autoreconf -i

# -------------------- Ruby 1.9.3 --------------------
#export CFLAGS="-O2 -pipe -U_FORTIFY_SOURCE"
#export CPPFLAGS="$CFLAGS"
#export CXXFLAGS="$CFLAGS"
#rvm install 1.9.3
ruby193: init openssl $(LIBDIR)/libruby193.so

$(LIBDIR)/libruby193.so: ruby193/Makefile
	cd ruby193; $(RUBY_193_ENV) && make -j$(NPROC); $(RUBY_193_ENV) && make install
	cd $(LIBDIR); ln -sf libruby.so.1.9.1 libruby193.so

ruby193/Makefile: ruby193/configure
	cd ruby193; DLDFLAGS="-Wl,-soname,libruby193.so" \
	CC="$(CC)" CXX="$(CXX)" AS="$(AS)" LD="$(LD)" AR="$(AR)" NM="$(NM)" \
	RANLIB="$(RANLIB)" STRIP="$(STRIP)" OBJCOPY="$(OBJCOPY)" OBJDUMP="$(OBJDUMP)" \
	CFLAGS="$(CFLAGS) -U_FORTIFY_SOURCE" LDFLAGS="$(LDFLAGS)" CLANG_FLAGS="$(CLANG_FLAGS)" \
	PKG_CONFIG_LIBDIR="$(PKG_CONFIG_LIBDIR)" \
	LDSHARED="$(CC) -shared" \
	LIBS="-lz -lm -ldl" \
	./configure --host=$(HOST_193) --prefix="$(BUILD_PREFIX)" \
	--includedir="$(BUILD_PREFIX)/include/ruby-1.9.3" \
	--with-baseruby=ruby \
	ac_cv_header_pwd_h=no ac_cv_header_langinfo_h=no \
	$(RUBY_CONFIGURE_ARGS) --program-suffix=193

ruby193/configure: ruby193/configure.in
	cd ruby193; $(RUBY_193_ENV) && autoreconf -i


init:
	@mkdir -p $(LIBDIR) $(INCDIR)

clean-iconv:
	-cd libiconv; make clean; rm Makefile

clean-openal:
	-cd openal; rm -rf cmakebuild

clean-openssl:
	-cd openssl; make clean; rm Makefile

clean-ruby:
	-cd ruby; make clean; rm Makefile; rm configure

clean-ruby187:
	-cd ruby187; make clean; rm Makefile; rm configure

clean-ruby193:
	-cd ruby193; make clean; rm Makefile; rm configure

clean-bison:
	-cd bison-2.7.1; make clean


clean: clean-iconv clean-openal clean-openssl clean-ruby clean-ruby187 clean-ruby193 clean-bison

all: iconv openal openssl ruby ruby187 ruby193 bison
