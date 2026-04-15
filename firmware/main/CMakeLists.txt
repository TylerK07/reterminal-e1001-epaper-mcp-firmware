idf_component_register(
    SRCS
        "app_main.c"
        "../components/core/src/app_controller.c"
    INCLUDE_DIRS "../components/core/include"
    REQUIRES core platform services mcp
)
