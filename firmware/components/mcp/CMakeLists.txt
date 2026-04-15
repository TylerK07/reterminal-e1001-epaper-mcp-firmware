idf_component_register(
    SRCS
        "src/mcp_dispatch.c"
        "src/mcp_registry.c"
        "src/mcp_server.c"
    INCLUDE_DIRS "include"
    REQUIRES core services esp_http_server
)
