cmake_minimum_required(VERSION 3.16)
project(host_algo CXX)

add_executable(algo algoritm.cpp core/net.cpp)
add_executable(sender sender.cpp core/net.cpp)

set_target_properties(algo PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

set_target_properties(sender PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
