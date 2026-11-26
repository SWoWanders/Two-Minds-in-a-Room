# Builds llama.cpp for Android plus a tiny JNI bridge (libllamajni.so) the app's LlamaPlugin calls.
cmake_minimum_required(VERSION 3.22)
project(llamajni C CXX)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_subdirectory(${LLAMA_DIR} llama-build EXCLUDE_FROM_ALL)
add_library(llamajni SHARED llama_jni.cpp)
target_link_libraries(llamajni PRIVATE llama android log)
