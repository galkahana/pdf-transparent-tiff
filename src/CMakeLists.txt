project(PDFModernTiff)
cmake_minimum_required (VERSION 3.10)

include_directories (${PDFWriter_SOURCE_DIR})
include_directories (${LibTiff_SOURCE_DIR}) # why dont include dirs works?!

add_executable(PDFModernTiff 
    lib/ModernTiffImageHandler.cpp
    lib/ModernTiffImageHandler.h
    cli.cpp
)

add_dependencies(PDFModernTiff PDFWriter)
add_dependencies(PDFModernTiff LibTiff)



target_link_libraries (PDFModernTiff PDFWriter)
target_link_libraries (PDFModernTiff ${LIBAESGM_LDFLAGS})
target_link_libraries (PDFModernTiff ${FREETYPE_LDFLAGS})
target_link_libraries (PDFModernTiff ${LIBJPEG_LDFLAGS})
target_link_libraries (PDFModernTiff ${ZLIB_LDFLAGS})
target_link_libraries (PDFModernTiff ${LIBTIFF_LDFLAGS})
target_link_libraries (PDFModernTiff ${LIBPNG_LDFLAGS})

if(APPLE)
	set(CMAKE_EXE_LINKER_FLAGS "-framework CoreFoundation")
endif(APPLE)

install(TARGETS PDFModernTiff
    RUNTIME DESTINATION bin COMPONENT executable
)