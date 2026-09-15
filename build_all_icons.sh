#!/usr/bin/env bash
set -e

mkdir -p assets/icons assets/icons/ubuntu assets/icons/cool
mkdir -p public/icons/ubuntu public/icons/cool

echo "=== 1. Generating SVG Vector Sources ==="

# 1. Ubuntu Palette SVG
cat << 'SVG_EOF' > assets/icons/appclip-manager-ubuntu.svg
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512" width="512" height="512">
  <defs>
    <!-- Background Gradient: Ubuntu Orange to Dark Aubergine -->
    <linearGradient id="bg-ubuntu" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" stop-color="#FF6A38"/>
      <stop offset="40%" stop-color="#E95420"/>
      <stop offset="100%" stop-color="#2C001E"/>
    </linearGradient>

    <!-- Metallic Clamp Gradient -->
    <linearGradient id="clamp-grad" x1="0%" y1="0%" x2="0%" y2="100%">
      <stop offset="0%" stop-color="#4A1538"/>
      <stop offset="100%" stop-color="#220017"/>
    </linearGradient>

    <!-- Board Drop Shadow -->
    <filter id="drop-shadow" x="-15%" y="-15%" width="130%" height="130%">
      <feDropShadow dx="0" dy="10" stdDeviation="12" flood-color="#0F172A" flood-opacity="0.45"/>
    </filter>

    <!-- Squircle Shadow -->
    <filter id="squircle-shadow" x="-10%" y="-10%" width="120%" height="125%">
      <feDropShadow dx="0" dy="16" stdDeviation="18" flood-color="#000000" flood-opacity="0.4"/>
    </filter>
  </defs>

  <!-- Squircle Base with Drop Shadow -->
  <rect x="36" y="36" width="440" height="440" rx="102" ry="102"
        fill="url(#bg-ubuntu)" filter="url(#squircle-shadow)"/>

  <!-- Subtle Top Highlight Rim -->
  <rect x="38" y="38" width="436" height="436" rx="100" ry="100"
        fill="none" stroke="#FFFFFF" stroke-opacity="0.25" stroke-width="2.5"/>

  <!-- Installed Applications: 2x2 App Grid -->
  <!-- Top-Left App Tile -->
  <rect x="106" y="112" width="112" height="112" rx="24" ry="24"
        fill="#FFFFFF" fill-opacity="0.92"/>
  <!-- App glyph: Terminal prompt -->
  <path d="M 138 152 L 158 168 L 138 184" fill="none" stroke="#E95420" stroke-width="6" stroke-linecap="round" stroke-linejoin="round"/>
  <line x1="166" y1="184" x2="186" y2="184" stroke="#E95420" stroke-width="6" stroke-linecap="round"/>

  <!-- Top-Right App Tile -->
  <rect x="238" y="112" width="112" height="112" rx="24" ry="24"
        fill="#FFFFFF" fill-opacity="0.92"/>
  <!-- App glyph: Package / Box icon -->
  <path d="M 294 140 L 324 154 L 294 168 L 264 154 Z" fill="#77216F"/>
  <path d="M 264 158 L 264 184 L 292 198 L 292 172 Z" fill="#5E1A57"/>
  <path d="M 296 172 L 296 198 L 324 184 L 324 158 Z" fill="#77216F"/>

  <!-- Bottom-Left App Tile -->
  <rect x="106" y="244" width="112" height="112" rx="24" ry="24"
        fill="#FFFFFF" fill-opacity="0.92"/>
  <!-- App glyph: Gear / Settings -->
  <circle cx="162" cy="300" r="14" fill="none" stroke="#2C001E" stroke-width="6"/>
  <path d="M 162 278 L 162 284 M 162 316 L 162 322 M 140 300 L 146 300 M 178 300 L 184 300 M 147 285 L 151 289 M 173 311 L 177 315 M 147 315 L 151 311 M 173 289 L 177 285"
        stroke="#2C001E" stroke-width="5" stroke-linecap="round"/>

  <!-- Overlapping Clipboard (Clipboard History) -->
  <!-- Clipboard Board with Drop Shadow -->
  <g filter="url(#drop-shadow)">
    <rect x="224" y="196" width="186" height="224" rx="22" ry="22"
          fill="#FFFFFF" stroke="#F1F5F9" stroke-width="2"/>

    <!-- Document Snippet Lines on Clipboard -->
    <!-- Primary clipped line (highlighted in Ubuntu Orange) -->
    <rect x="252" y="244" width="130" height="13" rx="6.5" ry="6.5" fill="#E95420"/>
    <!-- Secondary clipped line -->
    <rect x="252" y="272" width="114" height="11" rx="5.5" ry="5.5" fill="#64748B"/>
    <!-- Tertiary clipped line -->
    <rect x="252" y="298" width="124" height="11" rx="5.5" ry="5.5" fill="#94A3B8"/>
    <!-- Fourth clipped line -->
    <rect x="252" y="324" width="88" height="11" rx="5.5" ry="5.5" fill="#CBD5E1"/>

    <!-- Copy badge indicator (Two overlapping miniature papers at bottom right) -->
    <g transform="translate(336, 354)">
      <!-- Back sheet -->
      <rect x="4" y="0" width="26" height="32" rx="4" fill="#FED7AA" stroke="#E95420" stroke-width="2"/>
      <!-- Front sheet -->
      <rect x="0" y="6" width="26" height="32" rx="4" fill="#FFFFFF" stroke="#E95420" stroke-width="2"/>
      <!-- Content lines on copy icon -->
      <line x1="5" y1="13" x2="21" y2="13" stroke="#E95420" stroke-width="2" stroke-linecap="round"/>
      <line x1="5" y1="19" x2="17" y2="19" stroke="#E95420" stroke-width="2" stroke-linecap="round"/>
      <line x1="5" y1="25" x2="19" y2="25" stroke="#E95420" stroke-width="2" stroke-linecap="round"/>
    </g>

    <!-- Top Metallic Clipboard Clamp -->
    <rect x="275" y="180" width="84" height="30" rx="8" ry="8" fill="url(#clamp-grad)"/>
    <!-- Clamp clip handle -->
    <rect x="295" y="168" width="44" height="22" rx="6" ry="6"
          fill="none" stroke="#FFFFFF" stroke-width="3.5"/>
    <circle cx="317" cy="195" r="4" fill="#E95420"/>
  </g>
