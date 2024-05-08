project(c-edit C CXX)
cmake_minimum_required(VERSION 3.3.2)

include(cmake/App.cmake)

set(SOURCES src/main.cpp
            src/App.h
            src/App.cpp
            src/Settings.h
            src/Settings.cpp)

include_directories(d:/boost_1_84_0)
link_directories(d:/boost_1_84_0/stage/lib)

add_app("${SOURCES}")
