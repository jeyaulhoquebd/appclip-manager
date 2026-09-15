#!/usr/bin/env bash
set -e

# 1. Base transparent 512x512 canvas
# 2. Render shadow for squircle
# 3. Render squircle with 135-deg gradient #E95420 to #2C001E
# 4. Render app grid & clipboard
# 5. Composite all layers

mkdir -p assets/icons assets/icons/hicolor public/icons

# Squircle gradient
convert -size 440x440 -define gradient:angle=135 gradient:'#E95420'-'#2C001E' \
  \( +clone -alpha transparent -background none -fill white -draw "roundrectangle 0,0 439,439 100,100" \) \
  -compose DstIn -composite /tmp/squircle.png

# Subtle top highlight rim
convert -size 440x440 xc:none \
  -stroke 'rgba(255,255,255,0.35)' -strokewidth 3 -fill none \
  -draw "roundrectangle 2,2 437,437 99,99" /tmp/rim.png

# Composite squircle + rim
convert /tmp/squircle.png /tmp/rim.png -compose Over -composite /tmp/bg.png

# Squircle drop shadow
convert -size 512x512 xc:none \
  \( -size 440x440 xc:none -fill 'rgba(0,0,0,0.4)' -draw "roundrectangle 0,0 439,439 100,100" -blur 0x16 \) \
  -geometry +36+46 -composite /tmp/shadow.png

# Put squircle on shadow
convert /tmp/shadow.png /tmp/bg.png -geometry +36+36 -composite /tmp/base_card.png

# Elements layer (512x512 transparent)
convert -size 512x512 xc:none \
  -fill 'rgba(255,255,255,0.85)' \
  -draw "roundrectangle 104,116 216,228 22,22" \
  -draw "roundrectangle 240,116 352,228 22,22" \
  -draw "roundrectangle 104,252 216,364 22,22" \
  /tmp/tiles.png

# Symbols inside tiles (Package box glyphs / app glyphs)
convert /tmp/tiles.png \
  -fill '#E95420' \
  -draw "circle 160,172 160,154" \
  -fill '#7F5AF0' \
  -draw "roundrectangle 272,148 320,196 8,8" \
  -fill '#2CB67D' \
  -draw "path 'M 160,284 L 188,332 L 132,332 Z'" \
  /tmp/apps_grid.png

# Clipboard shadow
convert -size 512x512 xc:none \
  \( -size 184x224 xc:none -fill 'rgba(15,23,42,0.5)' -draw "roundrectangle 0,0 183,223 20,20" -blur 0x12 \) \
  -geometry +224+208 -composite /tmp/clip_shadow.png

# Clipboard body (White board + clip + lines)
convert -size 512x512 xc:none \
  -fill '#FFFFFF' -stroke '#E2E8F0' -strokewidth 2 \
  -draw "roundrectangle 224,196 400,410 20,20" \
  -stroke none \
  -fill '#2C001E' \
  -draw "roundrectangle 274,180 350,212 8,8" \
  -stroke '#FFFFFF' -strokewidth 3 -fill '#E95420' \
  -draw "roundrectangle 292,168 332,192 6,6" \
  -stroke none \
  -fill '#E95420' \
  -draw "roundrectangle 252,240 372,254 6,6" \
  -fill '#64748B' \
  -draw "roundrectangle 252,274 352,286 5,5" \
  -fill '#94A3B8' \
  -draw "roundrectangle 252,306 364,318 5,5" \
  -fill '#CBD5E1' \
  -draw "roundrectangle 252,338 320,350 5,5" \
  -fill '#E95420' -stroke '#FFFFFF' -strokewidth 2 \
  -draw "circle 364,374 364,360" \
  -fill '#FFFFFF' -stroke none \
  -draw "path 'M 358,374 L 363,379 L 372,369 L 370,367 L 363,375 L 360,372 Z'" \
  /tmp/clipboard.png

# Final composite
convert /tmp/base_card.png /tmp/apps_grid.png -compose Over -composite \
  /tmp/clip_shadow.png -compose Over -composite \
  /tmp/clipboard.png -compose Over -composite \
  assets/icons/appclip-manager-512x512.png

echo "512x512 icon generated successfully!"
file assets/icons/appclip-manager-512x512.png
