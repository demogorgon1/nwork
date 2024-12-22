cmake_minimum_required(VERSION 3.19)

target_include_directories(nwork PUBLIC  
    $<INSTALL_INTERFACE:include>    
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

file(GLOB_RECURSE H_FILES "nwork/*.h")

target_sources(nwork PRIVATE ${H_FILES})

install(DIRECTORY nwork DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
