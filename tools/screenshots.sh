#!/bin/bash
# Capture the "current release" screenshots: the main window and the
# Configuration dialog, in Breeze light and Breeze dark, as real KDE windows.
#
#     git archive v1.8.2 | tar -x -C /tmp/qtpass-src
#     (cd /tmp/qtpass-src && qmake -qt=qt5 && make -j)
#     tools/screenshots.sh /tmp/qtpass-src/main/qtpass
#
# Runs headless: Xvfb, KWin for the Breeze title bars, and a throwaway home
# with a demo GnuPG key and store, so nothing of yours ends up in a picture.
# Captured at 2x, then given rounded top corners and a drop shadow, and
# written to images/ as 1x and @2x PNG plus @2x WebP. Bump the ?v= asset
# version (sw.js, the pages, tools/build-page.py) after replacing them.
#
# Needs (Ubuntu 24.04): qtbase5-dev qttools5-dev-tools libqt5svg5-dev breeze
# breeze-icon-theme kwin-x11 plasma-integration xvfb x11-utils
# x11-xserver-utils xdotool dbus-x11 imagemagick webp pngquant pass
# pwgen qrencode fonts-noto-core

set -euo pipefail

QTPASS=$(realpath "${1:?usage: $0 path/to/qtpass}")
SITE=$(cd "$(dirname "$0")/.." && pwd)
SC=2                # capture scale
DPY=:77
# Short on purpose: gpg-agent's socket path must stay under ~100 characters.
WORK=$(mktemp -d /tmp/qtpass-shots.XXXX)
trap 'rm -rf "$WORK"' EXIT

export HOME=$WORK/home GNUPGHOME=$WORK/home/.gnupg
export PASSWORD_STORE_DIR=$HOME/.password-store
export XDG_RUNTIME_DIR=$HOME/run
export DISPLAY=$DPY LANG=en_US.UTF-8
export XDG_CURRENT_DESKTOP=KDE KDE_SESSION_VERSION=5 KDE_FULL_SESSION=true
export QT_QPA_PLATFORMTHEME=kde QT_STYLE_OVERRIDE=Breeze
mkdir -p -m700 "$GNUPGHOME" "$XDG_RUNTIME_DIR"

# --- demo key, store and settings ------------------------------------------
echo allow-loopback-pinentry > "$GNUPGHOME/gpg-agent.conf"
gpg -q --batch --pinentry-mode loopback --passphrase '' \
  --quick-gen-key 'Anne Example <anne@example.com>' ed25519 sign,cert never
FPR=$(gpg --list-keys --with-colons anne@example.com | awk -F: '/^fpr/{print $10; exit}')
gpg -q --batch --pinentry-mode loopback --passphrase '' \
  --quick-add-key "$FPR" cv25519 encr never
pass init "$FPR" >/dev/null
git config --global user.name 'Anne Example'
git config --global user.email anne@example.com
git config --global init.defaultBranch main
pass git init >/dev/null
ins() { printf '%b' "$2" | pass insert -m "$1" >/dev/null; }
ins Keys/ssh-passphrase 'correct-horse-battery-staple\n'
ins Personal/Banking/bank.example.nl 'Xq7!pLm2@vRt9#kWz\nlogin: anne\nurl: https://bank.example.nl\n'
ins Personal/Email/mail.example.com 'hT4$wQ9zLk2!mNp8vB6x\nlogin: anne@example.com\nurl: https://mail.example.com\notpauth://totp/mail.example.com:anne@example.com?secret=JBSWY3DPEHPK3PXP&issuer=mail.example.com\n'
ins Personal/Shopping/shop.example.com 'Zr8#nK3pQw6!\nlogin: anne@example.com\nurl: https://shop.example.com\n'
ins Personal/Social/social.example.org 'Mv5@tY2kLp9#\nlogin: anne\nurl: https://social.example.org\n'
ins Servers/web.example.net 'Hp3!zX8qWr5m\n'
ins Wi-Fi/home 'trees-under-the-bridge\n'
ins Wi-Fi/office 'meeting-room-coffee\n'
ins Work/ACME/git.acme.example 'Gb7#kP2xLq9w\nlogin: anne\nurl: https://git.acme.example\n'
ins Work/ACME/issues.acme.example 'Ds4!mR8tNz3q\nlogin: anne\nurl: https://issues.acme.example\n'
ins Work/ACME/vpn 'Wf6@jK1pXc8v\n'
ins Work/Clients/IJHack/wiki 'Ly2#qT7mBn4k\nlogin: anne\nurl: https://wiki.ijhack.org\n'

