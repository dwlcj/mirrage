#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "global_uniforms.glsl"
#include "normal_encoding.glsl"

layout(location = 0) in Vertex_data {
	vec3 world_pos;
	vec3 view_pos;
	vec3 normal;
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 depth_out;
layout(location = 1) out vec4 albedo_mat_id_out;
layout(location = 2) out vec4 mat_data_out;

layout(set=1, binding = 0) uniform sampler2D albedo_sampler;
layout(set=1, binding = 1) uniform sampler2D mat_data_sampler;

layout(push_constant) uniform Per_model_uniforms {
	mat4 model;
	vec4 light_color;
	vec4 options;
} model_uniforms;

const float PI = 3.14159265359;


vec3 decode_tangent_normal(vec2 tn);
vec3 tangent_space_to_world(vec3 N);

void main() {
	vec4 albedo = textureLod(albedo_sampler, vertex_out.tex_coords, 3);

//	if(albedo.a < 0.1)
//		discard;

	vec4 mat_data = texture(mat_data_sampler, vertex_out.tex_coords); // TODO: LOD bias?

	vec3  normal    = tangent_space_to_world(decode_tangent_normal(mat_data.rg));
	//normal.z = abs(normal.z); // TODO: remove?

	float roughness = mat_data.b;
	float metallic  = mat_data.a;

	roughness = mix(0.05, 0.99, roughness*roughness);

	depth_out         = vec4(-vertex_out.view_pos.z / global_uniforms.proj_planes.y, 0,0,1);
	albedo_mat_id_out = vec4(albedo.rgb, 0.0);
	mat_data_out      = vec4(encode_normal(normal), roughness, metallic);

	float disect = model_uniforms.options.x;
	if(disect>0 && vertex_out.world_pos.z>=disect)
		discard;
}

vec3 decode_tangent_normal(vec2 tn) {
	if(dot(tn,tn)<0.00001)
		return vec3(0,0,1);

	vec3 N = vec3(tn*2-1, 0);
	N.z = sqrt(1 - dot(N.xy, N.xy));
	return N;
}

vec3 tangent_space_to_world(vec3 N) {
	vec3 VN = normalize(vertex_out.normal);

// calculate tangent (assimp generated tangent contain weird artifacts)
	vec3 p_dx = dFdx(vertex_out.view_pos);
	vec3 p_dy = dFdy(vertex_out.view_pos);

	vec2 tc_dx = dFdx(vertex_out.tex_coords);
	vec2 tc_dy = dFdy(vertex_out.tex_coords);


	  // TODO: check alternativ
	vec3 p_dy_N = cross(p_dy, VN);
	vec3 p_dx_N = cross(VN, p_dx);

	vec3 T = p_dy_N * tc_dx.x + p_dx_N * tc_dy.x;
	vec3 B = p_dy_N * tc_dx.y + p_dx_N * tc_dy.y;

	float inv_max = inversesqrt(max(dot(T,T), dot(B,B)));
	mat3 TBN = mat3(T*inv_max, B*inv_max, VN);
	return normalize(TBN * N);


/*
	vec3 VT = normalize( tc_dy.y * p_dx - tc_dx.y * p_dy );

	VT = normalize(VT - dot(VT, VN) * VN);
	vec3 VB = cross(VT, VN);
	mat3 TBN = mat3(VT, VB, VN);

	return normalize(TBN * N);*/

}

