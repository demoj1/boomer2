#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT

# Keep generated protocol files and test executables out of the production build.
protocol_dir=$(pkg-config --variable=pkgdatadir wayland-protocols)
protocol_xml="$protocol_dir/unstable/xdg-output/xdg-output-unstable-v1.xml"
wayland-scanner client-header "$protocol_xml" "$test_dir/xdg-output-unstable-v1-client-protocol.h"
wayland-scanner private-code "$protocol_xml" "$test_dir/xdg-output-protocol.c"
"${CC:-clang}" -c "$test_dir/xdg-output-protocol.c" -o "$test_dir/protocol.o"
read -r -a test_flags <<< "${CXXFLAGS:--O3 -march=native}"
compile=("${CXX:-clang++}" -std=c++2b "${test_flags[@]}" -DWAYLAND_SCREENSHOT -I"$test_dir")
libs=("$test_dir/protocol.o" -lraylib -lwayland-client)

"${compile[@]}" tests/selection_test.cpp src/platform.cpp "${libs[@]}" -o "$test_dir/selection-test"
"$test_dir/selection-test"

cat > "$test_dir/grim" <<'GRIM'
#!/usr/bin/env bash
set -eu
# Verify that the real capture helper requests uncompressed PNG on stdout.
[[ "$*" == '-l 0 -' || "$*" == '-l 0 -o DP-1 -' ]] || exit 2
case "${BOOMER_TEST_CAPTURE:-}" in
  failure) exit 1 ;;
  invalid) printf 'not a PNG' ;;
  *) cat "$BOOMER_TEST_IMAGE" ;;
esac
GRIM
chmod +x "$test_dir/grim"
"${compile[@]}" tests/capture_test.cpp "${libs[@]}" -o "$test_dir/capture-test"
PATH="$test_dir:$PATH" BOOMER_TEST_IMAGE="$test_dir/capture.png" "$test_dir/capture-test"

if [[ "${BOOMER_TEST_GPU:-0}" == 1 ]]; then
  "${compile[@]}" tests/blur_test.cpp src/platform.cpp "${libs[@]}" -o "$test_dir/blur-test"
  "$test_dir/blur-test"
fi
