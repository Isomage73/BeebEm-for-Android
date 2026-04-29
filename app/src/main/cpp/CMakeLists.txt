cmake_minimum_required(VERSION 3.22.1)
project("beebem" CXX)

find_library(log-lib log)
find_library(android-lib android)

add_library(beebem SHARED
    jni/beebem_jni.cpp
)

target_compile_definitions(beebem PRIVATE BEEBEM_ANDROID=1)

target_link_libraries(beebem
    ${log-lib}
    ${android-lib}
)
