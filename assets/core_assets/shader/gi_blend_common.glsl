#ifndef GI_BLEND_COMMON_INCLUDED
#define GI_BLEND_COMMON_INCLUDED

#include "normal_encoding.glsl"
#include "upsample.glsl"
#include "brdf.glsl"
#include "color_conversion.glsl"


float luminance_norm(vec3 c) {
	vec3 f = vec3(0.299,0.587,0.114);
	return sqrt(c.r*c.r*f.r + c.g*c.g*f.g + c.b*c.b*f.b);
}

vec3 calculate_gi(vec2 uv, vec3 radiance, vec3 specular,
                  sampler2D albedo_sampler, sampler2D mat_sampler, sampler2D brdf_sampler) {
	const float PI = 3.14159265359;

//	radiance /= max(1, 1 - luminance_norm(radiance)/1);
//	specular /= max(1, 1 - luminance_norm(specular)/1);

	// load / calculate material properties and factors
	vec3 albedo = textureLod(albedo_sampler, uv, 0.0).rgb;

	vec4 mat_data = textureLod(mat_sampler, uv, 0.0);
	float roughness = mat_data.b;
	float metallic = mat_data.a;

	vec3 F0 = mix(vec3(0.04), albedo.rgb, metallic);
	albedo.rgb *= 1.0 - metallic;

	vec3 N = decode_normal(mat_data.rg);

	float depth  = textureLod(depth_sampler, uv, 0.0).r;
	vec3 P = position_from_ldepth(vertex_out.tex_coords, depth);
	vec3 V = -normalize(P);

	float NdotV = clamp(dot(N, V), 0.0, 1.0);

	vec2 brdf = texture(brdf_sampler, vec2(NdotV, roughness)).rg;

	vec3 diff = albedo * radiance*(1.0 - F0*brdf.x) / PI;
	vec3 spec = specular.rgb * (F0*brdf.x + brdf.y);

	return max(diff + spec, vec3(0,0,0))*1000.0;
}

vec3 calculate_gi(vec2 uv, vec2 gi_uv, int gi_lod, sampler2D diff_sampler, sampler2D spec_sampler,
                  sampler2D albedo_sampler, sampler2D mat_sampler, sampler2D brdf_sampler) {
    // load diff + spec GI
    vec3 radiance = textureLod(diff_sampler, gi_uv, 0).rgb;
	vec3 specular = textureLod(spec_sampler, gi_uv, 0).rgb;

    return calculate_gi(uv, radiance, specular, albedo_sampler, mat_sampler, brdf_sampler);
}

#endif
