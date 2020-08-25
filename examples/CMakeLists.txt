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

add_executable(wc wc.cpp)
target_link_libraries(wc ozimpl mama)
install(TARGETS wc DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(wc2 wc2.cpp)
target_link_libraries(wc2 ozimpl mama)
install(TARGETS wc2 DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(req req.cpp)
target_link_libraries(req ozimpl mama)
install(TARGETS req DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(req2 req2.cpp)
target_link_libraries(req2 ozimpl mama)
install(TARGETS req2 DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(rep rep.cpp)
target_link_libraries(rep ozimpl mama)
install(TARGETS rep DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(timer timer.cpp)
target_link_libraries(timer ozimpl mama)
install(TARGETS timer DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(timer2 timer2.cpp)
target_link_libraries(timer2 ozimpl mama)
install(TARGETS timer2 DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})
