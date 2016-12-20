CMAKE_MINIMUM_REQUIRED(VERSION 2.8.9)

PROJECT(seadrive-gui)

SET(SEADRIVE_GUI_VERSION_MAJOR 0)
SET(SEADRIVE_GUI_VERSION_MINOR 4)
SET(SEADRIVE_GUI_VERSION_PATCH 2)
SET(PROJECT_VERSION "${SEADRIVE_GUI_VERSION_MAJOR}.${SEADRIVE_GUI_VERSION_MINOR}.${SEADRIVE_GUI_VERSION_PATCH}")
ADD_DEFINITIONS(-DSEADRIVE_GUI_VERSION=${PROJECT_VERSION})

INCLUDE(FindPkgConfig)

IF (NOT (${CMAKE_BUILD_TYPE} MATCHES Release))
  SET(CMAKE_BUILD_TYPE Debug)
ENDIF()

IF (APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
  SET(CMAKE_OSX_DEPLOYMENT_TARGET "10.7")
ENDIF()

option(BUILD_ENABLE_WARNINGS "Enable compiler warnings." ON)

MESSAGE("Build type: ${CMAKE_BUILD_TYPE}")

## build in PIC mode
IF (NOT WIN32)
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
ENDIF()

## Setup warnings ouput if enabled
## but don't warning on unsupported warnings
if(BUILD_ENABLE_WARNINGS)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wsign-compare -Wno-long-long -Wno-unused-parameter")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wsign-compare -Wno-long-long -Wno-unused-parameter -Woverloaded-virtual")
  if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unknown-warning-option")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-warning-option -Wno-inconsistent-missing-override")
  endif()
endif()

## color diagnostics fix
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fcolor-diagnostics")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcolor-diagnostics")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  execute_process(COMMAND ${CMAKE_CXX_COMPILER}
    -dumpversion OUTPUT_VARIABLE GCC_VERSION)
  if(GCC_VERSION VERSION_GREATER 4.9 OR GCC_VERSION VERSION_EQUAL 4.9)
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fdiagnostics-color=always")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")
  endif()
endif()

IF (WIN32)
    CONFIGURE_FILE(
      ${CMAKE_SOURCE_DIR}/seadrive-gui.rc.in
      ${CMAKE_BINARY_DIR}/seadrive-gui.rc
    )
    SET(EXTRA_LIBS ${EXTRA_LIBS} psapi ws2_32 shlwapi mpr)
    SET(EXTRA_SOURCES ${EXTRA_SOURCES} ${CMAKE_BINARY_DIR}/seadrive-gui.rc)
    IF (${CMAKE_BUILD_TYPE} MATCHES Release)
      SET(GUI_TYPE WIN32)
    ENDIF()
    SET(platform_specific_moc_headers ${platform_specific_moc_headers} src/ext-handler.h src/ui/disk-letter-dialog.h)
    SET(platform_specific_sources ${platform_specific_sources} src/ext-handler.cpp src/ui/disk-letter-dialog.cpp)
    SET(platform_specific_ui_files ${platform_specific_ui_files} ui/disk-letter-dialog.ui)
