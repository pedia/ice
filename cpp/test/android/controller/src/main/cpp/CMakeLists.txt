# Copyright (c) ZeroC, Inc. All rights reserved.

cmake_minimum_required(VERSION 3.4.1)
set(icecpp_dir ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../..)

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

set(icetest_dir ${icecpp_dir}/test)

# Prepend given prefix if the file with prefix exists
function(prependIfExists var prefix)
    set(listVar "")
    foreach(f ${${var}})
        if(EXISTS "${prefix}/${f}")
            list(APPEND listVar "${prefix}/${f}")
        endif()
    endforeach()
    set(${var} "${listVar}" PARENT_SCOPE)
endfunction(prependIfExists)

# Create a test shared library
function(add_test name suffix)
    cmake_parse_arguments(${name} "" "DIR" "FILES" ${ARGN})

    # Compute source files
    list(APPEND ${name}_FILES ${${name}_extrafiles})
    list(APPEND ${name}_FILES ${${name}_${suffix}_extrafiles})
    prependIfExists(${name}_FILES ${icetest_dir}/${${name}_DIR})
    foreach(f ${${name}_FILES})
        get_filename_component(f_dir ${f} DIRECTORY)
        get_filename_component(f_name ${f} NAME_WE)
        get_filename_component(f_ext ${f} EXT)
        if(f_ext STREQUAL ".ice")
            list(APPEND ${name}_slicefiles ${f})
            list(APPEND ${name}_sourcefiles ${f_dir}/generated/${f_name}.cpp)
        else()
            list(APPEND ${name}_sourcefiles ${f})
        endif()
    endforeach()

    # Slice files
    foreach(f ${${name}_slicefiles})
        get_filename_component(f_name ${f} NAME_WE)
        get_filename_component(f_dir ${f} DIRECTORY)
        add_custom_command(
            OUTPUT ${f_dir}/generated/${f_name}.cpp
                   ${f_dir}/generated/${f_name}.h
            COMMAND ${slice2cpp}
            ARGS -I${icecpp_dir}/../slice
                 -I${f_dir}
                 --output-dir ${f_dir}/generated
                 ${f}
            MAIN_DEPENDENCY ${f})
    endforeach()

    # The test library target
    add_library(
            ${name}_${suffix}
            SHARED
            ${${name}_sourcefiles})
    target_include_directories(
            ${name}_${suffix}
            PUBLIC
            ${icetest_dir}/${${name}_DIR}
            ${icetest_dir}/${${name}_DIR}/generated)
    target_link_libraries(${name}_${suffix} testcommon ice android)
    target_compile_options(${name}_${suffix} PUBLIC ${${name}_cppflags})
    set_target_properties(${name}_${suffix} PROPERTIES LINKER_LANGUAGE CXX)

endfunction(add_test)

# Include directories and macros to build the tests
include_directories(${icecpp_dir}/test/include)
include_directories(${icecpp_dir}/gradle/include)
add_definitions(-DICE_CPP11_MAPPING)

# Test files
set(client Client.cpp AllTests.cpp)
set(server Server.cpp TestI.cpp Test.ice)
set(serveramd ServerAMD.cpp TestAMDI.cpp TestAMD.ice)
set(collocated Collocated.cpp Test.ice AllTests.cpp TestI.cpp)

set(Ice_adapterDeactivation_extrafiles ServantLocatorI.cpp)
set(Ice_exceptions_extrafiles ExceptionsI.cpp)
set(Ice_invoke_extrafiles BlobjectI.cpp)
set(Ice_custom_extrafiles MyByteSeq.cpp StringConverterI.cpp)
set(Ice_custom_sync_extrafiles Wstring.ice WStringI.cpp)
set(Ice_custom_async_skip true)
set(Ice_custom_collocated_skip true)
set(Ice_location_extrafiles ServerLocator.cpp)
set(Ice_objects_extrafiles Forward.ice Derived.ice DerivedEx.ice TestIntfI.cpp)
set(Ice_operations_extrafiles Twoways.cpp TwowaysAMI.cpp
                              Oneways.cpp OnewaysAMI.cpp
                              BatchOneways.cpp BatchOnewaysAMI.cpp)
set(Ice_retry_extrafiles InstrumentationI.cpp)
set(Ice_servantLocator_extrafiles ServantLocatorI.cpp)

# Add targets for the given tests
foreach(name
        acm
        adapterDeactivation
        admin
        ami
        binding
        custom
        enums
        exceptions
        facets
        hold
        invoke
        inheritance
        location
        objects
        operations
        optional
        proxy
        retry
        scope
        servantLocator
        timeout)
    if((EXISTS ${icetest_dir}/Ice/${name}/Server.cpp) AND NOT Ice_${name}_sync_skip)
        add_test(Ice_${name} sync DIR Ice/${name} FILES ${client} ${server})
    endif()
    if((EXISTS ${icetest_dir}/Ice/${name}/Collocated.cpp) AND NOT Ice_${name}_collocated_skip)
        add_test(Ice_${name} collocated DIR Ice/${name} FILES ${collocated})
    endif()
    if((EXISTS ${icetest_dir}/Ice/${name}/ServerAMD.cpp) AND NOT Ice_${name}_async_skip)
        add_test(Ice_${name} async DIR Ice/${name} FILES ${client} ${serveramd})
    endif()
endforeach()

add_test(Ice_defaultServant sync DIR Ice/defaultServant FILES ${client}
                                                              TestI.cpp
                                                              Test.ice)
add_test(Ice_defaultValue sync DIR Ice/defaultValue FILES ${client} Test.ice)
add_test(Ice_interceptor sync DIR Ice/interceptor FILES ${client}
                                                        Test.ice TestI.cpp
                                                        AMDInterceptorI.cpp
                                                        MyObjectI.cpp
                                                        InterceptorI.cpp)
add_test(Ice_stream sync DIR Ice/stream FILES ${client} Test.ice)

# Create a testcommon static library
add_library(testcommon STATIC ${icetest_dir}/Common/TestHelper.cpp)
target_compile_definitions(testcommon PRIVATE -DICE_STATIC_LIBS)

# Import the ice library
add_library(ice SHARED IMPORTED)
set_target_properties(ice PROPERTIES IMPORTED_LOCATION
    ${icecpp_dir}/gradle/lib/shared/${CMAKE_BUILD_TYPE}/${ANDROID_ABI}/libice.so)

# Create the controller JNI library used by the Java controller app.
add_library(controller SHARED Controller.cpp)
target_link_libraries(controller testcommon ice android)
