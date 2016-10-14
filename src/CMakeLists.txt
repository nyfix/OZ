include_directories(.)
include_directories(${MAMA_SRC}/common/c_cpp/src/c)
include_directories(${MAMA_SRC}/mama/c_cpp/src/c)
include_directories(${MAMA_SRC}/mama/c_cpp/src/c/bridge/qpid)
include_directories(${MAMA_ROOT}/include)
include_directories(${EVENT_ROOT}/include)
include_directories(${ZMQ_ROOT}/include)

link_directories(${MAMA_ROOT}/lib)
link_directories(${ZMQ_ROOT}/lib)
link_directories(${EVENT_ROOT}/lib)

add_definitions(-DBRIDGE -DMAMA_DLL)

if (CMAKE_BUILD_TYPE MATCHES "Debug")
    set(MAMA_LIB_SUFFIX "mdd")
else()
    set(MAMA_LIB_SUFFIX "md")    
endif()

add_library(libmamazmqimpl${MAMA_LIB_SUFFIX}
            MODULE bridge.c
                   inbox.c
                   inbox.h
                   io.c
                   io.h
                   msg.c
                   msg.h
                   publisher.c
                   queue.c
                   queue.h
                   subscription.c
                   subscription.h
                   timer.c
                   transport.c
                   transport.h
                   zmqbridgefunctions.h
                   zmqdefs.h
                   ${MAMA_SRC}/mama/c_cpp/src/c/bridge/qpid/endpointpool.c)

target_link_libraries(libmamazmqimpl${MAMA_LIB_SUFFIX}
                      libwombatcommon${MAMA_LIB_SUFFIX}
                      libmamac${MAMA_LIB_SUFFIX}
                      libzmq-v120-mt-4_0_4
                      event)

# Platform specific stuff
if (UNIX)
else()
    target_link_libraries(libmamazmqimpl${MAMA_LIB_SUFFIX} uuid Ws2_32)
endif()

# Default to installing directly to MAMA directory unless provided already
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set (CMAKE_INSTALL_PREFIX "${MAMA_ROOT}" CACHE PATH "default install path" FORCE)
endif()

install(TARGETS libmamazmqimpl${MAMA_LIB_SUFFIX}
        CONFIGURATIONS Release
        DESTINATION bin/dynamic)

install(TARGETS libmamazmqimpl${MAMA_LIB_SUFFIX}
        CONFIGURATIONS Debug
        DESTINATION bin/dynamic-debug)

