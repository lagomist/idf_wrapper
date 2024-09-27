set(inc_list
    ${COMPONENT_DIR}/include
    ${COMPONENT_DIR}/misc
)

set(src_list)

include(${COMPONENT_DIR}/wrapper_driver.cmake)

idf_component_register(
    SRCS
    ${src_list}
    INCLUDE_DIRS
    ${inc_list}
    REQUIRES
    ${dependencies}
)

# set_target_properties(${COMPONENT_LIB} PROPERTIES
#     CXX_STANDARD 20
#     CXX_STANDARD_REQUIRED ON
#     CXX_EXTENSIONS ON
# )
