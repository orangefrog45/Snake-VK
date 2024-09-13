vec3 WorldPosFromDepth(float depth, vec2 normalized_tex_coords) {
	vec4 clipSpacePosition = vec4(normalized_tex_coords * 2.0 - 1.0, depth, 1.0);
	vec4 worldSpacePosition = inverse(common_ubo.view) * inverse(common_ubo.proj) * clipSpacePosition;
	// Perspective division
	worldSpacePosition.xyz /= max(worldSpacePosition.w, 1e-6);
	return worldSpacePosition.xyz;
};

vec3 CalcRayDir(ivec2 launch_id, ivec2 launch_size, vec3 ro) {
    vec2 pixel_center = vec2(launch_id.xy) + vec2(0.5);
    vec2 uv = pixel_center / vec2(launch_size.xy);
    return normalize(WorldPosFromDepth(1.0, uv) - ro);
};