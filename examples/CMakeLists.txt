set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11 -Wno-unused-variable")

include_directories(${MAMA_ROOT}/include)
link_directories(${MAMA_ROOT}/lib)

add_executable(sub sub.cpp)
target_link_libraries(sub mama)
install(TARGETS sub DESTINATION .)

