#pragma once

#define HOST_CODE (defined __APPLE__ || defined __linux__ || defined _WIN32)
#define SHADER_CODE (!HOST_CODE)

#define MAX_POINT_LIGHTS 10
#define MAX_DIRECTIONAL_LIGHTS 4
#define MAX_SPOT_LIGHTS 10