cmake_minimum_required (VERSION 3.19)

file(GLOB_RECURSE CPP_FILES "*.cpp")
file(GLOB_RECURSE H_FILES "*.h")

if (MSVC)
    # warning level 4 and all warnings as errors
    add_compile_options(/W4 /WX)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    # lots of warnings and all warnings as errors
    add_compile_options(
		-Wall 
		-Wextra 
		-pedantic 
		-Werror 
		-Wno-unused-function 		# I want unused functions for debugging purposes
		-Wno-reorder				# I want to be able to initialize class members in any order		
		-Wno-invalid-offsetof		# I want to be able to use offsetof with non-PODs when it works (tm)
	)	
	
	if(NWORK_DISABLE_WARNING_CLASS_MEMACCESS)
		# This option seems to be missing sometimes
		add_compile_options(
			-Wno-class-memaccess	# I want to be able to memset() things that I know are safe to memset()
		)		
	endif()
endif()
	
add_library(nwork STATIC ${CPP_FILES} ${H_FILES})
add_library(nwork::nwork ALIAS nwork)

target_compile_features(nwork PRIVATE cxx_std_20)

if(NWORK_PRECOMPILED_HEADERS)
	target_precompile_headers(nwork PRIVATE "Pcheader.h")
endif()
	