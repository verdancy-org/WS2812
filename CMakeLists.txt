# CMakeLists.txt for WS2812

# Add module to include path
target_include_directories(xr PUBLIC ${CMAKE_CURRENT_LIST_DIR})

if(LIBXR_DRIVER STREQUAL "mspm0")
  target_include_directories(xr PUBLIC ${CMAKE_CURRENT_LIST_DIR}/TimedWaveform)
endif()

# Auto-include source files
file(GLOB MODULE_WS2812_SRC CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/*.cxx"
  "${CMAKE_CURRENT_LIST_DIR}/*.cc"
  "${CMAKE_CURRENT_LIST_DIR}/*.c"
)

if(LIBXR_DRIVER STREQUAL "mspm0")
  list(APPEND MODULE_WS2812_SRC
    "${CMAKE_CURRENT_LIST_DIR}/TimedWaveform/MSPM0TimedWaveform.cpp")
endif()

target_sources(xr PRIVATE ${MODULE_WS2812_SRC})

# INTERFACE deps shell
get_filename_component(_MODULE_DIRNAME ${CMAKE_CURRENT_LIST_DIR} NAME)
set(_DEPS_TARGET "${_MODULE_DIRNAME}_deps")
add_library(${_DEPS_TARGET} INTERFACE)

# find_package(YourPkg REQUIRED COMPONENTS a b c)

# target_link_libraries(${_DEPS_TARGET} INTERFACE
#   # YourLibA
#   # YourLibB
# )

# target_compile_definitions(${_DEPS_TARGET} INTERFACE
#   # FOO=1
# )
# target_compile_options(${_DEPS_TARGET} INTERFACE
#   # -Wall
# )

# Register to global
set_property(GLOBAL APPEND PROPERTY XR_MODULE_DEPS ${_DEPS_TARGET})
