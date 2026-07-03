# ============================================================================
# SwiftSearch — CMake 构建系统
# ============================================================================
# 项目：高性能跨平台文件搜索工具
# 语言：C++23 + Qt6 (Core/Gui/Widgets/Sql/Concurrent)
# 平台：Linux (GCC)、Windows (MSVC/MinGW)
# 编译：Ninja 或 Visual Studio
# 测试：Google Test (GTest)
# 打包：CPack (DEB/RPM/TGZ for Linux; NSIS/ZIP for Windows)
# ============================================================================

cmake_minimum_required(VERSION 3.16)

project(SwiftSearch
  VERSION 1.0.0
  DESCRIPTION "High-performance file search tool"
  LANGUAGES CXX
)

# ── C++23 标准 ──
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── IDE / 工具链辅助 ──
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # 生成 compile_commands.json 供 clangd 等工具使用
set(CMAKE_POSITION_INDEPENDENT_CODE ON) # 启用 PIC（静态库间链接必需）

# ── Qt6 自动处理（MOC/RCC/UIC） ──
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── 自定义 CMake 模块（编译警告选项、平台配置） ──
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(PlatformConfig)

# ── Qt6 依赖项（Core、Gui、Widgets、Sql(ite)、Concurrent） ──
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Sql Concurrent)

# ── 编译翻译文件 (.ts → .qm) ──
find_program(LRELEASE_QT6 lrelease-qt6 REQUIRED)
set(TRANSLATION_TS "${CMAKE_SOURCE_DIR}/resources/translations/swiftsearch_zh_CN.ts")
set(TRANSLATION_QM "${CMAKE_SOURCE_DIR}/resources/translations/swiftsearch_zh_CN.qm")

add_custom_command(
  OUTPUT "${TRANSLATION_QM}"
  COMMAND "${LRELEASE_QT6}" "${TRANSLATION_TS}" -qm "${TRANSLATION_QM}"
  DEPENDS "${TRANSLATION_TS}"
  COMMENT "Compiling translation: swiftsearch_zh_CN"
)

add_custom_target(translations ALL DEPENDS "${TRANSLATION_QM}")

# ── Resources ──
set(RESOURCE_FILE "${CMAKE_SOURCE_DIR}/resources/resources.qrc")

# ── 源文件子目录 ──
add_subdirectory(src)

# ── 单元测试（通过 BUILD_TESTS 选项控制） ──
option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
  enable_testing()
  find_package(GTest QUIET)
  if(NOT GTest_FOUND)
    # 如果系统未安装 GTest，从 GitHub 拉取 v1.15.2
    # 临时关闭 Qt 自动工具，避免 FetchContent 干扰 MOC
    set(CMAKE_AUTOMOC OFF)
    set(CMAKE_AUTORCC OFF)
    set(CMAKE_AUTOUIC OFF)
    include(FetchContent)
    FetchContent_Declare(googletest
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG v1.15.2
    )
    FetchContent_MakeAvailable(googletest)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)
  endif()
  add_subdirectory(tests)
endif()

# ── 安装目标 ──
include(GNUInstallDirs)
install(TARGETS SwiftSearch
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  BUNDLE  DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES ${RESOURCE_FILE}
  DESTINATION ${CMAKE_INSTALL_DATADIR}/swiftsearch/resources
  OPTIONAL
)

# ── CPack 打包配置 ──
# Linux: DEB（Debian/Ubuntu）、RPM（Fedora/RHEL）、TGZ（通用）
# Windows: NSIS 安装程序、ZIP 压缩包
set(CPACK_PACKAGE_NAME "SwiftSearch")
set(CPACK_PACKAGE_VENDOR "SwiftSearch")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
if(EXISTS "${CMAKE_SOURCE_DIR}/LICENSE")
  set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/README.md")
  set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")
endif()

if(WIN32)
  set(CPACK_GENERATOR "NSIS;ZIP")
  set(CPACK_NSIS_DISPLAY_NAME "SwiftSearch")
  set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
elseif(UNIX AND NOT APPLE)
  set(CPACK_GENERATOR "DEB;RPM;TGZ")
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "SwiftSearch")

  set(CPACK_RPM_PACKAGE_SUMMARY "High-performance file search tool")
  set(CPACK_RPM_PACKAGE_DESCRIPTION
    "SwiftSearch is a cross-platform desktop file search utility. "
    "It indexes file systems using SQLite and provides fast full-text "
    "and metadata search with relevance ranking.")
  set(CPACK_RPM_PACKAGE_GROUP "Applications/File")
  set(CPACK_RPM_PACKAGE_LICENSE "MIT")
  set(CPACK_RPM_PACKAGE_URL "https://github.com/anomalyco/SwiftSearch")
  set(CPACK_RPM_PACKAGE_RELEASE "1")
  set(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")
  set(CPACK_RPM_PACKAGE_AUTOREQ ON)
  set(CPACK_RPM_PACKAGE_AUTOPROV ON)
endif()

include(CPack)

# ── Qt 部署辅助工具 ──
# Windows: windeployqt 自动拷贝所需 Qt DLL 和插件
# macOS: macdeployqt 打包 .app bundle 及其依赖
if(WIN32)
  find_program(WINDEPLOYQT_EXECUTABLE windeployqt
    HINTS ${_qt6_install_prefix}/bin
  )
  if(WINDEPLOYQT_EXECUTABLE)
    install(CODE "
      execute_process(COMMAND \"${WINDEPLOYQT_EXECUTABLE}\"
        --no-translations
        --no-compiler-runtime
        \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/$<TARGET_FILE_NAME:SwiftSearch>\"
      )
    ")
  endif()
elseif(APPLE)
  find_program(MACDEPLOYQT_EXECUTABLE macdeployqt
    HINTS ${_qt6_install_prefix}/bin
  )
  if(MACDEPLOYQT_EXECUTABLE)
    install(CODE "
      execute_process(COMMAND \"${MACDEPLOYQT_EXECUTABLE}\"
        \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/SwiftSearch.app\"
        -always-overwrite
      )
    ")
  endif()
endif()
