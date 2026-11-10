#!/bin/sh
# Renders compute-gallery.svg into the icon files the build uses. Run it after changing the
# SVG (needs rsvg-convert, ImageMagick and png2icns: apt install librsvg2-bin imagemagick icnsutils).
set -e
cd "$(dirname "$0")"

for size in 16 32 48 64 128 256 512 1024; do
    rsvg-convert -w $size -h $size compute-gallery.svg -o /tmp/cg-icon-$size.png
done

# Window icons (GLFW) for Windows and Linux.
for size in 16 32 48 256; do
    cp /tmp/cg-icon-$size.png compute-gallery-$size.png
done

# Windows executable icon.
convert /tmp/cg-icon-16.png /tmp/cg-icon-32.png /tmp/cg-icon-48.png /tmp/cg-icon-64.png \
        /tmp/cg-icon-128.png /tmp/cg-icon-256.png compute-gallery.ico

# macOS bundle icon.
png2icns compute-gallery.icns /tmp/cg-icon-16.png /tmp/cg-icon-32.png /tmp/cg-icon-48.png \
         /tmp/cg-icon-128.png /tmp/cg-icon-256.png /tmp/cg-icon-512.png /tmp/cg-icon-1024.png
