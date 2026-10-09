#!/bin/bash
# Re-vendors libical into components/libical/libical/ from an upstream tag.
#
#   components/libical/vendor.sh [tag]        (default: the tag in UPSTREAM.md)
#
# Needs: git, cmake >= 3.20 (set CMAKE=/path/to/cmake if the default one is older), perl, a C compiler (the build is only run to produce the generated files).
# What it does: clones the tag, configures a static build without glib/ICU/C++/docs/tests, builds the `ical`
# target (this runs libical's perl generators), and copies exactly the files this component compiles - the
# sources, the headers they include, and the generated files (icalderived*.c/.h, icalrestriction.c,
# icaltime_p.h) - unmodified. config.h is NOT copied: port/config.h is ours. Finally it prints the SHA-256 of
# the vendored tree for UPSTREAM.md.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
TAG="${1:-v4.0.6}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

git clone --quiet --branch "$TAG" --depth 1 https://github.com/libical/libical.git "$WORK/src"
COMMIT="$(git -C "$WORK/src" rev-parse HEAD)"
"${CMAKE:-cmake}" -S "$WORK/src" -B "$WORK/build" -DCMAKE_BUILD_TYPE=Release -DLIBICAL_STATIC=ON -DLIBICAL_GLIB=OFF \
    -DLIBICAL_CXX_BINDINGS=OFF -DLIBICAL_JAVA_BINDINGS=OFF -DLIBICAL_BUILD_TESTING=OFF -DLIBICAL_BUILD_EXAMPLES=OFF \
    -DLIBICAL_BUILD_DOCS=OFF -DLIBICAL_GOBJECT_INTROSPECTION=OFF -DLIBICAL_ENABLE_BUILTIN_TZDATA=OFF \
    -DCMAKE_DISABLE_FIND_PACKAGE_ICU=TRUE > "$WORK/cmake.log"
"${CMAKE:-cmake}" --build "$WORK/build" --target ical -j4 > "$WORK/build.log"

S="$WORK/src/src/libical"
G="$WORK/build/src/libical"
OUT="$HERE/libical"

SRCS="icalarray icalattach icalcomponent icalenumarray icalenums icalerror_p icalerror icallimits icalmemory \
icalparameter icalparser icalproperty icalrecur icalstrarray icaltime_p icaltime icaltimezone_p icaltimezone \
icalduration icalperiod icaltypes icalvalue icalpvl_p qsort_gen icallangbind_p icaldate_p byref"
GEN_SRCS="icalderivedproperty icalderivedparameter icalderivedvalue icalrestriction"
HDRS="icalarray icalattach icalattachimpl icalcomponent icaldate_p icalduration icalenumarray icalenums icalerror \
icalerror_p icallangbind_p icallimits icalmemory icalparameter icalparameterimpl icalparser icalperiod icalproperty \
icalproperty_p icalpvl_p icalrecur icalrestriction icalstrarray icaltime icaltimezone icaltimezone_p \
icaltimezoneimpl icaltypes icaltypes_p icalvalue icalvalueimpl libical_ical_export libical_sentinel qsort_gen"
GEN_HDRS="icalderivedparameter icalderivedproperty icalderivedvalue icaltime_p"

rm -rf "$OUT"
mkdir -p "$OUT"
for f in $SRCS; do cp "$S/$f.c" "$OUT/"; done
for f in $HDRS; do cp "$S/$f.h" "$OUT/"; done
for f in $GEN_SRCS; do cp "$G/$f.c" "$OUT/"; done
for f in $GEN_HDRS; do cp "$G/$f.h" "$OUT/"; done
cp "$WORK/src/LICENSE.txt" "$HERE/LICENSE.txt"
mkdir -p "$HERE/LICENSES"
cp "$WORK/src/LICENSES/MPL-2.0.txt" "$HERE/LICENSES/MPL-2.0.txt"
cp "$WORK/src/LICENSES/LGPL-2.1-only.txt" "$HERE/LICENSES/LGPL-2.1-only.txt"

echo "tag:    $TAG"
echo "commit: $COMMIT"
echo "files:  $(ls "$OUT" | wc -l)"
echo "sha256 of the vendored tree (sorted file hashes):"
(cd "$OUT" && find . -type f | sort | xargs sha256sum | sha256sum | cut -d' ' -f1)
