cmake_minimum_required(VERSION 2.8)

project(seadrive-dfm-extension)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(DFM_EXTENSION REQUIRED dfm-extension)

pkg_check_modules(GLIB2 REQUIRED glib-2.0)

pkg_check_modules(LIBSEARPC REQUIRED libsearpc)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS
    "./*.h"
    "./*.cpp"
)

include_directories(${DFM_EXTENSION_INCLUDE_DIRS} ${GLIB2_INCLUDE_DIRS} ${LIBSEARPC_INCLUDE_DIRS})

add_library(${PROJECT_NAME} SHARED ${SRCS})

target_link_libraries(${PROJECT_NAME}
    PUBLIC ${DFM_EXTENSION_LIBRARIES} ${GLIB2_LIBRARIES} ${LIBSEARPC_LIBRARIES}
    )

include(GNUInstallDirs)
if(NOT DEFINED LIB_INSTALL_DIR)
    set(LIB_INSTALL_DIR ${CMAKE_INSTALL_FULL_LIBDIR})
endif()

if(NOT DEFINED DFM_EXT_PLUGIN_DIR)
    set(DFM_EXT_PLUGIN_DIR ${LIB_INSTALL_DIR}/dde-file-manager/plugins/extensions)
endif()

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${DFM_EXT_PLUGIN_DIR})
