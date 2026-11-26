#!/bin/bash
# redronin-install-local.sh -- install this checkout's DEsk on this machine,
# and the dojo login: what the Red Ronin rounds have been installed with.
#   - the build in build-install, and DEsk's wlroots if it was patched here
#   - the stale /usr/local session entry (it shadows /usr/share's) removed
#   - the SDDM theme red-ronin-dojo, and SDDM set to it (the old setting kept
#     next to it as .bak-<date>)
# Run it in a terminal of its own; it asks for the sudo password.
# The result goes to $XDG_RUNTIME_DIR/desk-install-result as well.
set -u
cd "$(dirname "$0")/.." || exit 1
R="${XDG_RUNTIME_DIR:-/tmp}/desk-install-result"
rm -f "$R"
echo "== DEsk + dojo login install (needs your sudo password) =="
PATH=/opt/desk-wlroots/bin:$PATH ninja -C build-install || { echo "build failed" | tee "$R"; read -r; exit 1; }
wlr=build-install-wlroots/wlroots-0.18.3/_build
conf=/etc/sddm.conf.d/10-red-ronin.conf
ok=1
if [ -d "$wlr" ] && ! cmp -s "$wlr/libwlroots-0.18.so" /opt/desk-wlroots/lib/libwlroots-0.18.so; then
	sudo meson install -C "$wlr" --no-rebuild --quiet || ok=0
fi
[ $ok = 1 ] && sudo meson install -C build-install --no-rebuild --quiet || ok=0
[ $ok = 1 ] && sudo rm -f /usr/local/share/wayland-sessions/desk.desktop || ok=0
if [ $ok = 1 ] && [ -d packaging/redronin/sddm/red-ronin-dojo ]; then
	sudo rm -rf /usr/share/sddm/themes/red-ronin-dojo \
		&& sudo cp -r packaging/redronin/sddm/red-ronin-dojo /usr/share/sddm/themes/ || ok=0
	if [ $ok = 1 ] && [ -f "$conf" ] && ! grep -q '^Current=red-ronin-dojo$' "$conf"; then
		sudo cp "$conf" "$conf.bak-$(date +%Y%m%d)" \
			&& sudo sed -i 's/^Current=.*/Current=red-ronin-dojo/' "$conf" || ok=0
	fi
fi
{
	echo "install exit=$((1 - ok))"
	cmp -s build-install/desk /usr/local/bin/desk && echo "desk matches the build"
	[ -x /usr/local/bin/desk-power-menu ] && echo "power menu installed"
	grep -q '^Current=red-ronin-dojo$' "$conf" 2>/dev/null && echo "sddm theme: red-ronin-dojo"
} | tee "$R"
echo "Done. Log out and back in for the new DEsk. Press Enter to close."
read -r
