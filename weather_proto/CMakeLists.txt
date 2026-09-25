set(WEATHER_PROTO_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(WEATHER_PROTO_FILE "${WEATHER_PROTO_DIR}/weather.proto")
set(WEATHER_PROTO_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(WEATHER_PROTO_GENERATED_C "${WEATHER_PROTO_GENERATED_DIR}/weather.pb.c")
set(WEATHER_PROTO_GENERATED_H "${WEATHER_PROTO_GENERATED_DIR}/weather.pb.h")

if(NOT CMAKE_SCRIPT_MODE_FILE)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)

    # nanopb's vendored source dir, published as a custom component
    # property (see shared_components/nanopb/CMakeLists.txt), since
    # COMPONENT_DIR would only return that wrapper's own folder.
    idf_component_get_property(NANOPB_SOURCE_DIR nanopb NANOPB_SOURCE_DIR)

    file(MAKE_DIRECTORY "${WEATHER_PROTO_GENERATED_DIR}")

    # Run at configure time (not as a build-time custom command) so the
    # real generated files exist before Ninja builds ANY component. A
    # build-time command only guarantees this component's own sources
    # wait for it; other components like "main" that just #include the
    # generated header have no such ordering guarantee and can race
    # ahead on a clean build.
    execute_process(
        COMMAND "${Python3_EXECUTABLE}"
            "${NANOPB_SOURCE_DIR}/generator/nanopb_generator.py"
            -I "${WEATHER_PROTO_DIR}"
            --output-dir "${WEATHER_PROTO_GENERATED_DIR}"
            "${WEATHER_PROTO_FILE}"
        WORKING_DIRECTORY "${WEATHER_PROTO_DIR}"
        RESULT_VARIABLE WEATHER_PROTO_GENERATE_RESULT
    )
    if(NOT WEATHER_PROTO_GENERATE_RESULT EQUAL 0)
        message(FATAL_ERROR "nanopb generation for weather.proto failed")
    endif()
endif()

idf_component_register(
    SRCS "${WEATHER_PROTO_GENERATED_C}"
    INCLUDE_DIRS "${WEATHER_PROTO_GENERATED_DIR}"
    REQUIRES nanopb
)
