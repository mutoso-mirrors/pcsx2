# zzogl Plugin

# Clear default flags
set(CMAKE_C_FLAGS "")
set(CMAKE_CXX_FLAGS "")
set(CMAKE_C_FLAGS_DEBUG "")
set(CMAKE_CXX_FLAGS_DEBUG "")
set(CMAKE_C_FLAGS_DEVEL "")
set(CMAKE_CXX_FLAGS_DEVEL "")
set(CMAKE_C_FLAGS_RELEASE "")
set(CMAKE_CXX_FLAGS_RELEASE "")

# Also clear shared library flag to remove fpic option
# that broke some asm code. The better solution will be to remove
# the asm or fix it.
set(CMAKE_SHARED_LIBRARY_CXX_FLAGS "")
set(CMAKE_SHARED_LIBRARY_C_FLAGS "")

# plugin name
set(zzoglName zzogl)

# Debug - Build
if(CMAKE_BUILD_TYPE STREQUAL Debug)
    # add defines
    add_definitions(-Wall -m32 -g)
endif(CMAKE_BUILD_TYPE STREQUAL Debug)

# Devel - Build
if(CMAKE_BUILD_TYPE STREQUAL Devel)
    # add defines
    add_definitions(-Wall -m32 -O2)
endif(CMAKE_BUILD_TYPE STREQUAL Devel)

# Release - Build
if(CMAKE_BUILD_TYPE STREQUAL Release)
    # add defines
    add_definitions(-Wall -m32 -O2 -s)
endif(CMAKE_BUILD_TYPE STREQUAL Release)

# zzogl sources
set(zzoglSources
    GifTransfer.cpp
    GLWin32.cpp
    GLWinX11.cpp
    GSmain.cpp
    Mem.cpp
    memcpy_amd.cpp
    Mem_Swizzle.cpp
    Mem_Tables.cpp
    Profile.cpp
    rasterfont.cpp
    Regs.cpp
    targets.cpp
    x86.cpp
    zerogs.cpp
    zpipe.cpp
    ZZoglCreate.cpp
    ZZoglCRTC.cpp
    ZZoglFlush.cpp
    ZZoglSave.cpp
    ZZoglShaders.cpp
    ZZoglShoots.cpp
    ZZoglVB.cpp)

# zzogl headers
set(zzoglHeaders
    common.h
    GifTransfer.h
    glprocs.h
    GS.h
    Mem.h
    Mem_Swizzle.h
    Mem_Transmit.h
    Profile.h
    rasterfont.h
    Regs.h
    targets.h
    Util.h
    x86.h
    zerogs.h
    zerogsmath.h
    zpipe.h)

# zzogl S sources
set(zzoglSSources
    x86-32.S)

# zzogl shader sources
set(zzoglShaderSources
    ctx0/ps2hw_ctx.fx
    ctx1/ps2hw_ctx.fx)

# zzogl Linux sources
set(zzoglLinuxSources
    Linux/Conf.cpp
    Linux/Linux.cpp)

# zzogl Linux headers
set(zzoglLinuxHeaders
    Linux/Linux.h)

# change language of .S-files to c++
set_source_files_properties(${zzoglSSources} PROPERTIES LANGUAGE CXX)

# add additional include directories
include_directories(.
    Linux)

# add library
add_library(${zzoglName} SHARED
    ${zzoglSources}
    ${zzoglHeaders}
    ${zzoglSSources}
    ${zzoglShaderSources}
    ${zzoglLinuxSources}
    ${zzoglLinuxHeaders})

# set output directory
set_target_properties(${zzoglName} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/plugins)

# WARNING can conflict with zerogs plugin
# copy ps2hw.dat to plugins folder
add_custom_command(TARGET ${zzoglName} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/plugins/zzogl-pg/opengl/ps2hw.dat ${PROJECT_SOURCE_DIR}/bin/plugins)

# link target with project internal libraries
target_link_libraries(${zzoglName} Utilities)

# link target with Cg
target_link_libraries(${zzoglName} ${CG_LIBRARIES})

# link target with glew
target_link_libraries(${zzoglName} ${GLEW_LIBRARY})

# link target with opengl
target_link_libraries(${zzoglName} ${OPENGL_LIBRARIES})

# link target with X11
target_link_libraries(${zzoglName} ${X11_LIBRARIES})

# link target with X11 videomod
target_link_libraries(${zzoglName} ${X11_Xxf86vm_LIB})

# Set link flag
target_link_libraries(${zzoglName} -m32)