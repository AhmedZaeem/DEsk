#!/bin/bash
# redronin-greeter.sh -- make the dojo login (SDDM theme red-ronin-dojo) from
# DEsk's own lock screen: the door at rest on a rainy night, rendered by
# desk-preview at the size given, and where its password slot lands, written
# into the theme's theme.conf as fractions of the picture.
#
#   scripts/redronin-greeter.sh [THEME_DIR] [WxH]
#
# THEME_DIR defaults to packaging/redronin/sddm/red-ronin-dojo; WxH to 2560x1440.
# Run it as the user whose name and picture the door's board should show.
set -eu
here="$(cd "$(dirname "$0")/.." && pwd)"
dir="${1:-$here/packaging/redronin/sddm/red-ronin-dojo}"
size="${2:-2560x1440}"
w="${size%x*}"
h="${size#*x}"
preview="${DESK_PREVIEW:-desk-preview}"
if [ -x "$here/build-install/desk-preview" ] && [ -z "${DESK_PREVIEW:-}" ]; then
	preview="$here/build-install/desk-preview"
fi
mkdir -p "$dir/assets"
out=$(DESK_LOCK_MOODS=1 DESK_LOCK_FRAME=9 "$preview" --lock -s "$size" -o "$dir/assets/door.png")
slot=$(printf '%s\n' "$out" | awk '/^slot /{print $2, $3, $4, $5}')
if [ -z "$slot" ]; then
	echo "redronin-greeter: desk-preview gave no slot" >&2
	exit 1
fi
set -- $slot
fx=$(awk -v a="$1" -v w="$w" 'BEGIN{printf "%.5f", a / w}')
fy=$(awk -v a="$2" -v h="$h" 'BEGIN{printf "%.5f", a / h}')
fw=$(awk -v a="$3" -v w="$w" 'BEGIN{printf "%.5f", a / w}')
fh=$(awk -v a="$4" -v h="$h" 'BEGIN{printf "%.5f", a / h}')
conf="$dir/theme.conf"
sed -i -e "s/^slotX=.*/slotX=$fx/" -e "s/^slotY=.*/slotY=$fy/" \
	-e "s/^slotW=.*/slotW=$fw/" -e "s/^slotH=.*/slotH=$fh/" "$conf"
echo "redronin-greeter: $dir/assets/door.png, slot $fx $fy $fw $fh"
