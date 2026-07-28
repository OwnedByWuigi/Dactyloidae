file(
	GLOB
	EXAMPLES_GL_SOURCES
	*.c
)
find_package(OpenGL)

foreach(PATH IN LISTS EXAMPLES_GL_SOURCES)
	get_filename_component(TARGET ${PATH} NAME_WE)
    if(${TARGET} STREQUAL "glutlayer")
	    continue()
	endif()
	add_executable(${TARGET} MACOSX_BUNDLE ${PATH})
	target_link_libraries(
		${TARGET}
		PRIVATE
		Mw
		OpenGL::GL OpenGL::GLU
	)
	if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
		target_link_libraries(
			${TARGET}
			PRIVATE
			m
		)
	endif()
endforeach()
