SET(qlgen_shared_srcdir ${CMAKE_CURRENT_LIST_DIR}/src)
SET(shared_srcfiles
  qlgen-client.mm
  qlgen.mm
  system-qlgen.mm
  gen-thumbnail.m
  gen-preview.m
  utils.mm
  NSDictionary+SFJSON.m
)
FOREACH(srcfile ${shared_srcfiles})
    SET(qlgen_shared_sources ${qlgen_shared_sources} ${qlgen_shared_srcdir}/${srcfile})
ENDFOREACH()

## Build with warnings
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fobjc-arc -Wall -Wextra -Wsign-compare -Wno-long-long -Wno-unused-parameter")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fobjc-arc -Wall -Wextra -Wsign-compare -Wno-long-long -Wno-unused-parameter -Woverloaded-virtual")

## Find the dependencies
FIND_LIBRARY(AppKit_LIBRARY AppKit)
FIND_LIBRARY(WebKit_LIBRARY WebKit)
FIND_LIBRARY(QuickLook_LIBRARY QuickLook)
FIND_LIBRARY(ApplicationServices_LIBRARY ApplicationServices)

INCLUDE_DIRECTORIES(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${qlgen_shared_srcdir}
)