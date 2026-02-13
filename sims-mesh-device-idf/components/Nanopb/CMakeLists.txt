idf_component_register(
    SRCS "pb_encode.c" "pb_decode.c" "pb_common.c"
    INCLUDE_DIRS "."
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC
    PB_ENABLE_MALLOC=1
    PB_FIELD_32BIT=1
)
