# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "")
  file(REMOVE_RECURSE
  "CMakeFiles\\ShadowLauncher_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\ShadowLauncher_autogen.dir\\ParseCache.txt"
  "ShadowLauncher_autogen"
  )
endif()
