#

add_library(e2sm_runOran STATIC
    e2sm_runOran_impl.cpp
)

target_link_libraries(e2sm_runOran
    srslog
    srsran_support
)
