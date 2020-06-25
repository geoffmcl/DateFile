######################################################################
# 20140819 - Hand crafed CMakeLists.txt for DateFile project - geoff #
# 20200513 - Consider adding it to a GitHub repo                     #
######################################################################
cmake_minimum_required (VERSION 3.6)

project(DateFile)

#------------------------------------------------------------------------
# Static Windows Runtime
#   Option to statically link to the Windows runtime. Maybe only 
#   applies to WIN32/MSVC.
#------------------------------------------------------------------------
if(WIN32)
  if (MSVC)
    option( USE_STATIC_RUNTIME "Set ON to change /MD(DLL) to /MT(static)" ON )
    if (USE_STATIC_RUNTIME)
        set(CompilerFlags
            CMAKE_CXX_FLAGS
            CMAKE_CXX_FLAGS_DEBUG
            CMAKE_CXX_FLAGS_RELEASE
            CMAKE_C_FLAGS
            CMAKE_C_FLAGS_DEBUG
            CMAKE_C_FLAGS_RELEASE
            )
        foreach(CompilerFlag ${CompilerFlags})
            string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
        endforeach()    
        message(STATUS "Using /MT STATIC runtime")
    else ()
        message(STATUS "Using /MD DYNAMIC runtime")
    endif ()
  endif ()
else()
    message(FATAL_ERROR "Has NOT been ported to other than Windows OS")
endif()

set(name DateFile)
set(dir src)
add_executable( ${name} 
    ${dir}/${name}.cpp
    ${dir}/${name}.h 
    )
set_target_properties( ${name} PROPERTIES DEBUG_POSTFIX d )
# install
# install (TARGETS ${name} DESTINATION bin)

# eof
