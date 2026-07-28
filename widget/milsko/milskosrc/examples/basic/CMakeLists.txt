file(
 GLOB
 EXAMPLES_BASIC_SOURCES
 *.c
)

foreach(PATH IN LISTS EXAMPLES_BASIC_SOURCES)
  get_filename_component(TARGET ${PATH} NAME_WE)
  if(CMAKE_SYSTEM_NAME MATCHES Retro)
    add_application(${TARGET} ${PATH})
  else()
    add_executable(${TARGET} MACOSX_BUNDLE ${PATH})
  endif()
  target_link_libraries(
  ${TARGET}
  PRIVATE
  Mw
 )
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_link_libraries(
   ${TARGET}
   PRIVATE
   m
  )
  endif()
endforeach()
