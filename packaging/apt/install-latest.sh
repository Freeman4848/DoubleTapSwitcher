#!/bin/sh
set -eu
cd -- "$(dirname -- "$0")/../.."
exec sudo apt install ./apt_dist/final-ui-139/double-tap-switcher_1.39-0ubuntu2_amd64.deb