mkdir -p "$HOME/.config/IJHack"
cat > "$HOME/.config/IJHack/QtPass.conf" <<EOF
[General]
usePass=false
passStore=$PASSWORD_STORE_DIR/
gpgExecutable=$(command -v gpg)
gitExecutable=$(command -v git)
passExecutable=$(command -v pass)
pwgenExecutable=$(command -v pwgen)
qrencodeExecutable=$(command -v qrencode)
usePwgen=true
useGit=true
addGPGId=true
useOtp=true
otpMigratedToNative=true
hidePassword=true
useAutoclear=true
autoclearSeconds=45
useAutoclearPanel=true
autoclearPanelSeconds=10
clipBoardType=1
passwordLength=16
useTrayIcon=false
useTemplate=true
EOF

# --- one desktop session per colour scheme ---------------------------------
RAW=$WORK/raw; mkdir -p "$RAW"

session() { # Breeze or Breeze Dark, KWin decorations scaled with the title font
  local scheme=$1 cs=BreezeLight icons=breeze laf=org.kde.breeze.desktop
  if [ "$scheme" = dark ]; then
    cs=BreezeDark icons=breeze-dark laf=org.kde.breezedark.desktop
  fi
  # KIconLoader caches icons across runs; a stale cache keeps the light set.
  rm -rf "$HOME/.cache"
  cp "/usr/share/color-schemes/$cs.colors" "$HOME/.config/kdeglobals"
  cat >> "$HOME/.config/kdeglobals" <<EOF

[KDE]
widgetStyle=Breeze
LookAndFeelPackage=$laf

[Icons]
Theme=$icons

[General]
ColorScheme=$cs
font=Noto Sans,10,-1,5,50,0,0,0,0,0
menuFont=Noto Sans,10,-1,5,50,0,0,0,0,0
toolBarFont=Noto Sans,10,-1,5,50,0,0,0,0,0
smallestReadableFont=Noto Sans,8,-1,5,50,0,0,0,0,0
fixed=Noto Mono,10,-1,5,50,0,0,0,0,0

[WM]
activeFont=Noto Sans,$((10 * SC)),-1,5,50,0,0,0,0,0
EOF
  printf '[org.kde.kdecoration2]\nlibrary=org.kde.breeze\ntheme=Breeze\n\n[Compositing]\nEnabled=false\n' \
    > "$HOME/.config/kwinrc"
  Xvfb $DPY -screen 0 $((1400 * SC))x$((1100 * SC))x24 -dpi 96 -nolisten tcp >/dev/null 2>&1 &
  XVFB=$!
  sleep 1
  eval "$(dbus-launch --sh-syntax)"
  kwin_x11 --replace >"$WORK/kwin.log" 2>&1 &
  KWIN=$!
  sleep 3
}

end_session() {
  kill "$KWIN" "$XVFB" "$DBUS_SESSION_BUS_PID" 2>/dev/null || true
  wait "$KWIN" "$XVFB" 2>/dev/null || true
}

frame() { # window $1 including its title bar, cropped from the root window
  local id=$1 out=$2 X Y WIDTH HEIGHT SCREEN WINDOW L R T B
  eval "$(xdotool getwindowgeometry --shell "$id")"
  read -r L R T B < <(xprop -id "$id" _NET_FRAME_EXTENTS | sed 's/.*= //; s/,//g')
  import -window root -crop $((WIDTH + L + R))x$((HEIGHT + T + B))+$((X - L))+$((Y - T)) +repage "$out"
}

