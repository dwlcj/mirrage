#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
	vec3 view_ray;
	flat vec3 corner_view_rays[4];
} vertex_out;

layout(location = 0) out vec4 out_color;

layout(set=1, binding = 0) uniform sampler2D color_sampler;
layout(set=1, binding = 1) uniform sampler2D depth_sampler;
layout(set=1, binding = 2) uniform sampler2D mat_data_sampler;
layout(set=1, binding = 3) uniform sampler2D result_sampler;
layout(set=1, binding = 5) uniform sampler2D albedo_sampler;

layout(push_constant) uniform Push_constants {
	mat4 projection;
	vec4 arguments;
} pcs;


#include "global_uniforms.glsl"
#include "normal_encoding.glsl"
#include "random.glsl"
#include "brdf.glsl"
#include "upsample.glsl"
#include "raycast.glsl"

// losely based on https://www.gamedev.net/topic/658702-help-with-gpu-pro-5-hi-z-screen-space-reflections/?view=findpost&p=5173175
float roughness_to_spec_lobe_angle(float roughness) {
	float power = mix(1024.0*8.0, 8.0, roughness);
	return cos(pow(0.244, 1.0/(power + 1.0)));
}

float isosceles_triangle_opposite(float adjacentLength, float coneTheta) {
	return 2.0f * tan(coneTheta) * adjacentLength;
}

float isosceles_triangle_inradius(float a, float h) {
	float a2 = a * a;
	float fh2 = 4.0f * h * h;
	return (a * (sqrt(a2 + fh2) - a)) / (4.0f * max(h, 0.00001f));
}

void main() {
	float startLod = pcs.arguments.x;
	vec2 textureSize = textureSize(depth_sampler, int(startLod + 0.5));

	out_color = vec4(0,0,0,0);

	float depth  = textureLod(depth_sampler, vertex_out.tex_coords, startLod).r;
	vec3 P = depth * vertex_out.view_ray;

	vec4 mat_data = textureLod(mat_data_sampler, vertex_out.tex_coords, startLod);
	vec3 N = decode_normal(mat_data.rg);

	vec3 V = -normalize(P);

	vec3 dir = -reflect(V, N);

	vec2 raycast_hit_uv;
	vec3 raycast_hit_point;
	if(traceScreenSpaceRay1(P+dir*0.25, dir, pcs.projection, depth_sampler,
							textureSize, 1.0, global_uniforms.proj_planes.x,
							max(4, 4), 0.1, 512, 128.0, int(startLod + 0.5),
							raycast_hit_uv, raycast_hit_point)) {

		float roughness = mat_data.b;

		vec3 L = raycast_hit_point - P;
		float L_length = length(L);
		L /= L_length;

		vec3 H = normalize(V+L);

		float lobe_angle = roughness_to_spec_lobe_angle(roughness);

		float oppositeLength = isosceles_triangle_opposite(L_length, lobe_angle);

		float hit_radius = isosceles_triangle_inradius(L_length, oppositeLength);

		float lod = log2(hit_radius * max(textureSize.x,textureSize.y));
		lod = clamp(lod, startLod, pcs.arguments.y);
		vec3 radiance = textureLod(color_sampler, raycast_hit_uv/textureSize, lod).rgb;

		out_color.rgb = radiance;
		out_color.a = mix(1.0, 0.0, clamp((L_length-80) / 20.0, 0.0, 1.0));
	}
}
