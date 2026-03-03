cmake_minimum_required(VERSION 3.30)
project(server_in VERSION 1.0)

find_package(Boost 1.75 REQUIRED COMPONENTS json)

include(FetchContent)

FetchContent_Declare(
        actions_ex
        GIT_REPOSITORY https://github.com/ImFGameDev/actions_ex.git
        GIT_TAG main
)

FetchContent_Declare(
        debug_system
        GIT_REPOSITORY https://github.com/ImFGameDev/debug_system.git
        GIT_TAG main
)

FetchContent_MakeAvailable(actions_ex debug_system)

add_library(${PROJECT_NAME}
        src/get/session_get.cpp
        src/post/session_post.cpp
        src/tcp/in_session_tcp.cpp
        src/web/in_web_session.cpp
        src/port_acceptor.cpp
        include/commands.h
)

target_include_directories(${PROJECT_NAME}
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PUBLIC actions_ex debug_system)
