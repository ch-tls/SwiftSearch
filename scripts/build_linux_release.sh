#!/usr/bin/env bash
#
# SwiftSearch — Linux Release Build + Package Script
#
# Usage: ./scripts/build_linux_release.sh [--no-tests] [--no-package]
#
# Prerequisites:
#   - CMake >= 3.16
#   - GCC >= 14 (for C++23)
#   - Ninja
#   - Qt6 (Core, Gui, Widgets, Sql, Concurrent)
#   - (optional) dpkg-dev for .deb packaging
#   - (optional) rpm-build for .rpm packaging

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/cmake-build-release"
BUILD_TYPE=Release
RUN_TESTS=true
RUN_PACKAGE=true
JOBS=$(nproc 2>/dev/null || echo 4)

for arg in "$@"; do
  case "$arg" in
    --no-tests)   RUN_TESTS=false ;;
    --no-package) RUN_PACKAGE=false ;;
    --help)
      echo "Usage: $0 [--no-tests] [--no-package]"
      exit 0
      ;;
  esac
done

echo "========================================"
echo " SwiftSearch — Linux Release Build"
echo "========================================"
echo " Build dir : ${BUILD_DIR}"
echo " Build type: ${BUILD_TYPE}"
echo " Jobs      : ${JOBS}"
echo " Tests     : ${RUN_TESTS}"
echo " Package   : ${RUN_PACKAGE}"
echo "========================================"
echo ""

# ── Configure ──
echo "[1/4] Configuring..."
cmake -B "${BUILD_DIR}" -G Ninja \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
  -DBUILD_TESTS=ON \
  -DSWIFTSEARCH_WERROR=OFF

# ── Build ──
echo ""
echo "[2/4] Building (${JOBS} jobs)..."
cmake --build "${BUILD_DIR}" -j "${JOBS}"

# ── Test ──
if [ "${RUN_TESTS}" = true ]; then
  echo ""
  echo "[3/4] Running tests..."
  ctest --test-dir "${BUILD_DIR}" --output-on-failure
fi

# ── Package ──
if [ "${RUN_PACKAGE}" = true ]; then
  echo ""
  echo "[4/4] Packaging (DEB + TGZ)..."
  cd "${BUILD_DIR}"

  if command -v dpkg &> /dev/null; then
    cpack -G DEB 2>&1
    echo "  → ${BUILD_DIR}/SwiftSearch-*.deb"
  else
    echo "  [SKIP] dpkg not found — skipping .deb"
  fi

  cpack -G TGZ 2>&1
  echo "  → ${BUILD_DIR}/SwiftSearch-*.tar.gz"

  cd "${PROJECT_DIR}"
fi

echo ""
echo "========================================"
echo " Release build complete."
echo " Binary: ${BUILD_DIR}/src/SwiftSearch"
echo "========================================"
