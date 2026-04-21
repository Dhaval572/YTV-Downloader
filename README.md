# YouTube Video Downloader

A clean, modern desktop app for downloading YouTube videos and audio.

![YouTube Video Downloader Demo](Application.gif)

## Built With

- **C++23**
- **Dear ImGui** — GUI framework
- **Raylib** — Window management
- **yt-dlp** — Video download backend
- **ffmpeg** — Media processing

## Build

The commands below use CMake’s recommended “out-of-source” workflow and `cmake --build` (so you don’t have to remember `ninja`/`make` flags).

### macOS (build + run)

This project requires CMake `>= 3.27` and a C++23-capable Clang (Xcode 15+ recommended).

1) Install Xcode Command Line Tools:

```bash
xcode-select --install
```

2) Install build tools (Homebrew recommended):

```bash
brew install cmake ninja
```

3) Configure + build (Ninja):

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

4) Run:

```bash
./build/ytv_downloader
```

If Ninja isn’t available, build with Xcode instead:

```bash
cmake -S . -B build-xcode -G Xcode
cmake --build build-xcode --config Release
./build-xcode/Release/ytv_downloader
```

If you hit C++23 / `<format>` compiler errors on older Xcode, use Homebrew LLVM:

```bash
brew install llvm
export CC="$(brew --prefix llvm)/bin/clang"
export CXX="$(brew --prefix llvm)/bin/clang++"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/ytv_downloader
```

### Linux (build + run)

Install prerequisites (example for Ubuntu/Debian):

```bash
sudo apt update
sudo apt install -y cmake ninja-build build-essential
```

Configure + build:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Run:

```bash
./build/ytv_downloader
```

### Windows (build + run)

You can build with either Visual Studio (recommended) or Ninja.

Option A) Visual Studio generator (multi-config):

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
.\build\Release\ytv_downloader.exe
```

Option B) Ninja generator (single-config):

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
.\build\ytv_downloader.exe
```

For Windows, the build produces an executable (`.exe`).  
For Linux, run `./AppImage.sh` to package as AppImage.

## Platforms

Windows • Linux • macOS