</svg>
SVG_EOF

# 2. Cooler Alternative Palette SVG (Teal to Purple)
cat << 'SVG_EOF' > assets/icons/appclip-manager-cool.svg
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512" width="512" height="512">
  <defs>
    <!-- Background Gradient: Purple to Teal -->
    <linearGradient id="bg-cool" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" stop-color="#7F5AF0"/>
      <stop offset="60%" stop-color="#3A86FF"/>
      <stop offset="100%" stop-color="#2CB67D"/>
    </linearGradient>

    <linearGradient id="clamp-cool" x1="0%" y1="0%" x2="0%" y2="100%">
      <stop offset="0%" stop-color="#1E1B4B"/>
      <stop offset="100%" stop-color="#0F172A"/>
    </linearGradient>

    <filter id="drop-shadow-cool" x="-15%" y="-15%" width="130%" height="130%">
      <feDropShadow dx="0" dy="10" stdDeviation="12" flood-color="#0F172A" flood-opacity="0.45"/>
    </filter>

    <filter id="squircle-shadow-cool" x="-10%" y="-10%" width="120%" height="125%">
      <feDropShadow dx="0" dy="16" stdDeviation="18" flood-color="#000000" flood-opacity="0.4"/>
    </filter>
  </defs>

  <rect x="36" y="36" width="440" height="440" rx="102" ry="102"
        fill="url(#bg-cool)" filter="url(#squircle-shadow-cool)"/>

  <rect x="38" y="38" width="436" height="436" rx="100" ry="100"
        fill="none" stroke="#FFFFFF" stroke-opacity="0.3" stroke-width="2.5"/>

  <!-- App Grid -->
  <rect x="106" y="112" width="112" height="112" rx="24" ry="24" fill="#FFFFFF" fill-opacity="0.92"/>
  <path d="M 138 152 L 158 168 L 138 184" fill="none" stroke="#7F5AF0" stroke-width="6" stroke-linecap="round" stroke-linejoin="round"/>
  <line x1="166" y1="184" x2="186" y2="184" stroke="#7F5AF0" stroke-width="6" stroke-linecap="round"/>

  <rect x="238" y="112" width="112" height="112" rx="24" ry="24" fill="#FFFFFF" fill-opacity="0.92"/>
  <path d="M 294 140 L 324 154 L 294 168 L 264 154 Z" fill="#2CB67D"/>
  <path d="M 264 158 L 264 184 L 292 198 L 292 172 Z" fill="#166534"/>
  <path d="M 296 172 L 296 198 L 324 184 L 324 158 Z" fill="#2CB67D"/>

  <rect x="106" y="244" width="112" height="112" rx="24" ry="24" fill="#FFFFFF" fill-opacity="0.92"/>
  <circle cx="162" cy="300" r="14" fill="none" stroke="#7F5AF0" stroke-width="6"/>
  <path d="M 162 278 L 162 284 M 162 316 L 162 322 M 140 300 L 146 300 M 178 300 L 184 300 M 147 285 L 151 289 M 173 311 L 177 315 M 147 315 L 151 311 M 173 289 L 177 285"
        stroke="#7F5AF0" stroke-width="5" stroke-linecap="round"/>

  <!-- Clipboard -->
  <g filter="url(#drop-shadow-cool)">
    <rect x="224" y="196" width="186" height="224" rx="22" ry="22"
          fill="#FFFFFF" stroke="#F1F5F9" stroke-width="2"/>

    <rect x="252" y="244" width="130" height="13" rx="6.5" ry="6.5" fill="#7F5AF0"/>
    <rect x="252" y="272" width="114" height="11" rx="5.5" ry="5.5" fill="#2CB67D"/>
    <rect x="252" y="298" width="124" height="11" rx="5.5" ry="5.5" fill="#94A3B8"/>
    <rect x="252" y="324" width="88" height="11" rx="5.5" ry="5.5" fill="#CBD5E1"/>

    <g transform="translate(336, 354)">
      <rect x="4" y="0" width="26" height="32" rx="4" fill="#DDD6FE" stroke="#7F5AF0" stroke-width="2"/>
      <rect x="0" y="6" width="26" height="32" rx="4" fill="#FFFFFF" stroke="#7F5AF0" stroke-width="2"/>
      <line x1="5" y1="13" x2="21" y2="13" stroke="#7F5AF0" stroke-width="2" stroke-linecap="round"/>
      <line x1="5" y1="19" x2="17" y2="19" stroke="#7F5AF0" stroke-width="2" stroke-linecap="round"/>
      <line x1="5" y1="25" x2="19" y2="25" stroke="#7F5AF0" stroke-width="2" stroke-linecap="round"/>
    </g>

    <rect x="275" y="180" width="84" height="30" rx="8" ry="8" fill="url(#clamp-cool)"/>
    <rect x="295" y="168" width="44" height="22" rx="6" ry="6"
          fill="none" stroke="#FFFFFF" stroke-width="3.5"/>
    <circle cx="317" cy="195" r="4" fill="#2CB67D"/>
  </g>
