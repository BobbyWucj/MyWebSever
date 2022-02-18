#!/bin/sh

set -x

SOURCE_DIR=`pwd`
BUILD_DIR=${BUILD_DIR:-./TinyWebServer-build}
BUILD_TYPE=${BUILD_TYPE:-Release}
INSTALL_DIR=${INSTALL_DIR:-./${BUILD_TYPE}-install}

mkdir -p $BUILD_DIR/$BUILD_TYPE \
    && cd $BUILD_DIR/$BUILD_TYPE \
    && cmake \
        -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
        -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR \
        $SOURCE_DIR \
    && make $*