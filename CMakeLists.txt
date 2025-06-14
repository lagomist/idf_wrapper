set(src_list)
set(inc_list)
set(priv_inc_list)
set(dependencies)

include(${COMPONENT_DIR}/wrapper_driver.cmake)

idf_component_register(
    SRCS
    ${src_list}
    INCLUDE_DIRS
    ${inc_list}
    PRIV_INCLUDE_DIRS
    ${priv_inc_list}
    REQUIRES
    ${dependencies}
)

add_definitions(-Wno-missing-field-initializers)
