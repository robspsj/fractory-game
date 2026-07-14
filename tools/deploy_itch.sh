#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

# --- Configuration ---
ITCH_USER="robspsj"
ITCH_GAME="fractory"
VERSION_FILE="VERSION"
VERSION=$(cat "$VERSION_FILE" | tr -d '[:space:]')

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

# --- Preflight checks ---
if ! command -v butler &>/dev/null; then
  echo "Error: butler not found. Install from https://itch.io/docs/butler/installing.html"
  exit 1
fi

if ! butler status "${ITCH_USER}/${ITCH_GAME}" &>/dev/null 2>&1; then
  echo "Error: not logged in to butler. Run 'butler login' first."
  exit 1
fi

echo "==> Fractory v${VERSION}"
echo "==> Target: ${ITCH_USER}/${ITCH_GAME}"
echo "==> Platforms: ${PLATFORMS[*]}"
if $DRY_RUN; then
  echo "==> DRY RUN — nothing will be uploaded"
fi
echo ""

# --- Build & push functions ---

build_web() {
  echo "--- Building web (Emscripten) ---"
  if ! command -v emcmake &>/dev/null; then
    echo "Error: emcmake not found. Install Emscripten first."
    return 1
  fi
  emcmake cmake -B build/web -S . &>/dev/null
  cmake --build build/web &>/dev/null
  echo "    Built: build/web/"
}

push_web() {
  local channel="html5"
  local dir="build/web"
  echo "--- Pushing web → ${ITCH_USER}/${ITCH_GAME}:${channel} ---"
  if $DRY_RUN; then
    echo "    [dry-run] butler push $dir ${ITCH_USER}/${ITCH_GAME}:${channel} --userversion-file $VERSION_FILE"
  else
    butler push "$dir" "${ITCH_USER}/${ITCH_GAME}:${channel}" --userversion-file "$VERSION_FILE"
  fi
}

build_osx() {
  echo "--- Building macOS (native) ---"
  cmake -B build/native -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=0 &>/dev/null
  cmake --build build/native &>/dev/null

  local pkg_dir="build/osx-pkg"
  rm -rf "$pkg_dir"
  mkdir -p "$pkg_dir"
  cp build/native/fractory "$pkg_dir/fractory"
  chmod +x "$pkg_dir/fractory"

  if [ -f "$pkg_dir/fractory" ]; then
    echo "    Packaged: $pkg_dir/fractory"
  else
    echo "Error: build failed — binary not found"
    return 1
  fi
}

push_osx() {
  local channel="osx"
  local dir="build/osx-pkg"
  echo "--- Pushing macOS → ${ITCH_USER}/${ITCH_GAME}:${channel} ---"
  if $DRY_RUN; then
    echo "    [dry-run] butler push $dir ${ITCH_USER}/${ITCH_GAME}:${channel} --userversion-file $VERSION_FILE"
  else
    butler push "$dir" "${ITCH_USER}/${ITCH_GAME}:${channel}" --userversion-file "$VERSION_FILE"
  fi
}

# --- Main loop ---
for platform in "${PLATFORMS[@]}"; do
  case "$platform" in
    html5)
      build_web
      push_web
      echo ""
      ;;
    osx)
      build_osx
      push_osx
      echo ""
      ;;
    linux|windows)
      echo "--- Skipping $platform ---"
      echo "    Cross-compilation not supported locally."
      echo "    Use CI (GitHub Actions) or build on the target OS."
      echo ""
      ;;
    *)
      echo "Unknown platform: $platform"
      echo "Supported: osx html5 linux windows"
      exit 1
      ;;
  esac
done

echo "==> Done."
