cmake_minimum_required(VERSION 2.8)
project(dropbox_sisop2 C)

set(CMAKE_C_STANDARD 99)
find_package(Threads REQUIRED)

add_executable(dropbox_sisop2 ./file-control/file-handler.c ./file-control/file-handler.h event-handler/event-handler.c event-handler/event-handler.h lib/utlist.h lib/helper.h lib/helper.c)

if(THREADS_HAVE_PTHREAD_ARG)
    set_property(TARGET dropbox_sisop2 PROPERTY COMPILE_OPTIONS "-pthread")
    set_property(TARGET dropbox_sisop2 PROPERTY INTERFACE_COMPILE_OPTIONS "-pthread")
endif()
if(CMAKE_THREAD_LIBS_INIT)
    target_link_libraries(dropbox_sisop2 "${CMAKE_THREAD_LIBS_INIT}")
endif()



