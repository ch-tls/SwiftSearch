#!/usr/bin/env bash
#
# package_windows.sh — 从 cmake-build-release-win/src/ 提取运行时文件打 ZIP
#
# 保持用户已验证的文件结构不变，排除构建中间产物。
# 输出: cmake-build-release-win/SwiftSearch-1.0.0-win64.zip

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
SRC_DIR="${PROJECT_DIR}/cmake-build-release-win/src"
PKG_NAME="SwiftSearch-1.0.0-win64"
STAGING="${PROJECT_DIR}/cmake-build-release-win/${PKG_NAME}"
ZIP_FILE="${PROJECT_DIR}/cmake-build-release-win/${PKG_NAME}.zip"

echo "========================================"
echo " SwiftSearch — Windows Package (ZIP)"
echo "========================================"
echo " Source dir  : ${SRC_DIR}"
echo " Staging     : ${STAGING}"
echo " Output      : ${ZIP_FILE}"
echo "========================================"

if [ ! -f "${SRC_DIR}/SwiftSearch.exe" ]; then
  echo "ERROR: SwiftSearch.exe not found in ${SRC_DIR}"
  exit 1
fi

rm -rf "${STAGING}"
mkdir -p "${STAGING}"

echo ""
echo "[1/4] Copying SwiftSearch.exe..."
cp "${SRC_DIR}/SwiftSearch.exe" "${STAGING}/"

echo "[2/4] Copying runtime DLLs..."
cp "${SRC_DIR}"/*.dll "${STAGING}/" 2>/dev/null || true

echo "[3/4] Copying plugin directories..."
for plug_dir in platforms plugins sqldrivers; do
  if [ -d "${SRC_DIR}/${plug_dir}" ]; then
    cp -r "${SRC_DIR}/${plug_dir}" "${STAGING}/${plug_dir}"
  fi
done

echo "[4/4] Creating ZIP..."
cd "${PROJECT_DIR}/cmake-build-release-win"
rm -f "${ZIP_FILE}"
zip -r "${ZIP_FILE}" "${PKG_NAME}/"

echo ""
echo "========================================"
echo " Package created successfully!"
echo " $(du -h "${ZIP_FILE}" | cut -f1)  ${ZIP_FILE}"
echo ""
echo " Contents:"
unzip -l "${ZIP_FILE}" | tail -n +4 | head -n -2
echo "========================================"

rm -rf "${STAGING}"
