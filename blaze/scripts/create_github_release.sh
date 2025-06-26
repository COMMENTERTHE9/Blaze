#!/bin/bash
# Simple script to create a GitHub release with the GitHub CLI

VERSION=$1
if [ -z "$VERSION" ]; then
    echo "Usage: ./create_github_release.sh v0.1.0"
    exit 1
fi

echo "Creating GitHub release for $VERSION..."

# Create release with gh CLI
gh release create $VERSION \
    --title "Blaze $VERSION" \
    --notes-file RELEASE_NOTES.md \
    --draft

echo "Draft release created! Upload binaries with:"
echo "gh release upload $VERSION releases/$VERSION/*.tar.gz"