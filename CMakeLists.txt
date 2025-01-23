cmake_minimum_required (VERSION 3.19)

project(nwork)

macro(nwork_option OPTION_NAME OPTION_TEXT OPTION_DEFAULT)
    option(${OPTION_NAME} ${OPTION_TEXT} ${OPTION_DEFAULT})
    if(DEFINED ENV{${OPTION_NAME}})
        # Allow overriding the option through an environment variable
        set(${OPTION_NAME} $ENV{${OPTION_NAME}})
    endif()
    if(${OPTION_NAME})
        add_definitions(-D${OPTION_NAME})
    endif()
    message(STATUS "${OPTION_NAME}: ${${OPTION_NAME}}")
endmacro()

nwork_option(NWORK_PRECOMPILED_HEADERS "Enable precompiled headers." ON)
nwork_option(NWORK_DISABLE_WARNING_CLASS_MEMACCESS "-Wno-class-memaccess" ON)
nwork_option(NWORK_TEST "Builds tests." ON)
nwork_option(NWORK_DISABLE_MSVC_ITERATOR_DEBUG "_ITERATOR_DEBUG_LEVEL=0" ON)

include(FetchContent)
include(GNUInstallDirs)

# -------------------

include_directories(extra)

if(MSVC)
	add_compile_options(/bigobj /MP)

	if(NWORK_DISABLE_MSVC_ITERATOR_DEBUG)
		add_compile_definitions(_ITERATOR_DEBUG_LEVEL=0)	
	endif()
endif()

add_subdirectory(lib)
add_subdirectory(include)

if(NWORK_TEST)
	add_subdirectory(test)
endif()


