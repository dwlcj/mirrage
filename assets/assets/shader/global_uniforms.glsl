#ifndef GLOBAL_UNIFORMS_INCLUDED
#define GLOBAL_UNIFORMS_INCLUDED

layout(binding = 0) uniform Global_uniforms {
	mat4 view_proj_mat;
	mat4 view_mat;
	mat4 proj_mat;
	mat4 inv_view_mat;
	mat4 inv_proj_mat;
	vec4 eye_pos;
	vec4 proj_planes; //< near, far, fov horizontal, fov vertical
	vec4 time; //< time, sin(time), delta_time
} global_uniforms;

#endif
