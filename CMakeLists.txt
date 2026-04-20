cmake_minimum_required(VERSION 3.16)
project(multigauge_web LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(STATIC_WASM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/wasm")
set(MULTIGAUGE_CORE_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/external/multigauge-core"
  CACHE PATH "Path to the multigauge-core checkout"
)

file(MAKE_DIRECTORY "${STATIC_WASM_DIR}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${STATIC_WASM_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${STATIC_WASM_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${STATIC_WASM_DIR}")

if (NOT EXISTS "${MULTIGAUGE_CORE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR
    "multigauge-core not found at '${MULTIGAUGE_CORE_DIR}'.\n"
    "Run 'git submodule update --init --recursive' or set MULTIGAUGE_CORE_DIR to a valid checkout."
  )
endif()

add_subdirectory("${MULTIGAUGE_CORE_DIR}" multigauge-core-build)

set(MG_WASM_SOURCES
  src/main.cpp
  src/AppEditorExports.cpp
  src/platform/FileSystemEmscripten.cpp
  src/platform/GraphicsContextCanvas.cpp
  src/platform/LoggerWebConsole.cpp
  src/platform/TimeWeb.cpp
)

add_executable(multigauge_wasm ${MG_WASM_SOURCES})
target_link_libraries(multigauge_wasm PRIVATE multigauge_core)

set_target_properties(multigauge_wasm PROPERTIES
  OUTPUT_NAME "multigauge"
  RUNTIME_OUTPUT_DIRECTORY "${STATIC_WASM_DIR}"
)

target_include_directories(multigauge_wasm PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(multigauge_wasm PRIVATE
  -ftemplate-backtrace-limit=5
  -Wno-deprecated-declarations
)

target_include_directories(multigauge_core
  SYSTEM PRIVATE
  "${MULTIGAUGE_CORE_DIR}/lib/rapidjson/src"
)

if (EMSCRIPTEN)
  set_target_properties(multigauge_wasm PROPERTIES SUFFIX ".js")

  target_link_options(multigauge_wasm PRIVATE
    "SHELL:-sWASM=1"
    "SHELL:-sENVIRONMENT=web"
    "SHELL:-sALLOW_MEMORY_GROWTH=1"
    "SHELL:-sASSERTIONS=1"
    "SHELL:-sFORCE_FILESYSTEM=1"
    "SHELL:-sEXPORTED_RUNTIME_METHODS=['FS','callMain','ccall','cwrap']"
    "SHELL:-sEXPORTED_FUNCTIONS=['_main','_mg_editor_attach','_mg_editor_create_view','_mg_editor_list_views','_mg_editor_select_view','_mg_editor_remove_view','_mg_editor_set_view_render_size','_mg_editor_list_tree','_mg_editor_list_elements','_mg_editor_list_values','_mg_editor_get_properties','_mg_editor_get_property_node','_mg_editor_set_property','_mg_editor_add_element','_mg_editor_insert_element','_mg_editor_move_element','_mg_editor_remove_element','_mg_editor_get_element_json','_mg_editor_insert_element_json','_mg_editor_replace_element_json','_mg_editor_bring_forward','_mg_editor_bring_to_front','_mg_editor_send_backward','_mg_editor_send_to_back','_mg_editor_reorder_element','_mg_editor_copy_element','_mg_editor_paste_into_element','_mg_editor_paste_to_replace_element','_mg_editor_duplicate_element','_mg_editor_get_element_bounds','_mg_editor_list_element_bounds','_mg_editor_hit_test','_mg_editor_hit_test_all','_mg_editor_export_face','_mg_preview_create_view','_mg_preview_remove_view']"
    "SHELL:-gsource-map"
  )

  add_custom_command(TARGET multigauge_wasm POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${STATIC_WASM_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:multigauge_wasm>"
      "${STATIC_WASM_DIR}/multigauge.js"
    VERBATIM
  )
endif()
