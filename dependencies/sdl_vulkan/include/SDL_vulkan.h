#ifndef _SDL_vulkan_h
#define _SDL_vulkan_h

#include <SDL2/SDL_stdinc.h>
#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C"
{
#endif

// forward declarations
typedef struct SDL_Window SDL_Window;


SDL_bool SDL_GetVulkanInstanceExtensions(unsigned* count, const char** names);
SDL_bool SDL_CreateVulkanSurface(SDL_Window* window, VkInstance instance, VkSurfaceKHR* surface);

#ifdef __cplusplus
}
#endif

#endif /* _SDL_vulkan_h */
