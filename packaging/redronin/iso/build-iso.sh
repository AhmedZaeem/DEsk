#!/bin/bash
# build-iso.sh -- the Red Ronin live/install ISO, with archiso.
#
# Run as root in an Arch Linux system or a privileged container:
#   docker run --rm --privileged -v "$PWD":/src -v /path/to/pkgs:/pkgs \
#       -v /path/to/out:/out archlinux:latest /src/packaging/redronin/iso/build-iso.sh
# /pkgs holds the desk-redronin and calamares packages (packaging/redronin/arch
# and the AUR's calamares, built with makepkg); the ISO goes to /out.
#
# It is Arch's releng profile (the official install ISO), with:
#   - DEsk · Red Ronin and Calamares from a local repository made of /pkgs;
#   - SDDM with the dojo's gate, logging a live user (ronin) into DEsk;
#   - "Install Red Ronin" in the drawer: Calamares, copying the live system
#     to the disk (packaging/redronin/iso/calamares).
set -euo pipefail
src=${SRC:-/src}
pkgs=${PKGS:-/pkgs}
out=${OUT:-/out}
iso=$src/packaging/redronin/iso
work=/tmp/redronin-iso
rm -rf "$work"
mkdir -p "$work/repo"

pacman -Syu --noconfirm --needed archiso >/dev/null
cp -r /usr/share/archiso/configs/releng "$work/profile"
p=$work/profile

# the local repository
cp "$pkgs"/*.pkg.tar.zst "$work/repo/"
repo-add -q "$work/repo/redronin.db.tar.gz" "$work/repo/"*.pkg.tar.zst
cat >> "$p/pacman.conf" <<EOF

[redronin]
SigLevel = Optional TrustAll
Server = file://$work/repo
EOF

# what goes on it, besides releng's own
cat >> "$p/packages.x86_64" <<'EOF'
desk-redronin
calamares
sddm
qt6-declarative
qt6-svg
qt6-wayland
xorg-server
kitty
fuzzel
firefox
thunar
mousepad
networkmanager
network-manager-applet
pipewire
pipewire-pulse
wireplumber
vulkan-intel
vulkan-radeon
mesa-utils
grub
efibootmgr
os-prober
mate-polkit
gnome-keyring
bluez
bluez-utils
blueman
adwaita-icon-theme
noto-fonts-cjk
EOF

# the live system's files, and Calamares' config
cp -a "$iso/airootfs/." "$p/airootfs/"
mkdir -p "$p/airootfs/etc/calamares"
cp -a "$iso/calamares/." "$p/airootfs/etc/calamares/"

# services: SDDM as the display manager, NetworkManager (not releng's
# systemd-networkd and iwd), and the live user made before SDDM starts
s=$p/airootfs/etc/systemd/system
rm -f "$s/multi-user.target.wants/systemd-networkd.service" \
	"$s/multi-user.target.wants/iwd.service" \
	"$s/multi-user.target.wants/systemd-resolved.service" \
	"$s/sockets.target.wants/systemd-networkd.socket" \
	"$s/network-online.target.wants/systemd-networkd-wait-online.service" \
	"$s/systemd-networkd-wait-online.service.wants"/* 2>/dev/null || true
mkdir -p "$s/multi-user.target.wants"
ln -sf /usr/lib/systemd/system/sddm.service "$s/display-manager.service"
ln -sf /usr/lib/systemd/system/NetworkManager.service "$s/multi-user.target.wants/NetworkManager.service"
ln -sf ../redronin-live.service "$s/multi-user.target.wants/redronin-live.service"

# its name, and the scripts' permissions
sed -i \
	-e 's/^iso_name=.*/iso_name="redronin"/' \
	-e 's/^iso_label=.*/iso_label="REDRONIN_$(date --date="@${SOURCE_DATE_EPOCH:-$(date +%s)}" +%Y%m)"/' \
	-e 's/^iso_publisher=.*/iso_publisher="Red Ronin <https:\/\/github.com\/AhmedZaeem\/DEsk>"/' \
	-e 's/^iso_application=.*/iso_application="Red Ronin live and install medium"/' \
	"$p/profiledef.sh"
sed -i 's|^file_permissions=(|file_permissions=(\n  ["/usr/local/bin/redronin-live-setup"]="0:0:755"\n  ["/usr/local/bin/redronin-install"]="0:0:755"|' "$p/profiledef.sh"

# the boot menus say what it is
grep -rl 'Arch Linux install medium' "$p/efiboot" "$p/syslinux" "$p/grub" 2>/dev/null \
	| xargs -r sed -i 's/Arch Linux install medium/Red Ronin live and install medium/g'

mkdir -p "$out"
mkarchiso -v -w "$work/work" -o "$out" "$p"
ls -la "$out"
