CMAKE_MINIMUM_REQUIRED(VERSION 2.8.11)

PROJECT(seadrive-helper)
SET(PROJECT_VERSION "1.0.0")

IF (NOT (${CMAKE_BUILD_TYPE} MATCHES Release))
  SET(CMAKE_BUILD_TYPE Debug)
ENDIF()

MESSAGE(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

IF (NOT APPLE)
    MESSAGE(FATAL_ERROR "Support only on OS X")
ENDIF()

## Build with warnings
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wsign-compare -Wno-long-long -Wno-unused-parameter")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wsign-compare -Wno-long-long -Wno-unused-parameter -Woverloaded-virtual")

## Find the dependencies
FIND_LIBRARY(COCOA_LIBRARY Cocoa)
MARK_AS_ADVANCED (COCOA_LIBRARY)
FIND_LIBRARY(Sec_LIBRARY Security)
MARK_AS_ADVANCED (Sec_LIBRARY)
FIND_LIBRARY(SM_LIBRARY ServiceManagement)
MARK_AS_ADVANCED (SM_LIBRARY)
FIND_LIBRARY(GHODictionary_LIBRARY GHODictionary PATHS ${CMAKE_SOURCE_DIR}/../third_party/carthage)
FIND_LIBRARY(MPMessagePack_LIBRARY MPMessagePack PATHS ${CMAKE_SOURCE_DIR}/../third_party/carthage)
SET(OSX_LIBS ${EXTRA_LIBS} ${COCOA_LIBRARY} ${Sec_LIBRARY} ${SM_LIBRARY} ${GHODictionary_LIBRARY} ${MPMessagePack_LIBRARY})

SET(CMAKE_EXE_LINKER_FLAGS "-fobjc-arc -fobjc-link-runtime ${CMAKE_EXE_LINKER_FLAGS}")

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})
SET(HELPER_SRC
  helper.m
  helper-main.m
  )

ADD_EXECUTABLE(seadrive-helper ${HELPER_SRC})

SET_TARGET_PROPERTIES(seadrive-helper PROPERTIES
  OUTPUT_NAME "com.seafile.seadrive.helper"
  XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET "10.7"
  XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC "YES"
  XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++11"
  XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++"
  XCODE_ATTRIBUTE_DEVELOPMENT_LANGUAGE "English"
  )


TARGET_LINK_LIBRARIES(seadrive-helper
  ${OSX_LIBS}
  "-sectcreate __TEXT __info_plist ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist -sectcreate __TEXT __launchd_plist ${CMAKE_CURRENT_SOURCE_DIR}/launchd.plist"
)