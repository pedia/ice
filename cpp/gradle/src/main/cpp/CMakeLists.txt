# Copyright (c) ZeroC, Inc. All rights reserved.

cmake_minimum_required(VERSION 3.4.1)
set(icecpp_dir ${CMAKE_CURRENT_SOURCE_DIR}/../../../..)

# Check for Slice translator
if("$ENV{ICE_BIN_DIST}" STREQUAL "cpp" OR "$ENV{ICE_BIN_DIST}" STREQUAL "all")
    if(DEFINED ENV{ICE_HOME})
        if(EXISTS $ENV{ICE_HOME}/cpp/bin/slice2cpp)
            set(slice2cpp $ENV{ICE_HOME}/cpp/bin/slice2cpp)
        else(EXISTS $ENV{ICE_HOME}/bin/slice2cpp)
            set(slice2cpp $ENV{ICE_HOME}/bin/slice2cpp)
        endif()
    else()
        if(EXISTS /usr/bin/slice2cpp)
            set(slice2cpp /usr/bin/slice2cpp)
        elseif(EXISTS /usr/local/bin/slice2cpp)
            set(slice2cpp /usr/local/bin/slice2cpp)
        endif()
    endif()
else()
    set(slice2cpp ${icecpp_dir}/bin/slice2cpp)
endif()

message("Using translator: ${slice2cpp}")
if(NOT slice2cpp OR NOT EXISTS ${slice2cpp})
    message(FATAL_ERROR "can't find slice2cpp")
endif()

# C++ files
file(GLOB sources "${icecpp_dir}/src/IceUtil/*.cpp"
                  "${icecpp_dir}/src/Ice/*.cpp"
                  "${icecpp_dir}/src/Ice/generated/*.cpp")

file(GLOB excludes "${icecpp_dir}/src/Ice/WS*.cpp"
                   "${icecpp_dir}/src/Ice/SHA1.cpp"
                   "${icecpp_dir}/src/Ice/DLLMain.cpp"
                   "${icecpp_dir}/src/Ice/StringConverterPlugin.cpp")

list(REMOVE_ITEM sources ${excludes})

# Slice files
file(GLOB slices ${icecpp_dir}/../slice/Ice/*.ice)

foreach(f ${slices})
    get_filename_component(f_name ${f} NAME_WE)
    add_custom_command(
        OUTPUT ${icecpp_dir}/src/Ice/generated/Ice/${f_name}.cpp
               ${icecpp_dir}/include/generated/Ice/${f_name}.h
        COMMAND ${slice2cpp}
        ARGS -I${icecpp_dir}/../slice
             --include-dir Ice
             --output-dir ${icecpp_dir}/src/Ice/generated/Ice
             ${f}
        COMMAND ${CMAKE_COMMAND} -E rename
            ${icecpp_dir}/src/Ice/generated/Ice/${f_name}.h
            ${icecpp_dir}/include/generated/Ice/${f_name}.h
        COMMAND ${CMAKE_COMMAND} -E copy
            ${icecpp_dir}/include/generated/Ice/${f_name}.h
            ${icecpp_dir}/gradle/include/Ice/${f_name}.h
        MAIN_DEPENDENCY ${f})
    list(APPEND sources ${icecpp_dir}/src/Ice/generated/Ice/${f_name}.cpp)
endforeach()

# Include directories
include_directories(${icecpp_dir}/src/Ice/generated
                    ${icecpp_dir}/src
                    ${icecpp_dir}/include/generated
                    ${icecpp_dir}/include)

# Ice static library
add_library(ice-static STATIC ${sources})

target_compile_definitions(ice-static
                           PUBLIC
                           -DICE_STATIC_LIBS
                           -DICE_CPP11_MAPPING
                           -DICE_BUILDING_SRC
                           -DICE_API_EXPORTS)

set_target_properties(ice-static
                      PROPERTIES
                      ARCHIVE_OUTPUT_DIRECTORY
                      ${icecpp_dir}/gradle/lib/static/${CMAKE_BUILD_TYPE}/${ANDROID_ABI})

set_target_properties(ice-static
                      PROPERTIES
                      ARCHIVE_OUTPUT_NAME
                      ice)

# Ice shared library
add_library(ice-shared SHARED ${sources})

target_compile_definitions(ice-shared
                           PUBLIC
                           -DICE_shared_LIBS
                           -DICE_CPP11_MAPPING
                           -DICE_BUILDING_SRC
                           -DICE_API_EXPORTS)

set_target_properties(ice-shared PROPERTIES LINKER_LANGUAGE CXX)

set_target_properties(ice-shared
                      PROPERTIES
                      LIBRARY_OUTPUT_DIRECTORY
                      ${icecpp_dir}/gradle/lib/shared/${CMAKE_BUILD_TYPE}/${ANDROID_ABI})


set_target_properties(ice-shared
                      PROPERTIES
                      LIBRARY_OUTPUT_NAME
                      ice)

target_link_libraries(ice-shared android)

# Copy Ice headers in gradle/include

file(MAKE_DIRECTORY ${icecpp_dir}/gradle/include
                    ${icecpp_dir}/gradle/include/Ice
                    ${icecpp_dir}/gradle/include/IceUtil)

file(COPY ${icecpp_dir}/include/Ice
     DESTINATION ${icecpp_dir}/gradle/include
     FILES_MATCHING PATTERN *.h)

file(COPY ${icecpp_dir}/include/IceUtil
     DESTINATION ${icecpp_dir}/gradle/include
     FILES_MATCHING PATTERN *.h)
