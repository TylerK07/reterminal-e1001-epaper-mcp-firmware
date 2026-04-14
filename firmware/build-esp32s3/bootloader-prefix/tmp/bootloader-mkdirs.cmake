# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/Users/tylerklein/esp/esp-idf/components/bootloader/subproject")
  file(MAKE_DIRECTORY "/Users/tylerklein/esp/esp-idf/components/bootloader/subproject")
endif()
file(MAKE_DIRECTORY
  "/Users/tylerklein/github/reterminal-e1001-epaper-mcp-firmware/trees/tylerk-codex-branch/firmware/build-esp32s3/bootloader"
  "/Users/tylerklein/github/reterminal-e1001-epaper-mcp-firmware/trees/tylerk-codex-branch/firmware/build-esp32s3/bootloader-prefix"
  "/Users/tylerklein/github/reterminal-e1001-epaper-mcp-firmware/trees/tylerk-codex-branch/firmware/build-esp32s3/bootloader-prefix/tmp"
  "/Users/tylerklein/github/reterminal-e1001-epaper-mcp-firmware/trees/tylerk-codex-branch/firmware/build-esp32s3/bootloader-prefix/src/bootloader-stamp"
  "/Users/tylerklein/github/reterminal-e1001-epaper-mcp-firmware/trees/tylerk-codex-branch/firmware/build-esp32s3/bootloader-prefix/src"
  "/Users/tylerklein/github/reterminal-e1001-epaper-mcp-firmware/trees/tylerk-codex-branch/firmware/build-esp32s3/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/tylerklein/github/reterminal-e1001-epaper-mcp-firmware/trees/tylerk-codex-branch/firmware/build-esp32s3/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/tylerklein/github/reterminal-e1001-epaper-mcp-firmware/trees/tylerk-codex-branch/firmware/build-esp32s3/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
