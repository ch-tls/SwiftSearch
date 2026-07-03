#!/usr/bin/env bash
#
# deploy_windows_dlls.sh — 解析 MinGW .exe 的 DLL 导入表，复制运行时依赖到输出目录
#
# Usage: ./scripts/deploy_windows_dlls.sh <path/to/SwiftSearch.exe>
#
# 依赖：
#   x86_64-w64-mingw32-objdump (来自 mingw64-binutils)
#   DLL 位于 /usr/x86_64-w64-mingw32/sys-root/mingw/bin/
#
# 工作原理：
#   1. objdump -p 解析 .exe 的所有 DLL 导入
#   2. 排除系统 DLL（KERNEL32/USER32/msvcrt 等，任何 Windows 都自带）
#   3. 其余 DLL 从 MinGW sys-root 复制
#   4. 递归解析每个 DLL 的传递依赖（同样排除系统 DLL）
#   5. 复制 platforms/qwindows.dll 窗口插件

set -euo pipefail

EXE_PATH="${1:?Usage: $0 <path/to/SwiftSearch.exe>}"
MINGW_ROOT="/usr/x86_64-w64-mingw32/sys-root/mingw"
MINGW_BIN="${MINGW_ROOT}/bin"
MINGW_PLUGINS="${MINGW_ROOT}/lib/qt6/plugins"
DEST_DIR="$(cd "$(dirname "$EXE_PATH")" && pwd)"
OBJDUMP=x86_64-w64-mingw32-objdump

# 白名单：Windows 自带 DLL（无需复制）
SYS_DLL_PATTERN='^(KERNEL32|USER32|GDI32|msvcrt|SHELL32|ole32|ADVAPI32|COMCTL32|'
SYS_DLL_PATTERN+='SHLWAPI|WS2_32|NSI|IPHLPAPI|WINMM|VERSION|OLEAUT32|IMM32|'
SYS_DLL_PATTERN+='SETUPAPI|CFGMGR32|d3d|dxgi|DWrite|UxTheme|RPCRT4|combase|'
SYS_DLL_PATTERN+='bcrypt|crypt32|sechost|sspicli|gdi32full|ucrtbase|wsock32|'
SYS_DLL_PATTERN+='powrprof|winmm|ntdll)'

echo "========================================"
echo " Deploying Windows runtime DLLs"
echo "========================================"
echo " EXE       : ${EXE_PATH}"
echo " Dest dir  : ${DEST_DIR}"
echo " MinGW bin : ${MINGW_BIN}"
echo "========================================"

if [ ! -f "${EXE_PATH}" ]; then
  echo "ERROR: ${EXE_PATH} not found"
  exit 1
fi

if [ ! -d "${MINGW_BIN}" ]; then
  echo "ERROR: MinGW bin dir not found: ${MINGW_BIN}"
  exit 1
fi

copied_count=0
declare -A copied_map

copy_dll() {
  local dll_name="$1"
  local src="${MINGW_BIN}/${dll_name}"
  local dst="${DEST_DIR}/${dll_name}"

  if [ -f "${src}" ] && [ ! -f "${dst}" ]; then
    cp -v "${src}" "${dst}"
    copied_map["${dll_name}"]=1
    copied_count=$((copied_count + 1))
  elif [ ! -f "${src}" ]; then
    echo "  [SKIP] not in MinGW: ${dll_name}"
  fi
}

filter_user_dlls() {
  grep -v -E "${SYS_DLL_PATTERN}" || true
}

resolve_transitive() {
  local dll_path="$1"
  local dll_name
  ${OBJDUMP} -p "${dll_path}" 2>/dev/null \
    | grep "DLL Name" \
    | awk '{print $3}' \
    | filter_user_dlls \
    | while read -r dll_name; do
      if [ -z "${copied_map["${dll_name}"]:-}" ]; then
        copy_dll "${dll_name}"
        if [ -f "${DEST_DIR}/${dll_name}" ]; then
          resolve_transitive "${DEST_DIR}/${dll_name}"
        fi
      fi
    done
}

# ── Step 1: 解析 .exe 导入表，复制所有用户态 DLL ──
echo ""
echo "[1/4] Resolving EXE imports..."

${OBJDUMP} -p "${EXE_PATH}" 2>/dev/null \
  | grep "DLL Name" \
  | awk '{print $3}' \
  | filter_user_dlls \
  | while read -r dll; do
    copy_dll "${dll}"
  done

# ── Step 2: 递归解析传递依赖 ──
echo ""
echo "[2/4] Resolving transitive dependencies..."

# 对每个已复制的 DLL 递归解析
for dll_path in "${DEST_DIR}"/*.dll; do
  if [ -f "${dll_path}" ]; then
    resolve_transitive "${dll_path}"
  fi
done

# ── Step 3: 复制平台插件 ──
echo ""
echo "[3/4] Copying platform plugins..."

PLUGINS_DST="${DEST_DIR}/plugins/platforms"
mkdir -p "${PLUGINS_DST}"

QWINDOWS_SRC="${MINGW_PLUGINS}/platforms/qwindows.dll"
if [ -f "${QWINDOWS_SRC}" ]; then
  cp -v "${QWINDOWS_SRC}" "${PLUGINS_DST}/"
else
  echo "  [WARN] qwindows.dll not found at ${QWINDOWS_SRC}"
fi

# ── Step 4: 复制 SQL 驱动插件 ──
echo ""
echo "[4/4] Copying SQL driver plugins..."

SQLDRIVERS_DST="${DEST_DIR}/plugins/sqldrivers"
mkdir -p "${SQLDRIVERS_DST}"

QSQLITE_SRC="${MINGW_PLUGINS}/sqldrivers/qsqlite.dll"
if [ -f "${QSQLITE_SRC}" ]; then
  cp -v "${QSQLITE_SRC}" "${SQLDRIVERS_DST}/"
else
  echo "  [WARN] qsqlite.dll not found at ${QSQLITE_SRC}"
fi

# ── Summary ──
echo ""
echo "========================================"
echo " Deploy complete — ${copied_count} DLL(s) copied (plugins + sqldrivers)"
echo "========================================"
echo " Output directory: ${DEST_DIR}/"
ls -lh "${DEST_DIR}"/*.dll 2>/dev/null || true
echo ""
ls -lh "${PLUGINS_DST}/" 2>/dev/null || true
echo ""
ls -lh "${SQLDRIVERS_DST}/" 2>/dev/null || true
