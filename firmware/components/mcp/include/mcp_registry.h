#pragma once
#include <stdint.h>
#include "enums.h"
#include "errors.h"

typedef struct {
    const char *name;
    mcp_auth_level_t auth_level;
    mcp_cost_class_t cost_class;
} mcp_tool_metadata_t;

typedef struct {
    const char *uri;
    mcp_auth_level_t auth_level;
} mcp_resource_metadata_t;

error_code_t mcp_registry_init(void);
const mcp_tool_metadata_t *mcp_registry_find_tool(const char *name);
const mcp_resource_metadata_t *mcp_registry_find_resource(const char *uri);
uint32_t mcp_registry_get_tool_count(void);
uint32_t mcp_registry_get_resource_count(void);
