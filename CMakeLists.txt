project(c-edit C CXX)
cmake_minimum_required(VERSION 3.3.2)

include(cmake/App.cmake)

set(SOURCES src/main.cpp
            src/App.h
            src/App.cpp
            src/Settings.h
            src/Settings.cpp
            src/AssetsProvider.h
            src/ProcessRunner.h
            src/ProcessRunner.cpp)

add_subdirectory(thirdparty/boost EXCLUDE_FROM_ALL)
link_libraries(Boost::filesystem Boost::process Boost::asio)

add_app("${SOURCES}")
