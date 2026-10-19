#!/bin/sh
# The fork's Buildroot post-build, run after upstream's (buildroot/board/rgds/post-build.sh) and after both rootfs
# overlays have been copied: changes to upstream's files in the image go here rather than a whole copy in the overlay.
#   post-build.sh <target dir> <board: rgds | rgds-plus | universal>
set -e
