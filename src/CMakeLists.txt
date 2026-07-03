# ── Model layer: IndexEngine ──
add_library(swiftsearch_model STATIC
  model/file_entry.h
  model/file_scanner.h
  model/file_scanner.cpp
  model/index_database.h
  model/index_database.cpp
  model/index_engine.h
  model/index_engine.cpp
  model/indexing_observer.h
  model/platform_handles.h
)
target_include_directories(swiftsearch_model PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(swiftsearch_model PUBLIC Qt6::Core Qt6::Sql)

# ── Controller layer: SearchService ──
add_library(swiftsearch_controller STATIC
  controller/search_types.h
  controller/search_service.h
  controller/search_service.cpp
  controller/search_observer.h
  controller/query_parser.h
  controller/query_parser.cpp
  controller/result_ranker.h
  controller/result_ranker.cpp
  controller/file_command.h
  controller/file_command.cpp
  controller/command_history.h
  controller/command_history.cpp
)
target_include_directories(swiftsearch_controller PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(swiftsearch_controller PUBLIC swiftsearch_model Qt6::Core Qt6::Widgets Qt6::Concurrent)

# ── View layer: UI ──
add_library(swiftsearch_view STATIC
  view/main_window.h
  view/main_window.cpp
  view/search_bar_widget.h
  view/search_bar_widget.cpp
  view/result_list_widget.h
  view/result_list_widget.cpp
  view/settings_dialog.h
  view/settings_dialog.cpp
)
target_include_directories(swiftsearch_view PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(swiftsearch_view
  PUBLIC
    swiftsearch_controller
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

# ── Utility layer ──
add_library(swiftsearch_util STATIC
  util/sort_util.h
  util/log_manager.h
  util/log_manager.cpp
)
target_include_directories(swiftsearch_util PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(swiftsearch_util PUBLIC Qt6::Core)

# ── Executable ──
add_executable(SwiftSearch
  "${CMAKE_SOURCE_DIR}/main.cpp"
  "${RESOURCE_FILE}"
)
target_link_libraries(SwiftSearch PRIVATE swiftsearch_view swiftsearch_util ${PLATFORM_LIBS})

add_dependencies(SwiftSearch translations)

# ── Windows DLL deploy (MinGW cross-compilation only) ──
if(MINGW)
  add_custom_command(TARGET SwiftSearch POST_BUILD
    COMMAND "${CMAKE_SOURCE_DIR}/scripts/deploy_windows_dlls.sh"
            "$<TARGET_FILE:SwiftSearch>"
    COMMENT "Deploying Qt6 Windows runtime DLLs"
    VERBATIM
  )
endif()

swiftsearch_enforce_warnings(swiftsearch_model)
swiftsearch_enforce_warnings(swiftsearch_controller)
swiftsearch_enforce_warnings(swiftsearch_view)
swiftsearch_enforce_warnings(swiftsearch_util)
swiftsearch_enforce_warnings(SwiftSearch)