ELSEIF (${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR ${CMAKE_SYSTEM_NAME} MATCHES "BSD")
    INCLUDE_DIRECTORIES(${QT_QTDBUS_INCLUDE_DIR})
    LINK_DIRECTORIES(${QT_QTDBUS_LIBRARIES})
    SET(EXTRA_LIBS ${EXTRA_LIBS} ${QT_QTDBUS_LIBRARIES})
ELSEIF (APPLE)
    SET(platform_specific_sources ${platform_specific_sources} src/application.cpp)
    SET(platform_specific_moc_headers ${platform_specific_moc_headers} src/application.h)
    ## Enforce ARC for this file, since ARC is only supported after the objc
    ## runtime changes in os x 10.10
    SET_SOURCE_FILES_PROPERTIES(src/utils/utils-mac.mm
      PROPERTIES COMPILE_FLAGS -fobjc-arc)

    FIND_LIBRARY(COCOA_LIBRARY Cocoa)
    MARK_AS_ADVANCED (COCOA_LIBRARY)
    FIND_LIBRARY(Sec_LIBRARY Security)
    MARK_AS_ADVANCED (Sec_LIBRARY)
    SET(EXTRA_LIBS ${EXTRA_LIBS} ${COCOA_LIBRARY} ${Sec_LIBRARY})
ENDIF()

IF(APPLE)
    ADD_DEFINITIONS(-DHAVE_FINDER_SYNC_SUPPORT)
    SET(platform_specific_moc_headers ${platform_specific_moc_headers} src/finder-sync/finder-sync-host.h)
    SET(platform_specific_sources ${platform_specific_sources}
        src/finder-sync/finder-sync.cpp src/finder-sync/finder-sync-listener.mm
        src/finder-sync/finder-sync-host.cpp)
    SET_SOURCE_FILES_PROPERTIES(src/finder-sync/finder-sync-listener.mm
        PROPERTIES COMPILE_FLAGS -fobjc-arc)
ENDIF()


####################
###### BEGIN: QT configuration
####################
SET(QT_VERSION_MAJOR 5)

# Qt 5.6.0 removes QtWebKit, we need to use QtWebEnigne. First we detect the
# current qt version, then use proper name based on the version. However we
# have to keep using QtWebKit on windows because QtWebEngine can't be
# compiled in msys2/mingw (QtWebEnigne is based on chrome, which has to be
# compiled with MSVC.)
FIND_PROGRAM(qmake_executable NAMES qmake qmake.exe)
EXECUTE_PROCESS(COMMAND
    bash -c "${qmake_executable} --version | grep -iE '^using qt version [0-9.]+' | awk '{print $4}'"
    OUTPUT_VARIABLE DETECTED_QT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE)
MESSAGE("qt5 version: ${DETECTED_QT_VERSION}")
IF(WIN32 OR DETECTED_QT_VERSION VERSION_LESS 5.6.0)
    ADD_DEFINITIONS(-DSEAFILE_USE_WEBKIT)
    SET(WEBKIT_NAME "WebKit")
    SET(WEBKIT_WIDGETS_NAME "WebKitWidgets")
ELSE()
    SET(WEBKIT_NAME "WebEngine")
    SET(WEBENGINE_CORE "WebEngineCore")
    SET(WEBKIT_WIDGETS_NAME "WebEngineWidgets")
    SET(SHIB_EXTRA_HEADER "src/shib/shib-helper.h")
ENDIF()

SET(USE_QT_LIBRARIES
    Core Gui Widgets LinguistTools Network Test
    )

IF (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    SET(USE_QT_LIBRARIES ${USE_QT_LIBRARIES} "DBus")
ENDIF()

FIND_PACKAGE(Qt5 REQUIRED ${USE_QT_LIBRARIES})
SET(QT_LUPDATE_EXECUTABLE ${Qt5_LUPDATE_EXECUTABLE})

if (CYGWIN OR MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

# for OS X, we requires libc++ instead
if (APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    SET(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++11")
    SET(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")
endif()

# MOC FILES
SET(moc_headers
  src/account-mgr.h

  src/api/api-client.h
  src/api/api-request.h
  src/api/requests.h

  src/auto-login-service.h
  src/daemon-mgr.h
  src/message-poller.h
  src/network-mgr.h
  src/remote-wipe-service.h
  src/rpc/rpc-client.h
  src/seadrive-gui.h
  src/settings-mgr.h

  src/traynotificationwidget.h
  src/traynotificationmanager.h

  src/ui/init-sync-dialog.h
  src/ui/login-dialog.h
  src/ui/seafilelink-dialog.h
  src/ui/settings-dialog.h
  src/ui/sharedlink-dialog.h
  src/ui/tray-icon.h

  src/shib/shib-login-dialog.h
  ${SHIB_EXTRA_HEADER}
  ${platform_specific_moc_headers}
)
QT5_WRAP_CPP(moc_output ${moc_headers})

SET(ui_files
  ui/init-sync-dialog.ui
  ui/login-dialog.ui
  ui/settings-dialog.ui
  ${platform_specific_ui_files}
)
QT5_WRAP_UI(ui_output ${ui_files})

# resources files
QT5_ADD_RESOURCES(resources_ouput seadrive-gui.qrc)

# MESSAGE("moc output: ${moc_output}")
# MESSAGE("ui output: ${ui_output}")

####################
###### END: QT configuration
####################

####################
###### BEGIN: pthread support is required explicitly on linux
####################

IF(NOT WIN32 AND NOT APPLE)
  SET(CMAKE_THREAD_PREFER_PTHREAD ON)
  INCLUDE(FindThreads)
  LINK_LIBRARIES(${CMAKE_THREAD_LIBS_INIT})
ENDIF(NOT WIN32 AND NOT APPLE)

####################
###### BEGIN: other libraries configuration
####################
FIND_PACKAGE(PkgConfig REQUIRED)

PKG_CHECK_MODULES(GLIB2 REQUIRED glib-2.0>=2.16.0)

PKG_CHECK_MODULES(JANSSON REQUIRED jansson>=2.0)

PKG_CHECK_MODULES(LIBSEARPC REQUIRED libsearpc>=1.0)

PKG_CHECK_MODULES(OPENSSL REQUIRED openssl>=0.98)

PKG_CHECK_MODULES(SQLITE3 REQUIRED sqlite3>=3.0.0)

####################
###### END: other libraries configuration
####################


# c/cpp sources
SET(seadrive_gui_sources
  src/account-mgr.cpp
  src/account.cpp

  src/api/api-client.cpp
  src/api/api-error.cpp
  src/api/api-request.cpp
  src/api/commit-details.cpp
  src/api/contact-share-info.cpp
  src/api/event.cpp
  src/api/requests.cpp
  src/api/server-repo.cpp
  src/api/seaf-dirent.cpp
  src/api/starred-file.cpp

  src/auto-login-service.cpp
  src/daemon-mgr.cpp
  src/main.cpp
  src/message-poller.cpp
  src/network-mgr.cpp
  src/remote-wipe-service.cpp

  src/rpc/rpc-client.cpp

  src/seadrive-gui.cpp
  src/settings-mgr.cpp

  src/shib/shib-login-dialog.cpp

  src/traynotificationwidget.cpp
  src/traynotificationmanager.cpp

  src/ui/init-sync-dialog.cpp
  src/ui/login-dialog.cpp
  src/ui/seafilelink-dialog.cpp
  src/ui/settings-dialog.cpp
  src/ui/sharedlink-dialog.cpp
  src/ui/tray-icon.cpp
  ${platform_specific_sources}

  src/i18n.cpp
)

INCLUDE_DIRECTORIES(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

FOREACH(USE_QT_LIBRARY ${USE_QT_LIBRARIES})
INCLUDE_DIRECTORIES(
${Qt5${USE_QT_LIBRARY}_INCLUDE_DIRS}
)
ENDFOREACH()

INCLUDE_DIRECTORIES(
  ${GLIB2_INCLUDE_DIRS}
  ${JANSSON_INCLUDE_DIRS}
  ${LIBSEARPC_INCLUDE_DIRS}
  ${OPENSSL_INCLUDE_DIRS}
  ${SQLITE3_INCLUDE_DIRS}
)

LINK_DIRECTORIES(
  ${GLIB2_LIBRARY_DIRS}
  ${JANSSON_LIBRARRY_DIRS}
  ${LIBSEARPC_LIBRARY_DIRS}
  ${OPENSSL_LIBRARY_DIRS}
  ${QT_LIBRARY_DIR}
  ${SQLITE3_LIBRARRY_DIRS}
)

####################
###### begin: lib
####################

# Helper Function to Add Seafile-Client Library
# Usage:
# ADD_SC_LIBRARY (name sources
#     MOC_HEADERS moc_headers
#     UI_FILES ui_files
#     DEPENDS part
#     LINK_LIBS libs
#     )
FUNCTION(ADD_SC_LIBRARY name)
    MESSAGE(STATUS "Found internal library: ${name}")
    CMAKE_PARSE_ARGUMENTS(ARG
      ""
      ""
      "MOC_HEADERS;UI_FILES;DEPENDS;LINK_LIBS"
      ${ARGN})
    IF(ARG_MOC_HEADERS)
        QT5_WRAP_CPP(MOC_OUTPUT ${ARG_MOC_HEADERS})
    ENDIF()
    IF(ARG_UI_FILES)
        QT5_WRAP_UI(UI_OUTPUT ${ARG_UI_FILES})
    ENDIF()

    ADD_LIBRARY(${name} STATIC ${ARG_UNPARSED_ARGUMENTS}
      ${MOC_OUTPUT} ${UI_OUTPUT})

    TARGET_LINK_LIBRARIES(${name} ${QT_LIBRARIES}
        ${SQLITE3_LIBRARIES} ${JANSSON_LIBRARIES}
        ${EXTRA_LIBS} -lglib-2.0 ${ARG_LINK_LIBS})
ENDFUNCTION(ADD_SC_LIBRARY)

# library utils
LIST(APPEND utils_sources
    src/utils/utils.cpp
    src/utils/rsa.cpp
    src/utils/api-utils.cpp
    src/utils/paint-utils.cpp
    src/utils/file-utils.cpp
    src/utils/translate-commit-desc.cpp
    src/utils/json-utils.cpp
    src/utils/log.c
    src/utils/stl.cpp
    )
IF (WIN32)
    LIST(APPEND utils_sources
        src/utils/process-win.cpp
        src/utils/registry.cpp
        src/utils/utils-win.cpp
        )
ELSEIF (${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR ${CMAKE_SYSTEM_NAME} MATCHES "BSD")
    LIST(APPEND utils_sources
        src/utils/process-linux.cpp
        )
ELSEIF(APPLE)
    LIST(APPEND utils_sources
        src/utils/process-mac.cpp
        src/utils/utils-mac.mm)
ENDIF()

ADD_SC_LIBRARY(utils ${utils_sources})

SET(SC_LIBS utils)

####################
###### end: lib
####################

####################
###### start: translations
####################

SET(SEAFILE_TRANSLATE_SOURCES ${seadrive_gui_sources} ${utils_sources} ${moc_output} ${ui_output})
SET(LANGUAGES
  ca
  de_DE
  en
  es
  es_AR
  es_MX
  fr_FR
  he_IL
  hu_HU
  is
  it
  ko_KR
  nl_BE
  pl_PL
  pt_BR
  pt_PT
  ru
  sk_SK
  uk
  zh_CN
  zh_TW
  tr
  nl_NL
  lv
  ja
  sv
  cs_CZ
  el_GR
  nb_NO
  )

SET(LANGUAGE_TS_FILES)
SET(SEADRIVE_TS_TARGETS)
FOREACH(LANGUAGE ${LANGUAGES})
  SET(TS_FILE "${PROJECT_SOURCE_DIR}/i18n/seadrive_${LANGUAGE}.ts")
  SET(LANGUAGE_TS_FILES ${LANGUAGE_TS_FILES} ${TS_FILE})
  SET_SOURCE_FILES_PROPERTIES(${TS_FILE} PROPERTIES OUTPUT_LOCATION "${PROJECT_SOURCE_DIR}/i18n")

  SET(TS_TARGET "${LANGUAGE}_ts")
  ADD_CUSTOM_TARGET(${TS_TARGET}
    COMMAND ${QT_LUPDATE_EXECUTABLE} ${SEAFILE_TRANSLATE_SOURCES}
        -ts "${PROJECT_SOURCE_DIR}/i18n/seadrive_${LANGUAGE}.ts"
        -I "${PROJECT_SOURCE_DIR}/src"
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

  SET(SEADRIVE_TS_TARGETS ${SEADRIVE_TS_TARGETS} ${TS_TARGET})

ENDFOREACH(LANGUAGE ${LANGUAGES})

QT5_ADD_TRANSLATION(qm_files ${LANGUAGE_TS_FILES})

ADD_CUSTOM_TARGET(update-ts DEPENDS ${SEADRIVE_TS_TARGETS})

####################
###### end: translations
####################



ADD_EXECUTABLE(seadrive-gui ${GUI_TYPE}
  ${seadrive_gui_sources}
  ${moc_output}
  ${ui_output}
  ${resources_ouput}
  ${EXTRA_SOURCES}
)

INSTALL(TARGETS seadrive-gui DESTINATION bin)

TARGET_LINK_LIBRARIES(seadrive-gui
  ${SC_LIBS}

  ${GLIB2_LIBRARIES}
  ${JANSSON_LIBRARIES}
  ${LIBSEARPC_LIBRARIES}
  ${OPENSSL_LIBRARIES}
  ${QT_LIBRARIES}
  ${SQLITE3_LIBRARIES}

  ${EXTRA_LIBS}
)

QT5_USE_MODULES(seadrive-gui Core Gui Widgets Network)
QT5_USE_MODULES(seadrive-gui ${WEBKIT_NAME} ${WEBKIT_WIDGETS_NAME})

## QtBus
IF (${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR ${CMAKE_SYSTEM_NAME} MATCHES "BSD")
  TARGET_LINK_LIBRARIES(seadrive-gui
    ${QT_QTDBUS_LIBRARIES}
  )
  QT5_USE_MODULES(seadrive-gui ${USE_QT_LIBRARIES})
ENDIF()

### Xcode-related, build as a osx bundle
IF(CMAKE_GENERATOR STREQUAL Xcode)
  ADD_DEFINITIONS(-DXCODE_APP)
  SET_TARGET_PROPERTIES(seadrive-gui PROPERTIES
    MACOSX_BUNDLE true
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
  )
  FIND_PROGRAM(seadrive seadrive)
  SET(RESOURCES_DIR ${CMAKE_CURRENT_BINARY_DIR}/\${CONFIGURATION}/seadrive-gui.app/Contents/Resources)
  ADD_CUSTOM_COMMAND(TARGET seadrive-gui
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory ${RESOURCES_DIR}
      # We rename the icon because: when we have both the program "seadrive" and
      # the icon "seadrive.icns" in "Resources" dir, the icon won't be able to
      # be displayed as the bundle icon.
      COMMAND ${CMAKE_COMMAND} -E copy seadrive.icns ${RESOURCES_DIR}/seadrive-icon.icns
      COMMAND ${CMAKE_COMMAND} -E copy ${seadrive} ${RESOURCES_DIR}/.
  )
  SET(CMAKE_XCODE_ATTRIBUTE_GCC_GENERATE_DEBUGGING_SYMBOLS "YES")
  SET(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym")
  SET(CMAKE_XCODE_ATTRIBUTE_GCC_ENABLE_PASCAL_STRINGS "NO")
  SET(CMAKE_XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC "YES")
ENDIF()


SET(ARCHIVE_NAME ${CMAKE_PROJECT_NAME}-${PROJECT_VERSION})
FIND_PROGRAM(git_executable NAMES git git.exe git.cmd)
ADD_CUSTOM_TARGET(dist
    COMMAND ${git_executable} archive -v --prefix=${ARCHIVE_NAME}/ HEAD
        | gzip > ${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}.tar.gz
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
