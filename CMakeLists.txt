cmake_minimum_required(VERSION 2.8)
project(dropbox_sisop2 C)

set(CMAKE_C_STANDARD 99)
find_package(Threads REQUIRED)

add_executable(client ./file-control/file-handler.c ./file-control/file-handler.h event-handler/event-handler.c event-handler/event-handler.h lib/utlist.h lib/helper.h lib/helper.c client-socket/test-client.c client-socket/test-client.h)
add_executable(server ./server/server.c ./server/server.h lib/utlist.h lib/helper.h lib/helper.c)


if(THREADS_HAVE_PTHREAD_ARG)
    set_property(TARGET client PROPERTY COMPILE_OPTIONS "-pthread")
    set_property(TARGET client PROPERTY INTERFACE_COMPILE_OPTIONS "-pthread")
    set_property(TARGET server PROPERTY COMPILE_OPTIONS "-pthread")
    set_property(TARGET server PROPERTY INTERFACE_COMPILE_OPTIONS "-pthread")
endif()
if(CMAKE_THREAD_LIBS_INIT)
    target_link_libraries(client "${CMAKE_THREAD_LIBS_INIT}")
    target_link_libraries(server "${CMAKE_THREAD_LIBS_INIT}")

endif()



