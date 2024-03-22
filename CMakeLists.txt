project(MyApp C CXX)
cmake_minimum_required(VERSION 3.3.2)

include(cmake/App.cmake)

set(SOURCES main.cpp)

include_directories(d:/boost_1_84_0)
link_directories(d:/boost_1_84_0/stage/lib)

add_app("${SOURCES}")
