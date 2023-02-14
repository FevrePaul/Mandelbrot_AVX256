# Find google/benchmark
# --------------------
# The following variables are defined :
#
#   - BENCHMARK_FOUND : true if specmicp is found
#   - BENCHMARK_INCLUDE_DIR : the include dir
#   - BENCHMARK_LIBRARIES : the libraries
#
# copyright (c) 2015 Fabien georget <fabieng@princeton.edu>
# Redistribution and use is allowed according to the terms of the 3-clause BSD license.


# Find the libraries
# ------------------
if (NOT BENCHMARK_LIBRARIES)

find_library(BENCHMARK_LIBRARY
    NAMES benchmark
    HINTS ${BENCHMARK_LIBS_DIR}
)
endif()

set(BENCHMARK_LIBRARIES
    ${BENCHMARK_LIBRARY} pthread
)

# Find the includes
# -----------------
if (NOT BENCHMARK_INCLUDE_DIR)
    find_path(BENCHMARK_INCLUDE_DIR
    NAMES benchmark/benchmark.h benchmark/benchmark_api.h
    PATHS ${CMAKE_INSTALL_PREFIX}/include
    )
endif()



# Check that everything is ok
# ---------------------------
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(benchmark DEFAULT_MSG BENCHMARK_INCLUDE_DIR BENCHMARK_LIBRARIES)

mark_as_advanced(
    BENCHMARK_INCLUDE_DIR
    BENCHMARK_LIBRARY
    BENCHMARK_LIBRARIES
)
