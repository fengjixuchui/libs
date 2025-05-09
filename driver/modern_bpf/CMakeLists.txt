# SPDX-License-Identifier: GPL-2.0-only OR MIT
#
# Copyright (C) 2023 The Falco Authors.
#
# This file is dual licensed under either the MIT or GPL 2. See MIT.txt or GPL.txt for full copies
# of the license.
#

option(MODERN_BPF_DEBUG_MODE "Enable BPF debug prints" OFF)
option(MODERN_BPF_EXCLUDE_PROGS "Regex to exclude tail-called programs" "")

# ##################################################################################################
# Debug mode
# ##################################################################################################

if(MODERN_BPF_DEBUG_MODE)
	set(DEBUG "MODERN_BPF_DEBUG")
else()
	set(DEBUG "")
endif()
message(STATUS "${MODERN_BPF_LOG_PREFIX} MODERN_BPF_DEBUG_MODE: ${MODERN_BPF_DEBUG_MODE}")

# ##################################################################################################
# Check kernel version.
# ##################################################################################################

# We check it here because the skeleton could be provided with the `MODERN_BPF_SKEL_DIR` env
# variable so the compilation is still possible on older kernels.
execute_process(
	COMMAND uname -r
	OUTPUT_VARIABLE UNAME_RESULT
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
string(REGEX MATCH "[0-9]+.[0-9]+" LINUX_KERNEL_VERSION ${UNAME_RESULT})
set(modern_bpf_min_kver_map_x86_64 5.8)
set(modern_bpf_min_kver_map_aarch64 5.8)
set(modern_bpf_min_kver_map_s390x 5.8)
set(modern_bpf_min_kver_map_ppc64le 5.8)
if(LINUX_KERNEL_VERSION VERSION_LESS ${modern_bpf_min_kver_map_${CMAKE_HOST_SYSTEM_PROCESSOR}})
	message(
		WARNING
			"${MODERN_BPF_LOG_PREFIX} To run this driver you need a Linux kernel version >= ${modern_bpf_min_kver_map_${CMAKE_HOST_SYSTEM_PROCESSOR}} but actual kernel version is: ${UNAME_RESULT}"
	)
endif()

# ##################################################################################################
# Get driver version.
# ##################################################################################################

# This is needed to obtain the modern bpf driver version
include(compute_versions RESULT_VARIABLE RESULT)
if(RESULT STREQUAL NOTFOUND)
	message(
		FATAL_ERROR
			"${MODERN_BPF_LOG_PREFIX} problem with compute_versions.cmake in ${CMAKE_MODULE_PATH}"
	)
endif()
compute_versions(../API_VERSION ../SCHEMA_VERSION)
configure_file(../driver_config.h.in ${CMAKE_CURRENT_SOURCE_DIR}/../driver_config.h)

# ##################################################################################################
# Check clang version.
# ##################################################################################################

# If the user doesn't provide the path to `clang` try to find it locally
if(NOT MODERN_CLANG_EXE)
	find_program(
		MODERN_CLANG_EXE
		NAMES clang
		DOC "Path to clang executable"
	)
	if(NOT MODERN_CLANG_EXE)
		message(FATAL_ERROR "${MODERN_BPF_LOG_PREFIX} unable to find clang")
	endif()
endif()

# If it is a relative path we convert it to an absolute one relative to the root source directory.
get_filename_component(
	MODERN_CLANG_EXE "${MODERN_CLANG_EXE}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}"
)
message(STATUS "${MODERN_BPF_LOG_PREFIX} clang used by the modern bpf probe: '${MODERN_CLANG_EXE}'")

# Check the right clang version (>=12)
execute_process(
	COMMAND ${MODERN_CLANG_EXE} --version
	OUTPUT_VARIABLE CLANG_version_output
	ERROR_VARIABLE CLANG_version_error
	RESULT_VARIABLE CLANG_version_result
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(${CLANG_version_result} EQUAL 0)
	if("${CLANG_version_output}" MATCHES "clang version ([^\n]+)\n")
		# Transform X.Y.Z into X;Y;Z which can then be interpreted as a list
		set(CLANG_VERSION "${CMAKE_MATCH_1}")
		string(REPLACE "." ";" CLANG_VERSION_LIST ${CLANG_VERSION})
		list(GET CLANG_VERSION_LIST 0 CLANG_VERSION_MAJOR)

		string(COMPARE LESS ${CLANG_VERSION_MAJOR} 12 CLANG_VERSION_MAJOR_LT12)

		if(${CLANG_VERSION_MAJOR_LT12})
			message(
				WARNING
					"${MODERN_BPF_LOG_PREFIX} clang '${CLANG_VERSION}' is too old for compiling the modern BPF probe, you need at least '12.0.0' version"
			)
		endif()

		message(STATUS "${MODERN_BPF_LOG_PREFIX} Found clang version: ${CLANG_VERSION}")
	else()
		message(
			WARNING
				"${MODERN_BPF_LOG_PREFIX} Failed to parse clang version string: ${CLANG_version_output}"
		)
	endif()
else()
	message(
		FATAL_ERROR
			"${MODERN_BPF_LOG_PREFIX} Command \"${MODERN_CLANG_EXE} --version\" failed with output:\n ${CLANG_version_error}"
	)
endif()

# ##################################################################################################
# Check bpftool version.
# ##################################################################################################

# If the user doesn't provide the path to `bpftool` try to find it locally
if(NOT MODERN_BPFTOOL_EXE)
	find_program(
		MODERN_BPFTOOL_EXE
		NAMES bpftool
		DOC "Path to bpftool executable"
	)
	if(NOT MODERN_BPFTOOL_EXE)
		message(FATAL_ERROR "${MODERN_BPF_LOG_PREFIX} unable to find bpftool on the system")
	endif()
endif()

# If it is a relative path we convert it to an absolute one relative to the root source directory.
get_filename_component(
	MODERN_BPFTOOL_EXE "${MODERN_BPFTOOL_EXE}" REALPATH BASE_DIR "${CMAKE_SOURCE_DIR}"
)
message(
	STATUS "${MODERN_BPF_LOG_PREFIX} bpftool used by the modern bpf probe: '${MODERN_BPFTOOL_EXE}'"
)

# Check the right bpftool version Since we want bpftool to have the gen skeleton subcommands and
# both gen and skeleton were added together, we can just grep the help output for gen. see:
# https://lore.kernel.org/bpf/20191210011438.4182911-12-andriin@fb.com/
#
# This is not as strict as checking versions, but it also allows compiling on bpftool versions for
# backported kernels.
execute_process(
	COMMAND sh -c "${MODERN_BPFTOOL_EXE} help 2>&1 | grep -wq 'gen'"
	OUTPUT_VARIABLE BPFTOOL_version_output
	ERROR_VARIABLE BPFTOOL_version_error
	RESULT_VARIABLE BPFTOOL_version_result
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(NOT ${BPFTOOL_version_result} EQUAL 0)
	message(WARNING "${MODERN_BPF_LOG_PREFIX} bpftool does not support gen command")
endif()

# ##################################################################################################
# Get clang bpf system includes
# ##################################################################################################

execute_process(
	COMMAND bash -c "${MODERN_CLANG_EXE} -v -E - < /dev/null 2>&1 |
		  sed -n '/<...> search starts here:/,/End of search list./{ s| \\(/.*\\)|-idirafter \\1|p }'"
	OUTPUT_VARIABLE CLANG_SYSTEM_INCLUDES_output
	ERROR_VARIABLE CLANG_SYSTEM_INCLUDES_error
	RESULT_VARIABLE CLANG_SYSTEM_INCLUDES_result
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(${CLANG_SYSTEM_INCLUDES_result} EQUAL 0)
	string(REPLACE "\n" " " CLANG_SYSTEM_INCLUDES "${CLANG_SYSTEM_INCLUDES_output}")
	message(STATUS "${MODERN_BPF_LOG_PREFIX} BPF system include flags: ${CLANG_SYSTEM_INCLUDES}")
else()
	message(
		FATAL_ERROR
			"${MODERN_BPF_LOG_PREFIX} Failed to determine BPF system includes: ${CLANG_SYSTEM_INCLUDES_error}"
	)
endif()

# ##################################################################################################
# Get target arch
# ##################################################################################################

execute_process(
	COMMAND uname -m
	COMMAND sed "s/x86_64/x86/"
	COMMAND sed "s/aarch64/arm64/"
	COMMAND sed "s/ppc64le/powerpc/"
	COMMAND sed "s/mips.*/mips/"
	COMMAND sed "s/s390x/s390/"
	OUTPUT_VARIABLE ARCH_output
	ERROR_VARIABLE ARCH_error
	RESULT_VARIABLE ARCH_result
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(${ARCH_result} EQUAL 0)
	set(ARCH ${ARCH_output})
	message(STATUS "${MODERN_BPF_LOG_PREFIX} Target arch: ${ARCH}")
else()
	message(
		FATAL_ERROR
			"${MODERN_BPF_LOG_PREFIX} Failed to determine target architecture: ${ARCH_error}"
	)
endif()

# ##################################################################################################
# Set includes and compilation flags
# ##################################################################################################

# Get modern probe include.
list(APPEND MODERN_PROBE_INCLUDE "-I${CMAKE_CURRENT_SOURCE_DIR}")

# Note here we use the libs root directory since we want to avoid conflicts between the `bpf` folder
# inside `driver` and the `libbpf` includes.
set(PPM_INCLUDE ${LIBS_DIR})

include(libbpf)

# Get libbpf include
get_target_property(LIBBPF_INTERFACE_INCLUDE_DIRS lbpf INTERFACE_INCLUDE_DIRECTORIES)
foreach(dir ${LIBBPF_INTERFACE_INCLUDE_DIRS})
	list(APPEND LIBBPF_INTERFACE_INCLUDE "-I${dir}")
endforeach()

# Set CLANG FLAGS
set(CLANG_FLAGS "")
list(
	APPEND
	CLANG_FLAGS
	-g
	-O2
	-target
	bpf
	-D__${DEBUG}__
	-D__TARGET_ARCH_${ARCH} # Match libbpf usage in `/libbpf/src/bpf_tracing.h`
	-D__USE_VMLINUX__ # Used to compile without kernel headers.
	${LIBBPF_INTERFACE_INCLUDE}
	${MODERN_PROBE_INCLUDE}
	-I${PPM_INCLUDE}
	-isystem
)

message(STATUS "${MODERN_BPF_LOG_PREFIX} Compilation flags: ${CLANG_FLAGS}")

# Search all bpf includes files. (we can use bpf.h files)
file(GLOB_RECURSE BPF_H_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

# Search all bpf.c files
file(GLOB_RECURSE BPF_C_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.bpf.c)

# ##################################################################################################
# Generate the events dimensions file generator executable.
# ##################################################################################################

add_executable(
	events_dimensions_generator ${CMAKE_CURRENT_SOURCE_DIR}/definitions/generator/generator.cpp
)
target_link_libraries(events_dimensions_generator PRIVATE scap_event_schema)
add_dependencies(events_dimensions_generator scap_event_schema)

# ##################################################################################################
# Generate the events dimensions file.
# ##################################################################################################

set(BPF_EVENTS_DIMENSIONS_FILE ${CMAKE_CURRENT_SOURCE_DIR}/definitions/events_dimensions.h)
add_custom_command(
	OUTPUT ${BPF_EVENTS_DIMENSIONS_FILE}
	COMMAND events_dimensions_generator ${BPF_EVENTS_DIMENSIONS_FILE}
	VERBATIM
	DEPENDS events_dimensions_generator ${CMAKE_CURRENT_SOURCE_DIR}/../event_table.c
	COMMENT
		"${MODERN_BPF_LOG_PREFIX} Building events dimensions file: ${BPF_EVENTS_DIMENSIONS_FILE}"
)

add_custom_target(EventsDimensions ALL DEPENDS ${BPF_EVENTS_DIMENSIONS_FILE})

# ##################################################################################################
# Generate an `bpf.o` file for every `bpf.c`
# ##################################################################################################

foreach(BPF_C_FILE ${BPF_C_FILES})
	get_filename_component(file_stem ${BPF_C_FILE} NAME_WE)

	if(MODERN_BPF_EXCLUDE_PROGS)
		if(${file_stem} MATCHES "${MODERN_BPF_EXCLUDE_PROGS}")
			message(STATUS "Exclude file: ${file_stem}")
			continue()
		endif()
	endif()

	set(BPF_O_FILE ${CMAKE_CURRENT_BINARY_DIR}/${file_stem}.bpf.o)

	add_custom_command(
		OUTPUT ${BPF_O_FILE}
		COMMAND ${MODERN_CLANG_EXE} ${CLANG_FLAGS} ${CLANG_SYSTEM_INCLUDES} -c ${BPF_C_FILE} -o
				${BPF_O_FILE}
		VERBATIM
		DEPENDS lbpf
		DEPENDS ${BPF_C_FILE} ${BPF_H_FILES} EventsDimensions
		COMMENT "${MODERN_BPF_LOG_PREFIX} Building BPF object: ${BPF_O_FILE}"
	)

	list(APPEND BPF_OBJECT_FILES ${BPF_O_FILE})
endforeach()

# ##################################################################################################
# Generate a unique `bpf.o` file
# ##################################################################################################

set(UNIQUE_BPF_O_FILE ${CMAKE_CURRENT_BINARY_DIR}/${UNIQUE_BPF_O_FILE_NAME}.o)
add_custom_command(
	OUTPUT ${UNIQUE_BPF_O_FILE}
	COMMAND ${MODERN_BPFTOOL_EXE} gen object ${UNIQUE_BPF_O_FILE} ${BPF_OBJECT_FILES}
	VERBATIM
	DEPENDS ${BPF_OBJECT_FILES}
	COMMENT "${MODERN_BPF_LOG_PREFIX} Building BPF unique object file: ${UNIQUE_BPF_O_FILE}"
)

# ##################################################################################################
# Generate the skeleton file
# ##################################################################################################

set(BPF_SKEL_FILE ${MODERN_BPF_SKEL_DIR}/${UNIQUE_BPF_O_FILE_NAME}.skel.h)
add_custom_command(
	OUTPUT ${BPF_SKEL_FILE}
	COMMAND bash -c "${MODERN_BPFTOOL_EXE} gen skeleton ${UNIQUE_BPF_O_FILE} > ${BPF_SKEL_FILE}"
	VERBATIM
	DEPENDS ${UNIQUE_BPF_O_FILE}
	COMMENT "${MODERN_BPF_LOG_PREFIX} Building BPF skeleton: ${BPF_SKEL_FILE}"
)

# ##################################################################################################
# Add the skeleton as a custom target
# ##################################################################################################

add_custom_target(ProbeSkeleton ALL DEPENDS ${BPF_SKEL_FILE})
