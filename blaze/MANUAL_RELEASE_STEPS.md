# Manual Release Steps for Blaze

Since GitHub Actions might not be enabled, here's how to create a release manually:

## Option 1: Using GitHub Web Interface

1. Go to https://github.com/COMMENTERTHE9/Blaze/releases
2. Click "Create a new release" 
3. Choose "Create new tag" and enter `v0.1.0`
4. Set release title: "Blaze v0.1.0"
5. Copy the content from RELEASE_NOTES.md into the description
6. Upload the file from `releases/v0.1.0/blaze-v0.1.0-linux-x86_64.tar.gz`
7. Click "Publish release"

## Option 2: Using GitHub CLI (if installed)

```bash
# Install gh if needed: https://cli.github.com/

# Create the release
gh release create v0.1.0 \
  --title "Blaze v0.1.0" \
  --notes-file RELEASE_NOTES.md \
  releases/v0.1.0/blaze-v0.1.0-linux-x86_64.tar.gz
```

## Option 3: Enable GitHub Actions

1. Go to https://github.com/COMMENTERTHE9/Blaze/settings/actions
2. Make sure Actions is enabled
3. Then push a tag:
   ```bash
   git tag v0.1.0
   git push --tags
   ```

## What You'll Get

Like Sapphire, you'll have:
- **Source code (zip)** - Automatically created by GitHub
- **Source code (tar.gz)** - Automatically created by GitHub  
- **blaze-v0.1.0-linux-x86_64.tar.gz** - Your compiled binary
- (Future: Windows and macOS binaries when Actions work)