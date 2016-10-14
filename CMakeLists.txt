cmake_minimum_required (VERSION 2.8.11)
project (OpenMAMA-zmq)

if(UNIX)
else()
    if(CMAKE_CL_64)
        set(DEFAULT_INSTALL_PREFIX $ENV{ProgramW6432})
    else()
        set(DEFAULT_INSTALL_PREFIX $ENV{PROGRAMFILES})
    endif()

    # Note path is relative to where it would be used
    set(DEFAULT_MAMA_SRC ../../OpenMAMA)
    set(DEFAULT_MAMA_ROOT "${DEFAULT_INSTALL_PREFIX}/OpenMAMA")
    set(DEFAULT_ZMQ_ROOT "${DEFAULT_INSTALL_PREFIX}/ZeroMQ 4.0.4")
    set(DEFAULT_EVENT_ROOT "${DEFAULT_INSTALL_PREFIX}/libevent")
endif()

if(NOT MAMA_SRC)
    set(MAMA_SRC ${DEFAULT_MAMA_SRC})
endif()

if(NOT MAMA_ROOT)
    set(MAMA_ROOT ${DEFAULT_MAMA_ROOT})
endif()

if(NOT ZMQ_ROOT)
    set(ZMQ_ROOT ${DEFAULT_ZMQ_ROOT})
endif()

if(NOT EVENT_ROOT)
    set(EVENT_ROOT ${DEFAULT_EVENT_ROOT})
endif()

add_subdirectory(src)