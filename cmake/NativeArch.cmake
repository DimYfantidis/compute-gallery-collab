# Tune for the CPU doing the build, using whichever spelling the compiler accepts.
# Turn off (-DCG_NATIVE_ARCH=OFF) when the binaries must run on other machines,
# e.g. when baking them into a container image that is pushed to a registry.

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

option(CG_NATIVE_ARCH "Optimise for the build machine's CPU (-march=native)" ON)

set(_cg_native_flag "")
if (CG_NATIVE_ARCH AND NOT MSVC)
    foreach (flag IN ITEMS -march=native -mcpu=native)
        string(MAKE_C_IDENTIFIER "CG_HAS${flag}" var)
        check_cxx_compiler_flag(${flag} ${var}_CXX)
        check_c_compiler_flag(${flag} ${var}_C)
        if (${var}_CXX AND ${var}_C)
            set(_cg_native_flag ${flag})
            break()
        endif()
    endforeach()
    if (_cg_native_flag)
        message(STATUS "Native CPU tuning: ${_cg_native_flag}")
    else()
        message(STATUS "Native CPU tuning: unsupported by this compiler, skipped")
    endif()
endif()

function(cg_enable_native_arch target)
    if (_cg_native_flag)
        target_compile_options(${target} PRIVATE ${_cg_native_flag})
    endif()
endfunction()
