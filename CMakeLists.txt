project(dynamic CXX)

cmake_minimum_required(VERSION 2.8.5)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif(CCACHE_FOUND)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto")

add_compile_options(--std=c++14 -Wall -Werror -Wno-parentheses-equality)

add_library(main_archive
            src/analysis.cpp
            src/block.cpp
            src/builtin.cpp
            src/callable.cpp
            src/common.cpp
            src/compiler.cpp
            src/dict.cpp
            src/exception.cpp
            src/file.cpp
            src/frame.cpp
            src/gc.cpp
            src/generator.cpp
            src/instr.cpp
            src/interp.cpp
            src/layout.cpp
            src/list.cpp
            src/module.cpp
            src/name.cpp
            src/numeric.cpp
            src/object.cpp
            src/parser.cpp
            src/reflect.cpp
            src/singletons.cpp
            src/slice.cpp
            src/specials.cpp
            src/string.cpp
            src/syntax.cpp
            src/token.cpp
            src/value.cpp)


add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} main_archive readline gmp gmpxx)
set(MAIN_EXE ${EXECUTABLE_OUTPUT_PATH}/${PROJECT_NAME})

add_executable(unittests src/unittests.cpp
            src/test/test_block.cpp
            src/test/test_builtin.cpp
            src/test/test_dict.cpp
            src/test/test_exception.cpp
            src/test/test_gc.cpp
            src/test/test_interp.cpp
            src/test/test_layout.cpp
            src/test/test_list.cpp
            src/test/test_numeric.cpp
            src/test/test_object.cpp
            src/test/test_parser.cpp
            src/test/test_string.cpp
            src/test/test_token.cpp
            src/test/test_vector.cpp)
target_link_libraries(unittests main_archive gmp gmpxx)
set(UNITTESTS_EXE ${EXECUTABLE_OUTPUT_PATH}/unittests)

add_custom_target(unittest
                  ${UNITTESTS_EXE}
                  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                  DEPENDS unittests)

add_custom_target(pythontest
                  python3 test/run-tests --command ${MAIN_EXE} --verbose
                  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                  DEPENDS ${PROJECT_NAME})

add_custom_target(check DEPENDS unittest pythontest)

add_custom_target(bench
            	  python3 test/run-tests --benchmark --command ${MAIN_EXE} --repeat 10
                  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                  DEPENDS ${PROJECT_NAME})

add_custom_target(bench-compare
            	  python3 test/run-tests --benchmark --command ${MAIN_EXE} --command "python3 -O" --command pypy3
                  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                  DEPENDS ${PROJECT_NAME})
