option(TOOLCHAIN_LIBDRAGON "Use LIBDRAGON" ON) #only set if this file is called~?

set(LIBDRAGON_PREFIX "/opt/libdragon/mips64-elf")

include_directories(
	${LIBDRAGON_PREFIX}/include
)

link_directories(
	${LIBDRAGON_PREFIX}/lib/
)

link_libraries(
	dragon
	c
	m
	stdc++
	dragonsys
)

# Inform cmake that we are compiling for N64
set(N64 ON)
set(WIN32 OFF)

# Add a macro for GCC so we can customize code
add_definitions(-D_N64)

# set the necessary tools we need for building the rom
set(N64_TOOL	       	${LIBDRAGON_PREFIX}/bin/n64tool)
set(CHECKSUM_TOOL       ${LIBDRAGON_PREFIX}/bin/chksum64)
set(OBJCOPY_TOOL        ${LIBDRAGON_PREFIX}/bin/mips64-elf-objcopy)
set(ROM_HEADER          ${LIBDRAGON_PREFIX}/lib/header)
set(MKDFS               ${LIBDRAGON_PREFIX}/bin/mkdfs)
set(N64SYM              ${LIBDRAGON_PREFIX}/bin/n64sym)

# Set additional link options for N64 linking
add_link_options(-Tn64.ld -Wl,--gc-sections -Wl,--wrap __do_global_ctors)

include(${CMAKE_CURRENT_LIST_DIR}/libdragon-n64.cmake)
