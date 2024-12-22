cmake_minimum_required (VERSION 3.19)

file(GLOB CPP_FILES "*.cpp")
file(GLOB H_FILES "*.h")

add_executable(nwork-test ${CPP_FILES} ${H_FILES})

if(NWORK_PRECOMPILED_HEADERS)
	target_precompile_headers(nwork-test PRIVATE "Pcheader.h")
endif()

target_compile_features(nwork-test PRIVATE cxx_std_20)
target_link_libraries(nwork-test nwork::nwork)

