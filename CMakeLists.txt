CMAKE_MINIMUM_REQUIRED(VERSION 2.8.9)

PROJECT(seadrive-gui)

SET(SEADRIVE_GUI_VERSION_MAJOR 3)
SET(SEADRIVE_GUI_VERSION_MINOR 0)
SET(SEADRIVE_GUI_VERSION_PATCH 15)
SET(PROJECT_VERSION "${SEADRIVE_GUI_VERSION_MAJOR}.${SEADRIVE_GUI_VERSION_MINOR}.${SEADRIVE_GUI_VERSION_PATCH}")
ADD_DEFINITIONS(-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_26)
ADD_DEFINITIONS(-DSEADRIVE_GUI_VERSION=${PROJECT_VERSION})

INCLUDE(FindPkgConfig)

IF (NOT (${CMAKE_BUILD_TYPE} MATCHES Release))
  SET(CMAKE_BUILD_TYPE Debug)
  ADD_DEFINITIONS(-DSEADRIVE_GUI_DEBUG)
ENDIF()

IF (APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
  SET(CMAKE_OSX_DEPLOYMENT_TARGET "11.0")
ENDIF()

option(BUILD_ENABLE_WARNINGS "Enable compiler warnings." ON)

OPTION(USE_QT_WEBKIT "use qt webkit" OFF)

MESSAGE("Build type: ${CMAKE_BUILD_TYPE}")

## build in PIC mode
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

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

IF (${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR ${CMAKE_SYSTEM_NAME} MATCHES "BSD")
    INCLUDE_DIRECTORIES(${QT_QTDBUS_INCLUDE_DIR})
    LINK_DIRECTORIES(${QT_QTDBUS_LIBRARIES})
    SET(EXTRA_LIBS ${EXTRA_LIBS} ${QT_QTDBUS_LIBRARIES})
ELSEIF (APPLE)
    SET(platform_specific_sources ${platform_specific_sources}
      src/application.cpp
      src/file-provider-mgr.cpp
      src/file-provider/file-provider.mm)
    SET(platform_specific_moc_headers ${platform_specific_moc_headers}
      src/application.h
      src/file-provider-mgr.h)
    ## Enforce ARC for this file, since ARC is only supported after the objc
    ## runtime changes in os x 10.10
    SET_SOURCE_FILES_PROPERTIES(src/utils/utils-mac.mm
      PROPERTIES COMPILE_FLAGS -fobjc-arc)

    FIND_LIBRARY(COCOA_LIBRARY Cocoa)
    MARK_AS_ADVANCED (COCOA_LIBRARY)
    FIND_LIBRARY(Sec_LIBRARY Security)
    MARK_AS_ADVANCED (Sec_LIBRARY)
    FIND_LIBRARY(FinderSync_LIBRARY FinderSync)
    FIND_LIBRARY(FileProvider_LIBRARY FileProvider)
    SET(EXTRA_LIBS ${EXTRA_LIBS} ${COCOA_LIBRARY} ${Sec_LIBRARY} ${FinderSync_LIBRARY} ${FileProvider_LIBRARY})
ENDIF()


####################
###### BEGIN: QT configuration
####################

FIND_PROGRAM(qmake_executable NAMES qmake qmake.exe)
EXECUTE_PROCESS(COMMAND
    bash -c "${qmake_executable} --version | grep -iE '^using qt version [0-9.]+' | awk '{print $4}'"
    OUTPUT_VARIABLE DETECTED_QT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE)
MESSAGE("Qt version: ${DETECTED_QT_VERSION}")

EXECUTE_PROCESS(COMMAND
    bash -c "qmake --version|grep -iE '^using qt version [0-9]'|awk '{print $4}'|grep -iE '^[0-9]'|awk -F . '{print $1}'"
    OUTPUT_VARIABLE QT_VERSION_MAJOR
    OUTPUT_STRIP_TRAILING_WHITESPACE)
MESSAGE("QT_VERSION_MAJOR: ${QT_VERSION_MAJOR}")

IF(USE_QT_WEBKIT)
    ADD_DEFINITIONS(-DSEAFILE_USE_WEBKIT)
    SET(WEBKIT_NAME "WebKit")
    SET(WEBKIT_WIDGETS_NAME "WebKitWidgets")
ELSE()
    SET(WEBKIT_NAME "WebEngine")
    SET(WEBENGINE_CORE "WebEngineCore")
    SET(WEBKIT_WIDGETS_NAME "WebEngineWidgets")
    SET(SHIB_EXTRA_HEADER "src/shib/shib-helper.h")
ENDIF()

IF(QT_VERSION_MAJOR EQUAL 6)
    SET(USE_QT_LIBRARIES Core Gui Widgets LinguistTools Network Test Core5Compat WebEngineCore WebEngineWidgets)
ELSE()
    SET(USE_QT_LIBRARIES Core Gui Widgets LinguistTools Network Test)
ENDIF()

IF (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    SET(USE_QT_LIBRARIES ${USE_QT_LIBRARIES} "DBus")
ENDIF()

IF(QT_VERSION_MAJOR EQUAL 6)
    FIND_PACKAGE(Qt6 COMPONENTS ${USE_QT_LIBRARIES} REQUIRED)
ELSE()
    FIND_PACKAGE(Qt5 REQUIRED ${USE_QT_LIBRARIES})
ENDIF()

SET(QT_LUPDATE_EXECUTABLE ${Qt5_LUPDATE_EXECUTABLE})

IF(QT_VERSION_MAJOR EQUAL 6)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
ELSE()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
ENDIF()

# for OS X, we requires libc++ instead
if (APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    IF(QT_VERSION_MAJOR EQUAL 6)
        SET(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++17")
    ELSE()
        SET(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++11")
    ENDIF()
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
  src/sync-command.h
  src/network-mgr.h
  src/remote-wipe-service.h
  src/account-info-service.h
  src/rpc/rpc-client.h
  src/rpc/rpc-server.h
  src/seadrive-gui.h
  src/settings-mgr.h

  src/traynotificationwidget.h
  src/traynotificationmanager.h

  src/ui/filter-menu.h
  src/ui/init-sync-dialog.h
  src/ui/loading-view.h
  src/ui/login-dialog.h
  src/ui/seafilelink-dialog.h
  src/ui/search-bar.h
  src/ui/search-dialog.h
  src/ui/settings-dialog.h
  src/ui/sharedlink-dialog.h
  src/ui/uploadlink-dialog.h
  src/ui/sync-errors-dialog.h
  src/ui/tray-icon.h
  src/ui/about-dialog.h
  src/ui/encrypted-repos-dialog.h
  src/ui/transfer-progress-dialog.h
  src/ui/uninstall-helper-dialog.h
  src/open-local-helper.h

  third_party/QtAwesome/QtAwesome.h

  src/shib/shib-login-dialog.h
  ${SHIB_EXTRA_HEADER}
  ${platform_specific_moc_headers}
)

IF (${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR ${CMAKE_SYSTEM_NAME} MATCHES "BSD")
    LIST(APPEND moc_headers
        src/ext-handler.h)
ENDIF()

IF(QT_VERSION_MAJOR EQUAL 6)
    QT6_WRAP_CPP(moc_output ${moc_headers})
ELSE()
    QT5_WRAP_CPP(moc_output ${moc_headers})
ENDIF()

SET(ui_files
  ui/uninstall-helper-dialog.ui
  ui/filter-menu.ui
  ui/init-sync-dialog.ui
  ui/login-dialog.ui
  ui/settings-dialog.ui
  ui/about-dialog.ui
)
IF(QT_VERSION_MAJOR EQUAL 6)
    QT6_WRAP_UI(ui_output ${ui_files})
ELSE()
    QT5_WRAP_UI(ui_output ${ui_files})
ENDIF()

# resources files
IF(QT_VERSION_MAJOR EQUAL 6)
  QT6_ADD_RESOURCES(
    resources_ouput
    seadrive-gui.qrc
    third_party/QtAwesome/QtAwesome.qrc
  )
ELSE()
  QT5_ADD_RESOURCES(
    resources_ouput
    seadrive-gui.qrc
    third_party/QtAwesome/QtAwesome.qrc
  )
ENDIF()

# MESSAGE("moc output: ${moc_output}")
# MESSAGE("ui output: ${ui_output}")

####################
###### END: QT configuration
####################

####################
###### BEGIN: pthread support is required explicitly on linux
####################

IF(NOT APPLE)
  SET(CMAKE_THREAD_PREFER_PTHREAD ON)
  INCLUDE(FindThreads)
  LINK_LIBRARIES(${CMAKE_THREAD_LIBS_INIT})
ENDIF(NOT APPLE)

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
  src/sync-command.cpp
  src/network-mgr.cpp
  src/remote-wipe-service.cpp
  src/account-info-service.cpp

  src/rpc/rpc-client.cpp
  src/rpc/rpc-server.cpp
  src/rpc/sync-error.cpp
  src/rpc/transfer-progress.cpp

  src/seadrive-gui.cpp
  src/settings-mgr.cpp

  src/shib/shib-login-dialog.cpp

  src/traynotificationwidget.cpp
  src/traynotificationmanager.cpp

  src/ui/filter-menu.cpp
  src/ui/init-sync-dialog.cpp
  src/ui/loading-view.cpp
  src/ui/login-dialog.cpp
  src/ui/seafilelink-dialog.cpp
  src/ui/search-bar.cpp
  src/ui/search-dialog.cpp
  src/ui/settings-dialog.cpp
  src/ui/sharedlink-dialog.cpp
  src/ui/uploadlink-dialog.cpp
  src/ui/sync-errors-dialog.cpp
  src/ui/tray-icon.cpp
  src/ui/about-dialog.cpp
  src/ui/encrypted-repos-dialog.cpp
  src/ui/transfer-progress-dialog.cpp
  src/ui/uninstall-helper-dialog.cpp

  src/open-local-helper.cpp

  src/utils/uninstall-helpers.cpp
  ${platform_specific_sources}

  src/i18n.cpp
  third_party/QtAwesome/QtAwesome.cpp
)

IF (${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR ${CMAKE_SYSTEM_NAME} MATCHES "BSD")
    LIST(APPEND seadrive_gui_sources
        src/ext-handler.cpp)
ENDIF()



INCLUDE_DIRECTORIES(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/QtAwesome
  )

FOREACH(USE_QT_LIBRARY ${USE_QT_LIBRARIES})
INCLUDE_DIRECTORIES(
IF(QT_VERSION_MAJOR EQUAL 6)
    ${Qt6${USE_QT_LIBRARY}_INCLUDE_DIRS}
ELSE()
    ${Qt5${USE_QT_LIBRARY}_INCLUDE_DIRS}
ENDIF()
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
        IF(QT_VERSION_MAJOR EQUAL 6)
            QT6_WRAP_CPP(MOC_OUTPUT ${ARG_MOC_HEADERS})
        ELSE()
            QT5_WRAP_CPP(MOC_OUTPUT ${ARG_MOC_HEADERS})
        ENDIF()
    ENDIF()
    IF(ARG_UI_FILES)
        IF(QT_VERSION_MAJOR EQUAL 6)
            QT6_WRAP_UI(UI_OUTPUT ${ARG_UI_FILES})
        ELSE()
            QT5_WRAP_UI(UI_OUTPUT ${ARG_UI_FILES})
        ENDIF()
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
    src/utils/api-utils.cpp
    src/utils/paint-utils.cpp
    src/utils/file-utils.cpp
    src/utils/translate-commit-desc.cpp
    src/utils/json-utils.cpp
    src/utils/log.c
    src/utils/stl.cpp
    src/utils/i18n-utils.cpp
    )

IF (${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR ${CMAKE_SYSTEM_NAME} MATCHES "BSD")
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
    COMMAND ${QT_LUPDATE_EXECUTABLE}
        -locations none ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/ui
        -ts "${PROJECT_SOURCE_DIR}/i18n/seadrive_${LANGUAGE}.ts"
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

  SET(SEADRIVE_TS_TARGETS ${SEADRIVE_TS_TARGETS} ${TS_TARGET})

ENDFOREACH(LANGUAGE ${LANGUAGES})

IF(QT_VERSION_MAJOR EQUAL 6)
    QT6_ADD_TRANSLATION(qm_files ${LANGUAGE_TS_FILES})
ELSE()
    QT5_ADD_TRANSLATION(qm_files ${LANGUAGE_TS_FILES})
ENDIF()

ADD_CUSTOM_TARGET(update-ts DEPENDS ${SEADRIVE_TS_TARGETS})

####################
###### end: translations
####################

####################
###### start: freedesktop files
####################

install(DIRECTORY
  ${CMAKE_SOURCE_DIR}/data/icons/16x16
  ${CMAKE_SOURCE_DIR}/data/icons/22x22
  ${CMAKE_SOURCE_DIR}/data/icons/24x24
  ${CMAKE_SOURCE_DIR}/data/icons/32x32
  ${CMAKE_SOURCE_DIR}/data/icons/48x48
  ${CMAKE_SOURCE_DIR}/data/icons/128x128
  ${CMAKE_SOURCE_DIR}/data/icons/scalable
  DESTINATION share/icons/hicolor
)
install(FILES
  ${CMAKE_SOURCE_DIR}/data/seadrive.desktop
  DESTINATION share/applications
)

install(FILES
  ${CMAKE_SOURCE_DIR}/data/icons/128x128/apps/seadrive.png
  DESTINATION share/pixmaps
)


####################
###### end: freedesktop files
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
  ${SPARKLE_LIBS}
  ${SC_LIBS}

  ${GLIB2_LIBRARIES}
  ${JANSSON_LIBRARIES}
  ${LIBSEARPC_LIBRARIES}
  ${OPENSSL_LIBRARIES}
  ${QT_LIBRARIES}
  ${SQLITE3_LIBRARIES}

  ${EXTRA_LIBS}
)

IF(QT_VERSION_MAJOR EQUAL 6)
    FIND_PACKAGE(Qt6 COMPONENTS Core Gui Widgets Network ${WEBKIT_WIDGETS_NAME} ${WEBENGINE_CORE}  REQUIRED)
    TARGET_LINK_LIBRARIES(seadrive-gui Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::${WEBKIT_WIDGETS_NAME} Qt6::${WEBENGINE_CORE} Qt6::Core5Compat)

ELSE()
    QT5_USE_MODULES(seadrive-gui Core Gui Widgets Network)
    QT5_USE_MODULES(seadrive-gui ${WEBKIT_NAME} ${WEBKIT_WIDGETS_NAME})
ENDIF()

## QtBus
IF (${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR ${CMAKE_SYSTEM_NAME} MATCHES "BSD")
IF(QT_VERSION_MAJOR EQUAL 6)
    FIND_PACKAGE(Qt6 COMPONENTS DBus REQUIRED)
    TARGET_LINK_LIBRARIES(seadrive-gui PRIVATE Qt6::DBus)
ELSE()
    TARGET_LINK_LIBRARIES(seadrive-gui ${QT_QTDBUS_LIBRARIES})
    QT5_USE_MODULES(seadrive-gui ${USE_QT_LIBRARIES})
ENDIF()

ENDIF()

### Xcode-related, build as a osx bundle
IF(CMAKE_GENERATOR STREQUAL Xcode)
  ADD_DEFINITIONS(-DXCODE_APP)
  SET_TARGET_PROPERTIES(seadrive-gui PROPERTIES
    MACOSX_BUNDLE true
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
  )
  SET(RESOURCES_DIR ${CMAKE_CURRENT_BINARY_DIR}/\${CONFIGURATION}/seadrive-gui.app/Contents/Resources)
  ADD_CUSTOM_COMMAND(TARGET seadrive-gui
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory ${RESOURCES_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/seadrive.icns ${RESOURCES_DIR}/seadrive-icon.icns
      COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/locked-by-me.icns ${RESOURCES_DIR}/locked-by-me.icns
      COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/locked-by-user.icns ${RESOURCES_DIR}/locked-by-user.icns
  )
  SET(CMAKE_XCODE_ATTRIBUTE_GCC_GENERATE_DEBUGGING_SYMBOLS "YES")
  SET(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym")
  SET(CMAKE_XCODE_ATTRIBUTE_GCC_ENABLE_PASCAL_STRINGS "NO")
  SET(CMAKE_XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC "YES")
  SET(CMAKE_XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME "TRUE")
ENDIF()


SET(ARCHIVE_NAME ${CMAKE_PROJECT_NAME}-${PROJECT_VERSION})
FIND_PROGRAM(git_executable NAMES git git.exe git.cmd)
ADD_CUSTOM_TARGET(dist
    COMMAND ${git_executable} archive -v --prefix=${ARCHIVE_NAME}/ HEAD
        | gzip > ${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}.tar.gz
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