</svg>
SVG_EOF

cp assets/icons/appclip-manager-ubuntu.svg assets/icons/appclip-manager.svg
cp assets/icons/appclip-manager.svg public/icons/

echo "=== 2. Rendering High-Resolution 512x512 Master PNGs ==="

# Render Ubuntu Master PNG with ImageMagick primitives for mathematically crisp rasterization
# Step A: Squircle with Ubuntu Gradient
convert -size 440x440 -define gradient:angle=135 gradient:'#FF6E3A'-'#2C001E' \
  \( +clone -alpha transparent -background none -fill white -draw "roundrectangle 0,0 439,439 102,102" \) \
  -compose DstIn -composite /tmp/sq_u.png

convert -size 440x440 xc:none \
  -stroke 'rgba(255,255,255,0.30)' -strokewidth 2.5 -fill none \
  -draw "roundrectangle 1,1 438,438 101,101" /tmp/rim_u.png

convert /tmp/sq_u.png /tmp/rim_u.png -compose Over -composite /tmp/bg_u.png

convert -size 512x512 xc:none \
  \( -size 440x440 xc:none -fill 'rgba(0,0,0,0.42)' -draw "roundrectangle 0,0 439,439 102,102" -blur 0x18 \) \
  -geometry +36+46 -composite /tmp/sh_u.png

convert /tmp/sh_u.png /tmp/bg_u.png -geometry +36+36 -composite /tmp/base_u.png

# Step B: App grid tiles
convert -size 512x512 xc:none \
  -fill 'rgba(255,255,255,0.92)' \
  -draw "roundrectangle 106,112 218,224 24,24" \
  -draw "roundrectangle 238,112 350,224 24,24" \
  -draw "roundrectangle 106,244 218,356 24,24" \
  /tmp/tiles_u.png

