set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR armv5te)

#set(DEVKITARM $ENV{DEVKITARM})
#set(DEVKITPRO $ENV{DEVKITPRO})

set(WONDERFUL_TOOLCHAIN $ENV{WONDERFUL_TOOLCHAIN})
set(ARM_EABI_GCC        $ENV{WONDERFUL_TOOLCHAIN}/toolchain/gcc-arm-none-eabi)
set(BLOCKSDS            $ENV{BLOCKSDS})

set(CMAKE_C_COMPILER "${ARM_EABI_GCC}/bin/arm-none-eabi-gcc")
set(CMAKE_CXX_COMPILER "${ARM_EABI_GCC}/bin/arm-none-eabi-g++")
set(CMAKE_ASM_COMPILER "${ARM_EABI_GCC}/bin/arm-none-eabi-gcc")
set(CMAKE_AR "${ARM_EABI_GCC}/bin/arm-none-eabi-gcc-ar")
set(CMAKE_RANLIB "${ARM_EABI_GCC}/bin/arm-none-eabi-gcc-ranlib")
set(NDSTOOL "${BLOCKSDS}/tools/ndstool/ndstool")
set(ARM7SPEC "${BLOCKSDS}/sys/crts/ds_arm7.specs")
set(ARM9SPEC "${BLOCKSDS}/sys/crts/ds_arm9.specs")

#set(CMAKE_FIND_ROOT_PATH ${DEVKITPRO})
set(CMAKE_FIND_ROOT_PATH ${WONDERFUL_TOOLCHAIN})

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(CMAKE_FIND_LIBRARY_PREFIXES "lib")
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a" ".la")

set(CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES ${ARM_EABI_GCC}/arm-none-eabi/include ${BLOCKSDS}/libs/libnds/include ${BLOCKSDS}/libs/libgba/include)

set(NDS TRUE)

SET(BUILD_SHARED_LIBS OFF CACHE INTERNAL "Shared libs not available" )

add_definitions(-D_NDS)

# Define paths to include libraries
include_directories("${BLOCKSDS}/libs/libnds/include")

# Define paths to libraries. Use link_libraries, as link_directories don't
# seem to work.
link_libraries("-L${ARM_EABI_GCC}/lib")
link_libraries("-L${ARM_EABI_GCC}/arm-none-eabi/lib")
link_libraries("-L${BLOCKSDS}/libs/libnds/lib")
link_libraries("-L${BLOCKSDS}/libs/libgba/lib")
