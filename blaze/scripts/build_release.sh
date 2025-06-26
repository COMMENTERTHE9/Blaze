#!/bin/bash
set -e

VERSION=$1
if [ -z "$VERSION" ]; then
    echo "Usage: ./build_release.sh v0.1.0"
    exit 1
fi

RELEASE_DIR=releases/$VERSION
mkdir -p $RELEASE_DIR

echo "Building Blaze $VERSION..."

# Linux (native)
echo "Building for Linux..."
make clean && make
mkdir -p blaze-$VERSION-linux-x86_64/{bin,examples,docs}
cp blaze blaze-$VERSION-linux-x86_64/bin/
cp examples/*.blaze blaze-$VERSION-linux-x86_64/examples/
cp README.md blaze-$VERSION-linux-x86_64/
cp LICENSE blaze-$VERSION-linux-x86_64/
cp -r docs/*.md blaze-$VERSION-linux-x86_64/docs/
tar -czf $RELEASE_DIR/blaze-$VERSION-linux-x86_64.tar.gz blaze-$VERSION-linux-x86_64
rm -rf blaze-$VERSION-linux-x86_64

# Windows (cross-compiled with MinGW)
echo "Building for Windows..."
echo "Note: Windows cross-compilation requires Docker or MinGW-w64"
echo "Skipping Windows build for now (use GitHub Actions for full cross-platform builds)"

# macOS
echo "Note: macOS build should be done on macOS hardware (use GitHub Actions)"

echo "Build complete. Check $RELEASE_DIR for archives."