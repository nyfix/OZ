include_directories(.)
include_directories(${MAMA_SRC}/common/c_cpp/src/c)
include_directories(${MAMA_SRC}/mama/c_cpp/src/c)
include_directories(${MAMA_SRC}/mama/c_cpp/src/c/bridge/qpid)
include_directories(${MAMA_ROOT}/include)
include_directories(${EVENT_ROOT}/include)
include_directories(${ZMQ_ROOT}/include)

link_directories(${MAMA_ROOT}/lib)
# version 4.2 and later put shared object in lib64
link_directories(${ZMQ_ROOT}/lib64)
# version 4.1 and earlier put shared object in lib64
#link_directories(${ZMQ_ROOT}/lib)
link_directories(${EVENT_ROOT}/lib)

add_definitions(-DBRIDGE -DMAMA_DLL)

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
                   util.h util.c
                   ${MAMA_SRC}/mama/c_cpp/src/c/bridge/qpid/endpointpool.c)

if(WIN32)
    target_link_libraries(mamazmqimpl${MAMA_LIB_SUFFIX}
                          libwombatcommon${MAMA_LIB_SUFFIX}
                          libmamac${MAMA_LIB_SUFFIX}
                          libzmq-v120-mt-4_0_4
                          event
                          uuid
                          Ws2_32)

    set_target_properties(mamazmqimpl${MAMA_LIB_SUFFIX} PROPERTIES PREFIX "lib")

    # Windows Targets
    install(TARGETS mamazmqimpl${MAMA_LIB_SUFFIX}
            CONFIGURATIONS Release
            DESTINATION bin/dynamic)
    install(TARGETS mamazmqimpl${MAMA_LIB_SUFFIX}
            CONFIGURATIONS Debug
            DESTINATION bin/dynamic-debug)

elseif(UNIX)
    # note: io.c uses u_char (gnu extension)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")

    target_link_libraries(mamazmqimpl${MAMA_LIB_SUFFIX}
                          wombatcommon
                          mama
                          zmq
                          uuid
                          event)
    install(TARGETS mamazmqimpl${MAMA_LIB_SUFFIX} DESTINATION lib)
endif()

# Default to installing directly to MAMA directory unless provided already
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set (CMAKE_INSTALL_PREFIX "${MAMA_ROOT}" CACHE PATH "default install path" FORCE)
endif()
