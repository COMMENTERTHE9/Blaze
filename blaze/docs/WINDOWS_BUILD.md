# Building Blaze on Windows

Blaze compiles directly to x86-64 Linux ELF binaries. **Windows users need WSL**.

## Quick Start (WSL)

1. **Install WSL** (if not already installed):
```powershell
# In PowerShell as Administrator:
wsl --install
```

2. **Enter WSL**:
```powershell
wsl
```

3. **Install build tools**:
```bash
sudo apt update
sudo apt install build-essential git
```

4. **Clone and build Blaze**:
```bash
git clone https://github.com/elyfly/blaze.git
cd blaze/blaze
make
```

5. **Test it works**:
```bash
make test
```

## Common Issues

### "make: command not found"
You're not in WSL. Type `wsl` first.

### PowerShell errors
```powershell
PS C:\> make
# ERROR - make is a Linux command!
```
**Solution**: Enter WSL with `wsl` command first.

### "cannot execute binary file"
You're trying to run a Linux binary on Windows.
**Solution**: Run inside WSL.

## Full Example

```powershell
# In Windows PowerShell:
PS C:\Users\You> wsl
```

```bash
# Now in WSL:
$ cd /mnt/c/Users/You/blaze/blaze
$ make clean
$ make
$ echo 'print/ "Hello from WSL!" \' > test.blaze
$ ./blaze test.blaze test_out
$ ./test_out
Hello from WSL!
```

## Why WSL?

Blaze generates native Linux x86-64 binaries with:
- Direct syscalls (Linux kernel interface)
- ELF format (Linux executable format)
- No Windows PE support (yet)

## Future Windows Support

Native Windows support (PE format) is planned. For now, WSL provides full Linux compatibility on Windows.

## VS Code Integration

1. Install "WSL" extension in VS Code
2. Open folder in WSL: `code .` from WSL terminal
3. Full IntelliSense and debugging support

## Performance Note

WSL2 runs at near-native speed. Blaze binaries execute with no performance penalty.