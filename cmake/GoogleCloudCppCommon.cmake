# Copyright 2018 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Find out the name of the subproject.
get_filename_component(GOOGLE_CLOUD_CPP_SUBPROJECT "${CMAKE_CURRENT_SOURCE_DIR}" NAME)

# Find out what flags turn on all available warnings and turn those warnings
# into errors.
include(CheckCXXCompilerFlag)
if (NOT MSVC)
    CHECK_CXX_COMPILER_FLAG(-Wall GOOGLE_CLOUD_CPP_COMPILER_SUPPORTS_WALL)
    CHECK_CXX_COMPILER_FLAG(-Werror GOOGLE_CLOUD_CPP_COMPILER_SUPPORTS_WERROR)
else ()
    CHECK_CXX_COMPILER_FLAG("/std:c++latest" GOOGLE_CLOUD_CPP_COMPILER_SUPPORTS_CPP_LATEST)
endif ()

# If possible, enable a code coverage build type.
include(EnableCoverage)

# Include the functions to enable Clang sanitizers.
include(EnableSanitizers)

# Include support for clang-tidy if available
include(EnableClangTidy)

# C++ Exceptions are enabled by default, but allow the user to turn them off.
include(EnableCxxExceptions)

# Get the destination directories based on the GNU recommendations.
include(GNUInstallDirs)

if (${CMAKE_VERSION} VERSION_LESS "3.9")
    # Old versions of CMake have really poor support for Doxygen generation.
    message(STATUS "Doxygen generation only enabled for cmake 3.9 and higher")
else ()
    find_package(Doxygen)
    if (Doxygen_FOUND)
        set(DOXYGEN_RECURSIVE YES)
        set(DOXYGEN_FILE_PATTERNS *.h *.cc *.proto *.md *.dox)
        set(DOXYGEN_EXAMPLE_RECURSIVE YES)
        set(DOXYGEN_EXCLUDE third_party)
        set(DOXYGEN_EXCLUDE_SYMLINKS YES)
        set(DOXYGEN_QUIET YES)
        set(DOXYGEN_WARN_AS_ERROR YES)
        set(DOXYGEN_INLINE_INHERITED_MEMB YES)
        set(DOXYGEN_JAVADOC_AUTOBRIEF YES)
        set(DOXYGEN_BUILTIN_STL_SUPPORT YES)
        set(DOXYGEN_IDL_PROPERTY_SUPPORT NO)
        set(DOXYGEN_EXTRACT_ALL YES)
        set(DOXYGEN_EXTRACT_STATIC YES)
        set(DOXYGEN_SORT_MEMBERS_CTORS_1ST YES)
        set(DOXYGEN_GENERATE_TODOLIST NO)
        set(DOXYGEN_GENERATE_BUGLIST NO)
        set(DOXYGEN_GENERATE_TESTLIST NO)
        set(DOXYGEN_CLANG_ASSISTED_PARSING YES)
        set(DOXYGEN_CLANG_OPTIONS "\
-std=c++11 \
-I${PROJECT_SOURCE_DIR} \
-I${PROJECT_BINARY_DIR} \
-I${PROJECT_SOURCE_DIR}/googletest/include \
-I${PROJECT_SOURCE_DIR}/googletest/googlemock/include")
        set(DOXYGEN_GENERATE_LATEX NO)
        set(DOXYGEN_GRAPHICAL_HIERARCHY NO)
        set(DOXYGEN_DIRECTORY_GRAPH NO)
        set(DOXYGEN_CLASS_GRAPH NO)
        set(DOXYGEN_COLLABORATION_GRAPH NO)
        set(DOXYGEN_INCLUDE_GRAPH NO)
        set(DOXYGEN_INCLUDED_BY_GRAPH NO)
        set(DOXYGEN_DOT_TRANSPARENT YES)
        set(DOXYGEN_MACRO_EXPANSION YES)
        set(DOXYGEN_EXPAND_ONLY_PREDEF YES)
        set(DOXYGEN_HTML_TIMESTAMP )
        set(DOXYGEN_STRIP_FROM_INC_PATH "${PROJECT_SOURCE_DIR}")

        doxygen_add_docs(${GOOGLE_CLOUD_CPP_SUBPROJECT}-docs ${CMAKE_CURRENT_SOURCE_DIR}
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                COMMENT "Generate HTML documentation")
        add_dependencies(doxygen-docs ${GOOGLE_CLOUD_CPP_SUBPROJECT}-docs)
    endif ()
endif ()

function(google_cloud_cpp_add_common_options target)
    if (GOOGLE_CLOUD_CPP_COMPILER_SUPPORTS_CPP_LATEST)
        target_compile_options(${target} INTERFACE "/std:c++latest")
    endif ()
    if (GOOGLE_CLOUD_CPP_COMPILER_SUPPORTS_WALL)
        target_compile_options(${target} INTERFACE "-Wall")
    endif ()
    if (GOOGLE_CLOUD_CPP_COMPILER_SUPPORTS_WERROR)
        target_compile_options(${target} INTERFACE "-Werror")
    endif ()
endfunction()

function (google_cloud_cpp_add_clang_tidy target)
    if (CLANG_TIDY_EXE AND GOOGLE_CLOUD_CPP_CLANG_TIDY)
        set_target_properties(
                ${target} PROPERTIES
                CXX_CLANG_TIDY "${CLANG_TIDY_EXE}"
        )
    endif ()
endfunction ()
