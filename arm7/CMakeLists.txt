add_definitions(-DARM7 -D_NDS)

set(ARCH "-mthumb -mthumb-interwork -mcpu=arm7tdmi -mtune=arm7tdmi")
set(CMAKE_C_FLAGS "${ARCH} -fomit-frame-pointer -fno-rtti -fno-exceptions -ffast-math")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_CXX_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "-specs=ds_arm7.specs -mthumb -mthumb-interwork  -Wl,-Map,vanilla.map")

set(ARM7_SRC
    main.cpp
)

# Link with those libraries
link_libraries("-lc")    # C library
link_libraries("-lnds7")

add_executable(arm7.elf ${ARM7_SRC})
