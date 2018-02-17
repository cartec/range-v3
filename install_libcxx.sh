#!/usr/bin/env bash

set -e

TRUNK_VERSION=6
TASKS=2

while getopts j: OPT; do
    case "$OPT" in
    j) TASKS=$OPTARG;;
    ?) exit 1;;
    esac
done

if [[ $OPTIND -gt $#  ]]; then
    echo "Usage: $0 [-j N] VERSION [INSTALL_PREFIX]" 1>&2
    exit 1
fi

VERSION=${!OPTIND}
OPTIND=$(($OPTIND + 1))

PREFIX="${!OPTIND}"
PREFIX="${PREFIX:-/usr}"

case "$VERSION" in
    3.4) VERSION=3.4.2;;
    3.5) VERSION=3.5.2;;
    3.6) VERSION=3.6.2;;
    3.7) VERSION=3.7.1;;
    3.8) VERSION=3.8.1;;
    3 | 3.9) VERSION=3.9.1;;
    4 | 4.0) VERSION=4.0.1;;
    5 | 5.0) VERSION=5.0.1;;
esac

VERSION_ARRAY=(${VERSION//./ })
VERSION_MAJOR=${VERSION_ARRAY[0]}
CMAKE_ARGS=""

if [[ "$VERSION_MAJOR" -lt $TRUNK_VERSION ]]; then
    echo "Fetching libc++ and libc++abi version: ${VERSION}..."
    mkdir -p llvm-source/projects/libcxx llvm-source/projects/libcxxabi
    LLVM_URL="http://releases.llvm.org/${VERSION}/llvm-${VERSION}.src.tar.xz"
    LIBCXX_URL="http://releases.llvm.org/${VERSION}/libcxx-${VERSION}.src.tar.xz"
    LIBCXXABI_URL="http://releases.llvm.org/${VERSION}/libcxxabi-${VERSION}.src.tar.xz"
    curl -# $LIBCXXABI_URL | tar -xJf - -C llvm-source/projects/libcxxabi --strip-components=1 &
    JOBS=$!
    curl -# $LIBCXX_URL | tar -xJf - -C llvm-source/projects/libcxx --strip-components=1 &
    JOBS+=" $!"
    curl -# $LLVM_URL | tar -xJf - -C llvm-source --strip-components=1 &
    JOBS+=" $!"
else
    echo "Fetching libc++ and libc++abi tip-of-trunk..."

    # Checkout LLVM sources
    git clone --depth=1 https://github.com/llvm-mirror/llvm.git llvm-source
    git clone --depth=1 https://github.com/llvm-mirror/libcxx.git llvm-source/projects/libcxx &
    JOBS=$!
    git clone --depth=1 https://github.com/llvm-mirror/libcxxabi.git llvm-source/projects/libcxxabi &
    JOBS+=" $!"
fi

mkdir llvm-build
cd llvm-build
for pid in $JOBS; do wait -n; done

# - only ASAN is enabled for clang/libc++ versions < 4.x
if [[ $VERSION_MAJOR -lt 4 ]]; then
    if [[ $SANITIZER == "Address;Undefined" ]]; then
        ASAN_FLAGS="-fsanitize=address"
        CMAKE_ARGS+=" -DCMAKE_CXX_FLAGS=$ASAN_FLAGS -DCMAKE_EXE_LINKER_FLAGS=$ASAN_FLAGS"
    fi
else
    CMAKE_ARGS+=" -DLIBCXX_ABI_UNSTABLE=ON -DLLVM_USE_SANITIZER=$SANITIZER"
fi

cmake ../llvm-source -DCMAKE_C_COMPILER=${CC} -DCMAKE_CXX_COMPILER=${CXX} \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX="${PREFIX}" $CMAKE_ARGS
make cxx -j2 VERBOSE=1

# - libc++ versions < 4.x do not have the install-cxxabi and install-cxx targets
if [[ $VERSION_MAJOR -lt 4 ]]; then
    mkdir -p "${PREFIX}/lib/" "${PREFIX}/include/"
    sudo cp -r lib/* "${PREFIX}/lib/"
    sudo cp -r include/c++ "${PREFIX}/include/"
else
    sudo make install-cxxabi install-cxx
fi
