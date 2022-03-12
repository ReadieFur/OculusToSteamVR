cmake_minimum_required(VERSION "3.7.1")

# Solution
project("OculusToSteamVR")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(SIZEOF_VOIDP ${CMAKE_SIZEOF_VOID_P})
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(PROCESSOR_ARCH "64")
else()
    set(PROCESSOR_ARCH "32")
endif()

if(WIN32)
    set(PLATFORM_NAME "win")
elseif(UNIX AND NOT APPLE)
    set(PLATFORM_NAME "linux")
elseif(APPLE)
    set(PLATFORM_NAME "osx")
endif()

find_library(OPENVR_LIB openvr_api HINTS "${CMAKE_CURRENT_SOURCE_DIR}/libraries/openvr/lib/${PLATFORM_NAME}${PROCESSOR_ARCH}/" NO_DEFAULT_PATH )
find_library(LIBOVR_LIB LibOVR HINTS "${CMAKE_CURRENT_SOURCE_DIR}/libraries/LibOVR/Lib/Windows/x64/Release/VS2017/" NO_DEFAULT_PATH ) #Windows only

set(DRIVER_NAME "oculus_to_steamvr")
set(PROJECT_NAME "driver_${DRIVER_NAME}")
file(GLOB_RECURSE HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/driver_files/src/*.hpp")
file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/driver_files/src/*.cpp")
add_library("${PROJECT_NAME}" SHARED "${HEADERS}" "${SOURCES}")

target_include_directories("${PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/libraries/openvr/headers")
target_include_directories("${PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/libraries/linalg")
target_include_directories("${PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/libraries/LibOVR/Include")
target_include_directories("${PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/libraries/LibOVR/Include/Extras")
target_include_directories("${PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/libraries/DirectX")
target_include_directories("${PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/driver_files/src/")
target_link_libraries("${PROJECT_NAME}" PUBLIC "${OPENVR_LIB}")
target_link_libraries("${PROJECT_NAME}" PUBLIC "${LIBOVR_LIB}")
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/driver_files/src" PREFIX "Header Files" FILES ${HEADERS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/driver_files/src" PREFIX "Source Files" FILES ${SOURCES})
set_property(TARGET "${PROJECT_NAME}" PROPERTY CXX_STANDARD 17)

# Copy driver assets to output folder
add_custom_command(
    TARGET ${PROJECT_NAME}
    PRE_BUILD 
    COMMAND ${CMAKE_COMMAND} -E copy_directory 
    ${CMAKE_SOURCE_DIR}/driver_files/driver/ 
    $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

# Copy dll to output folder
add_custom_command(
    TARGET ${PROJECT_NAME} 
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy 
    $<TARGET_FILE:${PROJECT_NAME}>
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/${DRIVER_NAME}/bin/${PLATFORM_NAME}${PROCESSOR_ARCH}/$<TARGET_FILE_NAME:${PROJECT_NAME}>
)