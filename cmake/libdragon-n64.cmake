set(CMAKE_SYSTEM_NAME N64)

set(CMAKE_C_COMPILER mips64-elf-gcc)
set(CMAKE_CXX_COMPILER mips64-elf-g++)

set(CMAKE_FIND_ROOT_PATH /usr/local/mips64-elf)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(CMAKE_FIND_LIBRARY_PREFIXES "lib" "")
set(CMAKE_FIND_LIBRARY_SUFFIXES ".lib" ".a")

set(CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES /usr/local/mips64-elf/include)
