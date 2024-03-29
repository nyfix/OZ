include(GNUInstallDirs)

# need c9x mode
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")

include_directories(.)
include_directories(${MAMA_ROOT}/include)
include_directories(${ZMQ_ROOT}/include)

# Default to installing directly to MAMA directory
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set (CMAKE_INSTALL_PREFIX "${MAMA_ROOT}" CACHE PATH "default install path" FORCE)
endif()

link_directories(${MAMA_ROOT}/lib)
if(${CMAKE_SYSTEM_NAME} STREQUAL Darwin)
  link_directories(${ZMQ_ROOT}/lib)
  link_directories("/usr/local/Cellar/ossp-uuid/1.6.2_2/lib")
  link_directories("/usr/local/lib")
else()
  link_directories(${ZMQ_ROOT}/${CMAKE_INSTALL_LIBDIR})
endif()

# while libzmq exposes a C api, it is coded in C++ and requires C++ stdlib and that libstdc++ MUST be at least as recent as the
# version used to build libzmq, which may not be the "system" libstdc++ so we explicitly add its location
execute_process(COMMAND bash "-c" "dirname $(${CMAKE_CXX_COMPILER} -m64 -print-file-name=libstdc++.so)" OUTPUT_VARIABLE ZMQ_CXX_LINK_DIRS OUTPUT_STRIP_TRAILING_WHITESPACE)
message("ZMQ_CXX_LINK_DIRS=${ZMQ_CXX_LINK_DIRS}")
link_directories(${ZMQ_CXX_LINK_DIRS})

add_definitions(-DBRIDGE -DMAMA_DLL -DOPENMAMA_INTEGRATION)

if(WIN32)
    if (CMAKE_BUILD_TYPE MATCHES "Debug")
        set(MAMA_LIB_SUFFIX "mdd")
    else()
        set(MAMA_LIB_SUFFIX "md")
    endif()
else()
    set(MAMA_LIB_SUFFIX "")
endif()

add_library(mamazmqimpl${MAMA_LIB_SUFFIX}
            SHARED bridge.c
                   inbox.c
                   inbox.h
                   io.c
                   msg.c
                   msg.h
                   publisher.c
                   uqueue.c
                   uqueue.h
                   queue.c
                   queue.h
                   subscription.c
                   subscription.h
                   timer.c
                   transport.c
                   transport.h
                   zmqbridgefunctions.h
                   zmqdefs.h
                   util.h util.c
                   notimpl.c
                   params.c
                   )

add_executable(nsd nsd.c)

if(WIN32)
    target_link_libraries(mamazmqimpl${MAMA_LIB_SUFFIX}
                          libwombatcommon${MAMA_LIB_SUFFIX}
                          libmamac${MAMA_LIB_SUFFIX}
                          libzmq-v120-mt-4_0_4
                          uuid
                          Ws2_32)

    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    set_target_properties(mamazmqimpl${MAMA_LIB_SUFFIX} PROPERTIES PREFIX "lib")

    # Windows Targets
    install(TARGETS mamazmqimpl${MAMA_LIB_SUFFIX}
            CONFIGURATIONS Release
            DESTINATION bin/dynamic)
    install(TARGETS mamazmqimpl${MAMA_LIB_SUFFIX}
            CONFIGURATIONS Debug
            DESTINATION bin/dynamic-debug)

else()
    target_link_libraries(mamazmqimpl${MAMA_LIB_SUFFIX}
                          wombatcommon
                          mama
                          zmq
                          uuid)
    install(TARGETS mamazmqimpl${MAMA_LIB_SUFFIX} DESTINATION lib)

    # need to use c++ linker w/nsd under certain conditions (e.g., w/ubsan)
    set_target_properties(nsd PROPERTIES LINKER_LANGUAGE CXX)
    target_link_libraries(nsd wombatcommon mama zmq)
    install(TARGETS nsd DESTINATION bin)
endif()
