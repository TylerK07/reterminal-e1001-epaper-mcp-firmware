idf_component_register(
    SRCS
        "src/asset_service.c"
        "src/config_service.c"
        "src/network_service.c"
        "src/provisioning_service.c"
        "src/render_service.c"
        "src/status_service.c"
    INCLUDE_DIRS "include"
    REQUIRES core platform
)
