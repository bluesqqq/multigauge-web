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
  bindings/ApiBindings.cpp
  bindings/EditorApi.cpp
  bindings/RuntimeApi.cpp
  src/platform/FileSystemEmscripten.cpp
  src/platform/GraphicsContextCanvas.cpp
  src/platform/LoggerWebConsole.cpp
  src/platform/TimeWeb.cpp
)

include("${CMAKE_CURRENT_SOURCE_DIR}/EditorApi.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/RuntimeAPI.cmake")

set(MG_WASM_EXPORTED_FUNCTIONS
  ${MG_RUNTIME_API_EXPORTS}
  ${MG_EDITOR_API_EXPORTS}
)
string(JOIN "," MG_WASM_EXPORTED_FUNCTIONS_JSON ${MG_WASM_EXPORTED_FUNCTIONS})

add_executable(multigauge_wasm ${MG_WASM_SOURCES})
target_link_libraries(multigauge_wasm PRIVATE multigauge_core)

set_target_properties(multigauge_wasm PROPERTIES
  OUTPUT_NAME "multigauge"
  RUNTIME_OUTPUT_DIRECTORY "${STATIC_WASM_DIR}"
)

target_include_directories(multigauge_wasm PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/bindings
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if (NOT MSVC)
  target_compile_options(multigauge_wasm PRIVATE
    -ftemplate-backtrace-limit=5
    -Wno-deprecated-declarations
  )
endif()

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
    "SHELL:-sEXPORTED_FUNCTIONS=[${MG_WASM_EXPORTED_FUNCTIONS_JSON}]"
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
