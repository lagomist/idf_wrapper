set(inc_list "inc")
set(src_list)
set(def_list)

include(${COMPONENT_DIR}/driver.cmake)

idf_component_register(
SRCS
${src_list}
INCLUDE_DIRS
${inc_list}
REQUIRES
${dependencies}
)

add_compile_definitions(${def_list})

set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS ON
)
