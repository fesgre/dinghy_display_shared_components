get_filename_component(NANOPB_SOURCE_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../vendor/nanopb" ABSOLUTE)

if(NOT EXISTS "${NANOPB_SOURCE_DIR}/pb.h")
    message(FATAL_ERROR
        "nanopb submodule not found at ${NANOPB_SOURCE_DIR}. "
        "Run 'git submodule update --init --recursive' in shared_components.")
endif()

idf_component_register(
    SRCS "${NANOPB_SOURCE_DIR}/pb_common.c"
         "${NANOPB_SOURCE_DIR}/pb_encode.c"
         "${NANOPB_SOURCE_DIR}/pb_decode.c"
    INCLUDE_DIRS "${NANOPB_SOURCE_DIR}"
)

# COMPONENT_DIR only exposes this wrapper's own folder, not the vendored
# checkout, so publish it explicitly for weather_proto's generator step.
idf_component_set_property(${COMPONENT_NAME} NANOPB_SOURCE_DIR "${NANOPB_SOURCE_DIR}")
