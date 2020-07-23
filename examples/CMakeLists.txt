set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11 -Wno-unused-variable")

include_directories(${MAMA_ROOT}/include)
link_directories(${MAMA_ROOT}/lib)

add_library(ozimpl SHARED ozimpl.cpp)
install(TARGETS ozimpl  DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(pub pub.cpp)
target_link_libraries(pub ozimpl mama)
install(TARGETS pub DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(sub sub.cpp)
target_link_libraries(sub ozimpl mama)
install(TARGETS sub DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})
