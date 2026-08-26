#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

# --- Configuration ---
ITCH_USER="robspsj"
ITCH_GAME="fractory"
VERSION=$(cat VERSION | tr -d '[:space:]')
NATIVE_BUILD_DIR="build-native"   # Skia/SDL2 native (Ninja)
WEB_BUILD_DIR="build-wasm2"       # Skia/SDL2 web (Emscripten)
# Emscripten needs python3.14 on PATH (homebrew wrapper picks Xcode's py3.9)
export PATH="/opt/homebrew/opt/python@3.14/bin:$PATH"

ALL_PLATFORMS=(osx html5)
DRY_RUN=false

# --- Parse args ---
PLATFORMS=()
for arg in "$@"; do
  case "$arg" in
    --dry-run) DRY_RUN=true ;;
    *) PLATFORMS+=("$arg") ;;
  esac
done
if [ ${#PLATFORMS[@]} -eq 0 ]; then
  PLATFORMS=("${ALL_PLATFORMS[@]}")
fi

# --- Preflight ---
if ! command -v butler &>/dev/null; then
  echo "Error: butler not found. Install from https://itch.io/docs/butler/installing.html"; exit 1
fi
if ! butler status "${ITCH_USER}/${ITCH_GAME}" &>/dev/null 2>&1; then
  echo "Error: not logged in to butler. Run 'butler login' first."; exit 1
fi

echo "==> Fractory v${VERSION}"
echo "==> Target: ${ITCH_USER}/${ITCH_GAME}"
echo "==> Platforms: ${PLATFORMS[*]}"
$DRY_RUN && echo "==> DRY RUN — nothing will be uploaded"
echo ""

build_web() {
  echo "--- Building web (Emscripten / Skia / SDL2) ---"
  if ! command -v emcmake &>/dev/null; then
    echo "Error: emcmake not found. Install Emscripten first."; return 1
  fi
  emcmake cmake -B "$WEB_BUILD_DIR" -S . >/dev/null
  cmake --build "$WEB_BUILD_DIR" >/dev/null
  echo "    Built: $WEB_BUILD_DIR/"
}
push_web() {
  echo "--- Pushing web → ${ITCH_USER}/${ITCH_GAME}:html5 ---"
  if $DRY_RUN; then
    echo "    [dry-run] butler push $WEB_BUILD_DIR ${ITCH_USER}/${ITCH_GAME}:html5 --userversion-file VERSION"
  else
    butler push "$WEB_BUILD_DIR" "${ITCH_USER}/${ITCH_GAME}:html5" --userversion-file VERSION
  fi
}
build_osx() {
  echo "--- Building macOS (native) ---"
  cmake -B "$NATIVE_BUILD_DIR" -S . >/dev/null
  cmake --build "$NATIVE_BUILD_DIR" >/dev/null
  local pkg_dir="build/osx-pkg"
  rm -rf "$pkg_dir"; mkdir -p "$pkg_dir"
  cp "$NATIVE_BUILD_DIR/fractory" "$pkg_dir/fractory"; chmod +x "$pkg_dir/fractory"
  [ -f "$pkg_dir/fractory" ] && echo "    Packaged: $pkg_dir/fractory" || { echo "Error: no binary"; return 1; }
}
push_osx() {
  echo "--- Pushing macOS → ${ITCH_USER}/${ITCH_GAME}:osx ---"
  if $DRY_RUN; then
    echo "    [dry-run] butler push build/osx-pkg ${ITCH_USER}/${ITCH_GAME}:osx --userversion-file VERSION"
  else
    butler push build/osx-pkg "${ITCH_USER}/${ITCH_GAME}:osx" --userversion-file VERSION
  fi
}

for platform in "${PLATFORMS[@]}"; do
  case "$platform" in
    html5) build_web; push_web; echo "" ;;
    osx)   build_osx; push_osx; echo "" ;;
    linux|windows)
      echo "--- Skipping $platform (cross-build not supported locally; use CI) ---"; echo "" ;;
    *) echo "Unknown platform: $platform"; echo "Supported: osx html5 linux windows"; exit 1 ;;
  esac
done

echo "==> Done."