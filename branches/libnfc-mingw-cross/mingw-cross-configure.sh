#!/bin/sh

MINGW_DIR="/usr/i586-mingw32msvc"

# Use MinGW binaries before others
#export PATH=$MINGW_DIR/bin:$PATH

# Set CPATH to MinGW include files
export CPATH=$MINGW_DIR/include
export LD_LIBRARY_PATH=$MINGW_DIR/lib
export LD_RUN_PATH=$MINGW_DIR/lib

# Force pkg-config to search in cross environement directory
export PKG_CONFIG_LIBDIR=$MINGW_DIR/lib/pkgconfig

# Stop compilation on first error
export CFLAGS="-Wfatal-errors"

# Include default MinGW include directory, and libnfc's win32 files
export CFLAGS="$CFLAGS -I$MINGW_DIR/include -I$PWD/contrib/win32"

# Configure to cross-compile using mingw32msvc
./configure --target=i586-mingw32msvc --host=i586-mingw32msvc --with-drivers=pn532_uart,arygon $*
