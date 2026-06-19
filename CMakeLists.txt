cmake_minimum_required(VERSION 3.12)

project(
    hip_cpu_rt
        VERSION 0.1.4142
        DESCRIPTION "The HIP CPU Runtime"
        HOMEPAGE_URL "https://github.com/ROCm-Developer-Tools/HIP-CPU"
        LANGUAGES CXX)

add_library(${PROJECT_NAME} INTERFACE)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(is_top_level TRUE)
else ()
    set(is_top_level FALSE)
endif ()

include(GNUInstallDirs)

target_include_directories(
    ${PROJECT_NAME} SYSTEM INTERFACE
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_17)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    message(FATAL_ERROR "GCC ${CMAKE_CXX_COMPILER_VERSION} is too old. GCC 9 or later is required.")
endif()

include(CheckCXXSourceCompiles)
check_cxx_source_compiles(
    "__attribute__((flatten, simd)) void f() {} int main() { return 0; }"
    HIP_CPU_HAS_FLATTEN_SIMD)
if (HIP_CPU_HAS_FLATTEN_SIMD)
    target_compile_definitions(${PROJECT_NAME} INTERFACE __HIP_CPU_HAS_FLATTEN_SIMD__)
endif()

if (NOT ${CMAKE_CXX_COMPILER_ID} STREQUAL MSVC)
    find_package(Threads REQUIRED)

    target_compile_options(${PROJECT_NAME} INTERFACE -fopenmp-simd)
    target_link_libraries(
        ${PROJECT_NAME} INTERFACE ${CMAKE_DL_LIBS} Threads::Threads)

    include(CheckCXXSourceCompiles)
    set(_hip_cpu_tbb_check_src
        "#include <algorithm>\n#include <execution>\n#include <vector>\nint main() {\n  std::vector<int> v{1};\n  std::for_each(std::execution::par, v.begin(), v.end(), [](int&){});\n  return 0;\n}")
    set(CMAKE_REQUIRED_FLAGS "-std=c++17")
    set(CMAKE_REQUIRED_LIBRARIES "")
    check_cxx_source_compiles("${_hip_cpu_tbb_check_src}" _HIP_CPU_PAR_WORKS)
    if (NOT _HIP_CPU_PAR_WORKS)
        find_package(TBB REQUIRED)
        target_link_libraries(
            ${PROJECT_NAME} INTERFACE TBB::tbb)

        try_compile(_HIP_CPU_TBB_COMPAT_OK
            "${CMAKE_BINARY_DIR}/CMakeTmp"
            SOURCES "${PROJECT_SOURCE_DIR}/tools/cmake/tbb_compat_check.cpp"
            CMAKE_FLAGS "-DCMAKE_CXX_STANDARD=17"
            LINK_LIBRARIES TBB::tbb Threads::Threads ${CMAKE_DL_LIBS}
            OUTPUT_VARIABLE _tbb_compat_out)
        if (NOT _HIP_CPU_TBB_COMPAT_OK)
            message(FATAL_ERROR
                "libstdc++ PSTL headers are incompatible with the installed TBB.\n"
                "  This occurs when GCC < 11 is paired with oneTBB (libtbb12+).\n"
                "  Solutions:\n"
                "  1. Upgrade to GCC 11 or later, OR\n"
                "  2. Install the TBB backward-compatibility package\n"
                "     (e.g. libtbb2-dev / tbb::tbb_compat / libtbb-dev on older distros)")
        endif()
    endif()
else ()
    target_compile_options(${PROJECT_NAME} INTERFACE /openmp:experimental)
endif ()

option(
    hip_cpu_rt_INSTALL_LIBRARY
        "Enable installing the HIP CPU Runtime into the default location."
        ${is_top_level})

if (hip_cpu_rt_INSTALL_LIBRARY)
    install(
        TARGETS ${PROJECT_NAME}
        EXPORT ${PROJECT_NAME}_Targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    include(CMakePackageConfigHelpers)

    write_basic_package_version_file(
        "${PROJECT_NAME}ConfigVersion.cmake"
            VERSION ${PROJECT_VERSION}
            COMPATIBILITY SameMajorVersion)
    configure_package_config_file(
        "${PROJECT_SOURCE_DIR}/tools/cmake/${PROJECT_NAME}Config.cmake.in"
        "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake)

    install(
        EXPORT ${PROJECT_NAME}_Targets
        FILE ${PROJECT_NAME}Targets.cmake
        NAMESPACE ${PROJECT_NAME}::
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake)
    install(
        FILES
            "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
            "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake)
    install(DIRECTORY ${PROJECT_SOURCE_DIR}/external/half DESTINATION external)
    install(DIRECTORY ${PROJECT_SOURCE_DIR}/external/libco DESTINATION external)
    install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/hip DESTINATION include)
    install(DIRECTORY ${PROJECT_SOURCE_DIR}/src/include DESTINATION src)
endif ()

option(
    hip_cpu_rt_BUILD_EXAMPLES
        "Build examples which use the HIP CPU Runtime."
        ${is_top_level})
option(
    hip_cpu_rt_BUILD_TESTING
        "Build the HIP CPU Runtime legacy, performance and unit tests."
        ${is_top_level})

if (hip_cpu_rt_BUILD_EXAMPLES)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/examples)
endif ()

if (hip_cpu_rt_BUILD_TESTING)
    include(CTest)
    enable_testing()
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests)
endif ()

set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
include(CPack)
