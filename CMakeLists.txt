cmake_minimum_required(VERSION 3.12)
project(OS_ex5_VirtualMemory)

set(CMAKE_CXX_STANDARD 14)

#add_executable(OS_ex5_VirtualMemory main.cpp PhysicalMemory.cpp VirtualMemory.cpp)
add_executable(OS_ex5_VirtualMemory SimpleTest.cpp PhysicalMemory.cpp VirtualMemory.cpp)