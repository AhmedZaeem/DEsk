#!/bin/sh
# install_redronin.sh -- DEsk Red Ronin on Arch Linux, in one go.
#
# Run it as yourself from the folder you unpacked the release in; it asks for
# sudo when it needs to. It:
#   1. installs DEsk with scripts/install_arch.sh: what DEsk needs, wlroots
#      0.18 built here with Red Ronin's explicit-sync patch (unless a wlroots
#      0.18 from a repository is installed, which is used as it is: then
#      explicit sync is simply not offered), DEsk itself, and "DEsk" on the
#      login screen;
#   2. gives you Red Ronin's settings (packaging/redronin: shell.conf and the
#      launcher's fuzzel.ini) in ~/.config/desk -- unless you have your own
#      there already, which are left alone;
#   3. with --sddm, the dojo's gate as the login screen (SDDM theme
#      red-ronin-dojo), with your own name on its board, and SDDM set to it.
#
#   --sddm        also install and select the dojo login theme (SDDM)
#   --no-config   leave ~/.config/desk as it is
#   anything else is passed to install_arch.sh: --yes, --no-apps, --bare ...
#
# When it is done, log out and pick "DEsk" at the login screen.
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
sddm=0
config=1
pass=""
for a in "$@"; do
	case "$a" in
	--sddm) sddm=1 ;;
	--no-config) config=0 ;;
	--help | -h)
		sed -n '2,22p' "$0" | sed 's/^# \{0,1\}//'
		exit 0
		;;
	*) pass="$pass $a" ;;
	esac
done

say() { printf '\n== %s\n' "$*"; }

say "DEsk, built and installed (scripts/install_arch.sh)"
# (this copy's files were checked against SHA256SUMS by install_arch.sh itself)
# shellcheck disable=SC2086
"$root/scripts/install_arch.sh" $pass

if [ "$config" = 1 ]; then
	say "Red Ronin's settings"
	d="${XDG_CONFIG_HOME:-$HOME/.config}/desk"
	mkdir -p "$d"
	for f in shell.conf fuzzel.ini; do
		if [ -e "$d/$f" ]; then
			echo "kept your own $d/$f (Red Ronin's is packaging/redronin/$f, to compare)"
		else
			cp "$root/packaging/redronin/$f" "$d/$f"
			echo "put $d/$f"
		fi
	done
fi

if [ "$sddm" = 1 ]; then
	say "The dojo's gate as the login screen (SDDM)"
	theme="$root/packaging/redronin/sddm/red-ronin-dojo"
	tmp=$(mktemp -d)
	trap 'rm -rf "$tmp"' EXIT
	cp -r "$theme" "$tmp/"
	# your name on the board, if desk-preview (installed with DEsk) can draw it
	if command -v desk-preview >/dev/null 2>&1; then
		DESK_PREVIEW=desk-preview "$root/scripts/redronin-greeter.sh" "$tmp/red-ronin-dojo" \
			|| echo "(could not draw it with your name: the board says 浪人 instead)"
	fi
	sudo rm -rf /usr/share/sddm/themes/red-ronin-dojo
	sudo cp -r "$tmp/red-ronin-dojo" /usr/share/sddm/themes/
	conf=/etc/sddm.conf.d/10-red-ronin.conf
	sudo mkdir -p /etc/sddm.conf.d
	if [ -f "$conf" ]; then
		sudo cp "$conf" "$conf.bak-$(date +%Y%m%d)"
		if grep -q '^Current=' "$conf"; then
			sudo sed -i 's/^Current=.*/Current=red-ronin-dojo/' "$conf"
		else
			printf '\n[Theme]\nCurrent=red-ronin-dojo\n' | sudo tee -a "$conf" >/dev/null
		fi
	else
		printf '[Theme]\nCurrent=red-ronin-dojo\n' | sudo tee "$conf" >/dev/null
	fi
	echo "SDDM will show the dojo's gate from the next time it starts."
fi

say "Done. Log out and pick DEsk at the login screen."
