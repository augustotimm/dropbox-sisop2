cmake_minimum_required(VERSION 2.8)
project(dropbox_sisop2 C)

set(CMAKE_C_STANDARD 99)
find_package(Threads REQUIRED)

add_executable(serverAPP ./server/server.c ./server/server.h lib/utlist.h lib/helper.h lib/helper.c server/user.c server/user.h server/server_functions.c server/server_functions.h server/replica-manager/replica-manager.c server/replica-manager/replica-manager.h)
add_executable(client ./client-socket/test-client.c ./client-socket/test-client.h lib/utlist.h lib/helper.h lib/helper.c ./file-control/file-handler.c ./file-control/file-handler.h server/server_functions.c server/server_functions.h client-socket/front-end.c client-socket/front-end.h)



if(THREADS_HAVE_PTHREAD_ARG)
    set_property(TARGET client PROPERTY COMPILE_OPTIONS "-pthread")
    set_property(TARGET client PROPERTY INTERFACE_COMPILE_OPTIONS "-pthread")
    set_property(TARGET serverAPP PROPERTY COMPILE_OPTIONS "-pthread")
    set_property(TARGET serverAPP PROPERTY INTERFACE_COMPILE_OPTIONS "-pthread")
endif()
if(CMAKE_THREAD_LIBS_INIT)
    target_link_libraries(client "${CMAKE_THREAD_LIBS_INIT}")
    target_link_libraries(serverAPP "${CMAKE_THREAD_LIBS_INIT}")
endif()



