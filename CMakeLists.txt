cmake_minimum_required (VERSION 2.8.7)
project (OpenMAMA-zmq)

# not everyone is compatible w/C++11 yet
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++98 -D_GLIBCXX_USE_CXX11_ABI=0")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")

# use address sanitizer? (same params for gcc/clang)
if("$ENV{BUILD_TYPE}" STREQUAL "asan")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fsanitize-address-use-after-scope -fno-common")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize-address-use-after-scope")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address -fsanitize-address-use-after-scope")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address -fsanitize-address-use-after-scope")
endif()

# use thread sanitizer? (same params for gcc/clang)
if("$ENV{BUILD_TYPE}" STREQUAL "tsan")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread -fPIE")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=thread  ")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread -pie ")
endif()

add_subdirectory(src)
