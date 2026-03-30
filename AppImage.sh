#!/bin/bash

set -e

mkdir -p build && cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
ninja -j8
cd ..

mkdir -p AppDir/usr/{bin,lib,share/ytv_downloader/assets,share/fonts}
mkdir -p AppDir/etc/fonts

cp build/ytv_downloader AppDir/usr/bin/
cp -r assets/* AppDir/usr/share/ytv_downloader/assets/

if [ -d "assets/Font" ]; then
    cp -r assets/Font/*.ttf AppDir/usr/share/fonts/ || true
fi

if [ -f "/etc/fonts/fonts.conf" ]; then
    cp /etc/fonts/fonts.conf AppDir/etc/fonts/
fi

# Copy libraries recursively
echo "Copying dependencies..."
ldd build/ytv_downloader | grep "=> /" | awk '{print $3}' | while read lib; do
    if [ -f "$lib" ]; then
        cp -n "$lib" AppDir/usr/lib/ 2>/dev/null || true
        # Also copy dependencies of each library
        ldd "$lib" 2>/dev/null | grep "=> /" | awk '{print $3}' | while read dep; do
            if [ -f "$dep" ]; then
                cp -n "$dep" AppDir/usr/lib/ 2>/dev/null || true
            fi
        done
    fi
done

# Create AppRun with library path
cat > AppDir/AppRun << 'EOF'
#!/bin/bash
HERE="$(dirname "$(readlink -f "$0")")"

export APPDIR="$HERE"
export LD_LIBRARY_PATH="$HERE/usr/lib:$LD_LIBRARY_PATH"
export FONTCONFIG_PATH="$HERE/etc/fonts"
export FONTCONFIG_FILE="$HERE/etc/fonts/fonts.conf"
export XDG_DATA_DIRS="$HERE/usr/share:$XDG_DATA_DIRS"

exec "$HERE/usr/bin/ytv_downloader" "$@"
EOF

chmod +x AppDir/AppRun

cat > AppDir/ytv_downloader.desktop << EOF
[Desktop Entry]
Name=Youtube Video Downloader
Comment=Download videos from YouTube
Exec=ytv_downloader
Icon=ytv_downloader
Terminal=false
Type=Application
Categories=AudioVideo;Network;
EOF

if command -v convert &> /dev/null; then
    convert -size 128x128 xc:gray -fill white -gravity center -pointsize 20 -annotate 0 "YTV" AppDir/ytv_downloader.png
else
    touch AppDir/ytv_downloader.png
fi

if [ ! -f appimagetool-x86_64.AppImage ]; then
    wget -q https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
    chmod +x appimagetool-x86_64.AppImage
fi

echo "Extracting appimagetool..."
./appimagetool-x86_64.AppImage --appimage-extract
mv squashfs-root appimagetool-extracted

echo "Creating AppImage..."
./appimagetool-extracted/AppRun AppDir

rm -rf appimagetool-extracted

mv *.AppImage Youtube_Video_Downloader-Linux-x86_64.AppImage 2>/dev/null || true

echo "Created: $(ls *.AppImage 2>/dev/null)"