# Step C: App glyphs inside tiles
convert /tmp/tiles_u.png \
  -stroke '#E95420' -strokewidth 6 -fill none \
  -draw "path 'M 138,152 L 158,168 L 138,184'" \
  -draw "path 'M 166,184 L 186,184'" \
  -stroke none -fill '#77216F' \
  -draw "path 'M 294,140 L 324,154 L 294,168 L 264,154 Z'" \
  -fill '#5E1A57' \
  -draw "path 'M 264,158 L 264,184 L 292,198 L 292,172 Z'" \
  -fill '#77216F' \
  -draw "path 'M 296,172 L 296,198 L 324,184 L 324,158 Z'" \
  -stroke '#2C001E' -strokewidth 5 -fill none \
  -draw "circle 162,300 162,286" \
  -draw "path 'M 162,278 L 162,284 M 162,316 L 162,322 M 140,300 L 146,300 M 178,300 L 184,300'" \
  /tmp/grid_u.png

# Step D: Overlapping Clipboard & Clip
convert -size 512x512 xc:none \
  \( -size 186x224 xc:none -fill 'rgba(15,23,42,0.48)' -draw "roundrectangle 0,0 185,223 22,22" -blur 0x14 \) \
  -geometry +224+208 -composite /tmp/clip_sh_u.png

convert -size 512x512 xc:none \
  -fill '#FFFFFF' -stroke '#F1F5F9' -strokewidth 2 \
  -draw "roundrectangle 224,196 410,420 22,22" \
  -stroke none \
  -fill '#E95420' \
  -draw "roundrectangle 252,244 382,257 6,6" \
  -fill '#64748B' \
  -draw "roundrectangle 252,272 366,283 5,5" \
  -fill '#94A3B8' \
  -draw "roundrectangle 252,298 376,309 5,5" \
  -fill '#CBD5E1' \
  -draw "roundrectangle 252,324 340,335 5,5" \
  -fill '#FED7AA' -stroke '#E95420' -strokewidth 2 \
  -draw "roundrectangle 342,354 368,386 4,4" \
  -fill '#FFFFFF' -stroke '#E95420' -strokewidth 2 \
  -draw "roundrectangle 338,360 364,392 4,4" \
  -stroke none -fill '#2C001E' \
  -draw "roundrectangle 275,180 359,210 8,8" \
  -stroke '#FFFFFF' -strokewidth 3.5 -fill none \
  -draw "roundrectangle 295,168 339,190 6,6" \
  -stroke none -fill '#E95420' \
  -draw "circle 317,195 317,191" \
  /tmp/clip_body_u.png

# Composite Ubuntu 512x512 Master
convert /tmp/base_u.png /tmp/grid_u.png -compose Over -composite \
  /tmp/clip_sh_u.png -compose Over -composite \
  /tmp/clip_body_u.png -compose Over -composite \
  assets/icons/ubuntu/appclip-manager-512x512.png

# Standard primary path
cp assets/icons/ubuntu/appclip-manager-512x512.png assets/icons/appclip-manager-512x512.png
cp assets/icons/ubuntu/appclip-manager-512x512.png public/icons/appclip-manager-512x512.png
cp assets/icons/ubuntu/appclip-manager-512x512.png public/logo.png
cp assets/icons/ubuntu/appclip-manager-512x512.png assets/logo.png

# Render Cooler Master PNG
convert -size 440x440 -define gradient:angle=135 gradient:'#7F5AF0'-'#2CB67D' \
  \( +clone -alpha transparent -background none -fill white -draw "roundrectangle 0,0 439,439 102,102" \) \
  -compose DstIn -composite /tmp/sq_c.png

convert -size 440x440 xc:none \
  -stroke 'rgba(255,255,255,0.30)' -strokewidth 2.5 -fill none \
  -draw "roundrectangle 1,1 438,438 101,101" /tmp/rim_c.png

convert /tmp/sq_c.png /tmp/rim_c.png -compose Over -composite /tmp/bg_c.png

convert /tmp/sh_u.png /tmp/bg_c.png -geometry +36+36 -composite /tmp/base_c.png

