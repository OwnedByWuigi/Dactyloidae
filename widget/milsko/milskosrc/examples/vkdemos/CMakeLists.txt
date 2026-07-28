file(
 GLOB
 EXAMPLES_VULKAN_SOURCES
 *.c
)
find_package(Vulkan)

if(${Vulkan_FOUND})
  foreach(PATH IN LISTS EXAMPLES_VULKAN_SOURCES)
    get_filename_component(TARGET ${PATH} NAME_WE)
    add_executable(${TARGET} MACOSX_BUNDLE ${PATH})
    target_include_directories(
  ${TARGET}
  PRIVATE
  ${Vulkan_INCLUDE_DIR}
 )
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
endif()
