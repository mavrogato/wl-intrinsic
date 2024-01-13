
cmake_minimum_required(VERSION 3.28)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_COMPILER clang)
set(CMAKE_C_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug) # debug only

include(FetchContent)

FetchContent_Populate(aux
  GIT_REPOSITORY https://github.com/mavrogato/aux
  GIT_TAG main)

FetchContent_Populate(wayland
  GIT_REPOSITORY https://gitlab.freedesktop.org/wayland/wayland
  GIT_TAG main)
add_custom_target(wayland-client-private
  COMMAND cd wayland-src && meson ../wayland-build/ --buildtype=debug --prefix=${CMAKE_CURRENT_BINARY_DIR} # debug only
  COMMAND ninja -C ./wayland-build/ install)

FetchContent_Populate(wayland-protocols
  GIT_REPOSITORY https://gitlab.freedesktop.org/wayland/wayland-protocols
  GIT_TAG main)
add_custom_command(
  OUTPUT xdg-shell-private.c
  COMMAND wayland-scanner client-header wayland-protocols-src/stable/xdg-shell/xdg-shell.xml xdg-shell.h
  COMMAND wayland-scanner private-code  wayland-protocols-src/stable/xdg-shell/xdg-shell.xml xdg-shell-private.c)

add_library(xdg-shell-private
  xdg-shell-private.c)

add_executable(wl-intrinsic
  wl-intrinsic.cc)
target_include_directories(wl-intrinsic
  PRIVATE
  ${aux_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}/include)

target_compile_options(wl-intrinsic
  PRIVATE
  -Wall
  -Wextra
  -fsycl
  -fsycl-targets=nvptx64-nvidia-cuda
  -Wno-unknown-cuda-version)

target_link_directories(wl-intrinsic
  PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/lib/x86_64-linux-gnu)
target_link_libraries(wl-intrinsic
  PRIVATE
  sycl
  xdg-shell-private
  wayland-client)

add_dependencies(
  wl-intrinsic
  wayland-client-private)

add_custom_target(run
  DEPENDS wl-intrinsic
  COMMAND WAYLAND_DEBUG=1 ./wl-intrinsic)
