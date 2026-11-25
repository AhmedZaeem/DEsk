#!/bin/sh
# Build this tree in the Docker build image (desk:dev), install it into a
# copy of the runtime image (desk:baseline, from the Dockerfile at the top)
# and run headless.sh there (or layershell.sh: run.sh layershell). Pictures
# and logs land in out/redronin.
#   docker build --target build -t desk:dev . && docker build -t desk:baseline .
set -eu
root="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$root/out/redronin" "$root/out/cache"
rm -rf "$root/out/destdir"
docker run --rm -u "$(id -u):$(id -g)" -v "$root:/work" -w /work desk:dev sh -c '
	[ -d build-docker ] || meson setup build-docker --buildtype=debugoptimized
	meson configure build-docker -Dprefix=/usr -Dsysconfdir=/etc
	meson compile -C build-docker && meson test -C build-docker --print-errorlogs \
		&& DESTDIR=/work/out/destdir meson install -C build-docker --quiet'
docker run --rm -v "$root:/work" -v "$root/out/redronin:/out" \
	-v "$root/out/cache:/tmp/desk-cache" desk:baseline \
	sh -c 'cp -a /work/out/destdir/usr/. /usr/ && cp /work/scripts/desk-ctl /usr/bin/desk-ctl \
		&& chmod +x /usr/bin/desk-ctl && sh /work/tests/redronin/'"${1:-headless}"'.sh'
