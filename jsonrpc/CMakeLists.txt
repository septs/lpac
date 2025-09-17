aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} DIR_LPAC_SRCS)

add_executable(lpac-rpc ${DIR_LPAC_SRCS})

target_link_libraries(lpac-rpc euicc)
target_compile_options(lpac-rpc PRIVATE -Wall -Wextra -Wvla)
