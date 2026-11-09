#!/bin/sh
set -eu
cd -- "$(dirname -- "$0")/../.."
exec sudo apt install ./apt_dist/lintian-fixed-139/double-tap-switcher_1.39-0ubuntu1_amd64.deb
