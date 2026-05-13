cmake_minimum_required(VERSION 3.20)

add_library(pbm_1064nm STATIC
    src/np_pbm1064_detect.c
    src/np_pbm1064_drive.c
    src/np_pbm1064_dose.c
    src/np_pbm1064_session.c
    src/np_pbm1064_t2_combined.c
)

target_include_directories(pbm_1064nm
    PUBLIC  include
    PRIVATE ${NEUROPULSE_HAL_INCLUDE}
)

target_link_libraries(pbm_1064nm
    PRIVATE neuropulse_hal
    PRIVATE zone_announce   # bone conduction callback for smart module announcement
)

target_compile_options(pbm_1064nm PRIVATE
    -Wall -Wextra -Werror
    -fno-exceptions
    -ffunction-sections -fdata-sections
    -Os
)
