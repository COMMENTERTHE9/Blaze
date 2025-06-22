# Blaze Compiler - Setup Instructions

## What You Need to Do:

### 1. Fix the package issue in WSL:
```bash
# Try updating with fix-missing
sudo apt update --fix-missing
sudo apt install gcc

# OR try cleaning the cache first
sudo apt clean
sudo apt update
sudo apt install gcc
```

### 2. Once gcc is installed, build the compiler:
```bash
cd "/mnt/c/Users/Gabri/OneDrive/Desktop/folder of folders/elyfly/blaze"
./build.sh
```

### 3. Use the compiler:

**From WSL (required):**
```bash
# Generate Windows .exe
./blaze_compiler hello_world.blaze -p w

# Generate Linux executable
./blaze_compiler hello_world.blaze

# Generate both
./blaze_compiler hello_world.blaze -a
```

### 4. Run the output:

**Windows executable (from Windows PowerShell/CMD):**
```powershell
cd "C:\Users\Gabri\OneDrive\Desktop\folder of folders\elyfly\blaze"
.\output.exe
```

**Linux executable (from WSL):**
```bash
./output
```

## Key Points:

1. **The compiler runs in WSL** - You cannot run `./blaze_compiler` from Windows
2. **The compiler generates both Windows and Linux executables**
3. **Windows .exe files can be run from Windows PowerShell/CMD**
4. **Linux executables run in WSL**

## If gcc won't install:

Try these alternatives:
```bash
# Option 1: Use a different mirror
sudo sed -i 's/archive.ubuntu.com/mirror.math.princeton.edu/g' /etc/apt/sources.list
sudo apt update
sudo apt install gcc

# Option 2: Install build-essential (includes gcc)
sudo apt install build-essential

# Option 3: Clear everything and retry
sudo rm -rf /var/lib/apt/lists/*
sudo apt update
sudo apt install gcc
```

## Demo Mode:

If you can't get gcc installed, you can still see how it works:
```bash
./demo_compiler.sh hello_world.blaze -p w
```

This shows you what the real compiler would do!