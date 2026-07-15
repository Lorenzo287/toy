set(TOY_BINDING_GENERATOR
    "${CMAKE_CURRENT_LIST_DIR}/../tools/generate-binding.js")
find_program(TOY_BINDING_NODE NAMES node node.exe)

function(toy_add_generated_binding TARGET MANIFEST OUTPUT_NAME)
    set(OPTIONS EXCLUDE_FROM_ALL)
    cmake_parse_arguments(TOY_BINDING "${OPTIONS}" "" "" ${ARGN})

    if(NOT TARGET toy::module_support)
        message(FATAL_ERROR
            "toy_add_generated_binding requires the toy::module_support target")
    endif()
    if(NOT TOY_BINDING_NODE)
        message(FATAL_ERROR
            "toy_add_generated_binding requires a Node.js executable")
    endif()

    get_filename_component(MANIFEST_ABSOLUTE "${MANIFEST}" ABSOLUTE
                           BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    set(GENERATED_SOURCE
        "${CMAKE_CURRENT_BINARY_DIR}/generated/${TARGET}.c")
    add_custom_command(
        OUTPUT "${GENERATED_SOURCE}"
        COMMAND "${TOY_BINDING_NODE}" "${TOY_BINDING_GENERATOR}"
                "${MANIFEST_ABSOLUTE}" "${GENERATED_SOURCE}"
        DEPENDS "${TOY_BINDING_GENERATOR}" "${MANIFEST_ABSOLUTE}"
        COMMENT "Generating Toy binding ${TARGET}"
        VERBATIM
    )

    if(TOY_BINDING_EXCLUDE_FROM_ALL)
        add_library(${TARGET} MODULE EXCLUDE_FROM_ALL "${GENERATED_SOURCE}")
    else()
        add_library(${TARGET} MODULE "${GENERATED_SOURCE}")
    endif()
    target_link_libraries(${TARGET} PRIVATE toy::module_support)
    set_target_properties(${TARGET} PROPERTIES
        PREFIX ""
        OUTPUT_NAME "${OUTPUT_NAME}"
        C_VISIBILITY_PRESET hidden)
endfunction()
