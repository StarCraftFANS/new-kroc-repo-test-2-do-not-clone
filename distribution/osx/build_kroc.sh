PKG_CONFIG=pkg-config-0.23
SDL=SDL-1.2.13
PLAYER=player-2.1.2
STAGE=stage-2.1.1
# Arduino package could perhaps be replaced with a avr-gcc package?
# FIXME: 0017 is out, perhaps upgrade
ARDUINO=arduino-0016

#FIXME: Possibly do SDL_sound
#http://icculus.org/SDL_sound/


INSTALL=$PWD/install
BUILD=$PWD/build

PATH=$PATH:$PWD/install/bin
export PATH


which -s pkg-config
if [ "$?" == "1" ] ; then
  mkdir -p build
  cd build
  curl -O http://pkgconfig.freedesktop.org/releases/$PKG_CONFIG.tar.gz || exit 1
  tar -xvzf $PKG_CONFIG.tar.gz
  cd $PKG_CONFIG
  ./configure --prefix=$INSTALL
  make install
  cd ../..
fi

which -s sdl-config
if [ "$?" == "1" ] ; then
  mkdir -p build
  cd build
  curl -O http://www.libsdl.org/release/$SDL.tar.gz || exit 1
  tar -xvzf $SDL.tar.gz
  cd $SDL
  # Compiling with newer xquartz (xquartz.macosforge.org)
  # does not work w/o this patch
  patch -p0 < ../../patch-SDL_x11gl_c.h.diff
  ./configure --prefix=$INSTALL
  make install
  cd ../..
fi

pkg-config playerc
if [ "$?" == "1" ] ; then
  mkdir -p build
  cd build
  curl -L -O \
    http://prdownloads.sourceforge.net/playerstage/$PLAYER.tar.bz2 \
    || exit 1
  tar -xvjf $PLAYER.tar.bz2
  cd $PLAYER
  ./configure --prefix=$INSTALL --disable-jplayer
  make install
  cd ../..
fi

#pkg-config stage
#if [ "$?" == "1" ] ; then
#  mkdir -p build
#  cd build
#  curl -L -O \
#    http://prdownloads.sourceforge.net/playerstage/$STAGE.tar.bz2 \
#    || exit 1
#  tar -xvjf $STAGE.tar.bz2
#  cd $STAGE
#  ./configure --prefix=$INSTALL
#  make install
#  cd ../..
#fi

if ! [ -d $BUILD/$ARDUINO ] ; then
  mkdir -p build
  cd build
  curl -O \
    http://arduino.googlecode.com/files/$ARDUINO-mac.zip \
    || exit 1
  unzip $ARDUINO-mac.zip
  cd $ARDUINO
fi


cd $BUILD
cd ../../../

autoreconf -v -f -i

cd $BUILD
mkdir kroc-ccsp
cd kroc-ccsp

../../../../configure --with-toolchain=kroc --prefix=$INSTALL
make
make install

cd $BUILD
mkdir kroc-tvm-posix
cd kroc-tvm-posix

../../../../configure --with-toolchain=tvm --prefix=$INSTALL
make
make install

cd $BUILD
mkdir kroc-tvm-posix-ppc
cd kroc-tvm-posix-ppc
CFLAGS="-isysroot /Developer/SDKs/MacOSX10.4u.sdk \
  -I/usr/lib/gcc/powerpc-apple-darwin9/4.2.1/include/ \
  -mmacosx-version-min=10.4" \
  CC=powerpc-apple-darwin9-gcc-4.2.1 \
  ../../../../configure --host=powerpc-apple-darwin9 \
  --build=powerpc --with-toolchain=tvm \
  --prefix=$INSTALL-tvm-ppc
# FIXME: Both of these fail at the moment, though they get far enough to be
# useful, which incidentally is up to, but not including, the useful library.
make
make install

cd $BUILD
mkdir kroc-tvm-avr
cd kroc-tvm-avr

../../../../configure --target=avr --with-toolchain=tvm --prefix=$INSTALL-avr
make
make install
cd $BUILD/kroc-tvm-avr/modules/inmoslibs/libsrc
make
make install


OLD_PATH=$PATH
PATH=$PATH:$BUILD/$ARDUINO/hardware/tools/avr/bin
cd $BUILD
cd ../../../tvm/arduino
unset ACLOCAL
autoreconf -v -f -i
cd $BUILD
mkdir -p kroc-tvm-avr-wrapper
cd kroc-tvm-avr-wrapper
../../../../tvm/arduino/configure --host=avr --with-toolchain=tvm --prefix=$INSTALL --libdir=${INSTALL}/lib/avr
make
make tvm-arduino.hex
PATH=$OLD_PATH