capture() {
  local scheme=$1 W C
  session "$scheme"
  QT_SCALE_FACTOR=$SC QT_FONT_DPI=96 "$QTPASS" >"$WORK/qtpass.log" 2>&1 &
  local qp=$!
  sleep 4
  W=$(xdotool search --sync --onlyvisible --name '^QtPass$' | head -1)
  xdotool windowsize "$W" $((980 * SC)) $((800 * SC))
  xdotool windowmove "$W" $((100 * SC)) $((100 * SC))
  sleep 1

  # Tree rows, in logical pixels from the top of the client area.
  row() {
    xdotool mousemove --window "$W" $((60 * SC)) $(((116 + 26 * $1) * SC)) click 1
    sleep 0.4
  }
  # The model loads folders lazily: expand bottom-up so the rows above keep
  # their place. Top level: Work, Wi-Fi, Personal, Keys.
  for r in 4 3 1 0; do row $r; xdotool key --window "$W" asterisk; sleep 0.6; done
  # Then Clients, ACME, Social, Shopping, Email, Banking, and Clients/IJHack.
  for r in 13 12 6 5 4 3 21; do row $r; xdotool key --window "$W" Right; sleep 0.6; done
  row 6 # Personal/Email/mail.example.com
  sleep 2
  xdotool mousemove $((1350 * SC)) $((1050 * SC))
  sleep 1
  frame "$W" "$RAW/qtpass-$scheme.png"

  # The Configuration button, last on the toolbar.
  xdotool mousemove --window "$W" $((327 * SC)) $((20 * SC)) click 1
  sleep 3
  C=$(xdotool search --onlyvisible --name '^Configuration$' | head -1)
  xdotool windowmove "$C" $((100 * SC)) $((50 * SC))
  xdotool mousemove $((1350 * SC)) $((1050 * SC))
  sleep 1
  frame "$C" "$RAW/config-$scheme.png"

  kill "$qp"; wait "$qp" 2>/dev/null || true
  end_session
}

capture light
capture dark

# --- rounded top corners, outline, shadow; 1x, @2x and WebP ----------------
for shot in qtpass config; do
  for scheme in light dark; do
    in=$RAW/$shot-$scheme.png
    name=$shot edge='rgba(0,0,0,0.22)'
    if [ "$scheme" = dark ]; then name=$shot-dark edge='rgba(0,0,0,0.55)'; fi
    read -r W H < <(identify -format '%w %h\n' "$in")
    r=$((4 * SC))
    convert -size "${W}x${H}" xc:none -fill white \
      -draw "roundrectangle 0,0 $((W - 1)),$((H - 1)) $r,$r" \
      -draw "rectangle 0,$((H - 2 * r)) $((W - 1)),$((H - 1))" "$WORK/mask.png"
    convert "$in" -alpha set "$WORK/mask.png" -compose DstIn -composite \
      \( +clone -alpha extract -morphology EdgeIn Diamond:1 -background "$edge" -alpha shape \) \
      -compose Over -composite "$WORK/win.png"
    convert "$WORK/win.png" \( +clone -background black -shadow 45x$((18 * SC))+0+$((12 * SC)) \) +swap \
      -background none -layers merge +repage \
      -gravity center -extent $((W + 130 * SC))x$((H + 130 * SC)) "$SITE/images/$name@2x.png"
    convert "$SITE/images/$name@2x.png" -filter Lanczos -resize 50% "$SITE/images/$name.png"
    for f in "$SITE/images/$name@2x.png" "$SITE/images/$name.png"; do
      pngquant --force --skip-if-larger --quality 85-98 --speed 1 --output "$f" "$f" || true
    done
    cwebp -quiet -lossless -z 9 "$SITE/images/$name@2x.png" -o "$SITE/images/$name@2x.webp"
  done
done

identify "$SITE"/images/{qtpass,config}{,-dark}{,@2x}.png