convert /tmp/tiles_u.png \
  -stroke '#7F5AF0' -strokewidth 6 -fill none \
  -draw "path 'M 138,152 L 158,168 L 138,184'" \
  -draw "path 'M 166,184 L 186,184'" \
  -stroke none -fill '#2CB67D' \
  -draw "path 'M 294,140 L 324,154 L 294,168 L 264,154 Z'" \
  -fill '#166534' \
  -draw "path 'M 264,158 L 264,184 L 292,198 L 292,172 Z'" \
  -fill '#2CB67D' \
  -draw "path 'M 296,172 L 296,198 L 324,184 L 324,158 Z'" \
  -stroke '#7F5AF0' -strokewidth 5 -fill none \
  -draw "circle 162,300 162,286" \
  -draw "path 'M 162,278 L 162,284 M 162,316 L 162,322 M 140,300 L 146,300 M 178,300 L 184,300'" \
  /tmp/grid_c.png

convert -size 512x512 xc:none \
  -fill '#FFFFFF' -stroke '#F1F5F9' -strokewidth 2 \
  -draw "roundrectangle 224,196 410,420 22,22" \
  -stroke none \
  -fill '#7F5AF0' \
  -draw "roundrectangle 252,244 382,257 6,6" \
  -fill '#2CB67D' \
  -draw "roundrectangle 252,272 366,283 5,5" \
  -fill '#94A3B8' \
  -draw "roundrectangle 252,298 376,309 5,5" \
  -fill '#CBD5E1' \
  -draw "roundrectangle 252,324 340,335 5,5" \
  -fill '#DDD6FE' -stroke '#7F5AF0' -strokewidth 2 \
  -draw "roundrectangle 342,354 368,386 4,4" \
  -fill '#FFFFFF' -stroke '#7F5AF0' -strokewidth 2 \
  -draw "roundrectangle 338,360 364,392 4,4" \
  -stroke none -fill '#0F172A' \
  -draw "roundrectangle 275,180 359,210 8,8" \
  -stroke '#FFFFFF' -strokewidth 3.5 -fill none \
  -draw "roundrectangle 295,168 339,190 6,6" \
  -stroke none -fill '#2CB67D' \
  -draw "circle 317,195 317,191" \
  /tmp/clip_body_c.png

convert /tmp/base_c.png /tmp/grid_c.png -compose Over -composite \
  /tmp/clip_sh_u.png -compose Over -composite \
  /tmp/clip_body_c.png -compose Over -composite \
  assets/icons/cool/appclip-manager-512x512.png

cp assets/icons/cool/appclip-manager-512x512.png public/icons/cool/

echo "=== 3. Downsampling to All Required Sizes (256, 128, 64, 48, 32, 16) ==="

SIZES="256 128 64 48 32 16"

for sz in $SIZES; do
  # Ubuntu Palette
  convert assets/icons/ubuntu/appclip-manager-512x512.png -filter Lanczos -resize ${sz}x${sz} assets/icons/ubuntu/appclip-manager-${sz}x${sz}.png
  cp assets/icons/ubuntu/appclip-manager-${sz}x${sz}.png assets/icons/appclip-manager-${sz}x${sz}.png
  cp assets/icons/ubuntu/appclip-manager-${sz}x${sz}.png public/icons/
  cp assets/icons/ubuntu/appclip-manager-${sz}x${sz}.png public/icons/ubuntu/

  # Cool Palette
  convert assets/icons/cool/appclip-manager-512x512.png -filter Lanczos -resize ${sz}x${sz} assets/icons/cool/appclip-manager-${sz}x${sz}.png
  cp assets/icons/cool/appclip-manager-${sz}x${sz}.png public/icons/cool/

  # Hicolor standard FreeDesktop directory hierarchy
  mkdir -p assets/icons/hicolor/${sz}x${sz}/apps
  cp assets/icons/ubuntu/appclip-manager-${sz}x${sz}.png assets/icons/hicolor/${sz}x${sz}/apps/appclip-manager.png
done

mkdir -p assets/icons/hicolor/512x512/apps
cp assets/icons/ubuntu/appclip-manager-512x512.png assets/icons/hicolor/512x512/apps/appclip-manager.png

echo "=== 4. Creating Icon Archive (tar.gz & zip) ==="
cd assets/icons
tar -czvf appclip-manager-icons.tar.gz *.png hicolor/ appclip-manager.svg
cp appclip-manager-icons.tar.gz ../../public/
cd ../..

echo "=== Done! All icons generated successfully ==="
ls -lh assets/icons/*.png
