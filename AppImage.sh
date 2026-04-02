#!/bin/bash

set -e

# Build and install to AppDir
mkdir -p build && cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr ..
ninja
DESTDIR=$(pwd)/AppDir ninja install
cd ..

# Create AppRun script with library path
cat > build/AppDir/AppRun << 'EOF'
#!/bin/bash
SELF=$(readlink -f "$0")
HERE=${SELF%/*}
export LD_LIBRARY_PATH="${HERE}/usr/lib/:${LD_LIBRARY_PATH}"
exec "${HERE}/usr/bin/ytv_downloader" "$@"
EOF
chmod +x build/AppDir/AppRun

# Create desktop file
cat > build/AppDir/ytv_downloader.desktop << 'EOF'
[Desktop Entry]
Name=YTV Downloader
Comment=YouTube Video Downloader
Exec=ytv_downloader
Icon=ytv_downloader
Type=Application
Categories=AudioVideo;
EOF

# Add icon
cp assets/Icon/ytv_downloader.png build/AppDir/ytv_downloader.png

# Bundle raylib library
mkdir -p build/AppDir/usr/lib
cp /usr/lib64/libraylib.so.550 build/AppDir/usr/lib/

# Download runtime file (once, not every time)
if [ ! -f "runtime-x86_64" ]; then
    wget https://github.com/AppImage/AppImageKit/releases/download/continuous/runtime-x86_64
fi

# Create AppImage with runtime file
ARCH=x86_64 appimagetool --runtime-file runtime-x86_64 build/AppDir YTV-Downloader-x86_64.AppImage

chmod +x YTV-Downloader-x86_64.AppImage

echo "AppImage created: YTV-Downloader-x86_64.AppImage"