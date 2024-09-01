struct DirectionalLight {
    vec4 colour;
    vec4 dir;
    mat4 light_transform;
};

struct Pointlight {
    vec4 colour;
    vec4 pos;

    float atten_exp;
    float atten_linear;
    float atten_constant;
    float padding;
};

struct Spotlight {
    vec4 colour;
    vec4 pos;
	vec4 dir;

    float atten_exp;
    float atten_linear;
    float atten_constant;

    float aperture;
};

#define PI 3.1415926

layout(set = 2, binding = 0) readonly buffer LightData {
    DirectionalLight dir_light;
    uint num_pointlights;
    uint num_spotlights;
	float pad0;
	float pad1;
    Pointlight pointlights[1024];
    Spotlight spotlights[1024];
} ssbo_light_data;

vec3 FresnelSchlick(float cos_theta, vec3 f0) {
	return f0 + (1.0 - f0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

float DistributionGGX(vec3 h, vec3 n, float roughness) {
	float a = roughness * roughness;
	float a2 = a * a;
	float n_dot_h = max(dot(n, h), 0.0);
	float n_dot_h2 = n_dot_h * n_dot_h;

	float num = a2;
	float denom = (n_dot_h2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;

	return num / denom;
}

float GeometrySchlickGGX(float n_dot_v, float roughness) {
	float r = (roughness + 1.0);
	float k = (r * r) / 8.0;

	return n_dot_v / (n_dot_v * (1.0 - k) + k);
}

float GeometrySmith(vec3 v, vec3 l, vec3 n, float roughness) {
	float n_dot_v = max(dot(n, v), 0.0);
	float n_dot_l = max(dot(n, l), 0.0);
	float ggx2 = GeometrySchlickGGX(n_dot_v, roughness);
	float ggx1 = GeometrySchlickGGX(n_dot_l, roughness);

	return ggx2 * ggx1;
}

vec3 FresnelSchlickRoughness(float cos_theta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

vec3 CalcDirectionalLight(vec3 v, vec3 f0, vec3 n, float roughness, float metallic, vec3 albedo) {
	vec3 l = ssbo_light_data.dir_light.dir.xyz;
	vec3 h = normalize(v + l);
	vec3 f = FresnelSchlick(max(dot(h, v), 0.0), f0);

	float ndf = DistributionGGX(h, n, roughness);
	float g = GeometrySmith(v, l, n, roughness);

	vec3 num = ndf * g * f;
	float denom = 4.0 * max(dot(n, v), 0.0) * max(dot(n, l), 0.0) + 0.0001;
	vec3 specular = num / denom;


	vec3 kd = vec3(1.0) - f;
	kd *= 1.0 - metallic;

	float n_dot_l = max(dot(n, l), 0.0);
	return (kd * albedo / PI + specular) * n_dot_l * ssbo_light_data.dir_light.colour.xyz;
}



vec3 CalcPointlight(in Pointlight light, vec3 v, vec3 f0, vec3 world_pos, vec3 n, float roughness, float metallic, vec3 albedo) {
	vec3 frag_to_light = light.pos.xyz - world_pos.xyz;

	float distance = length(frag_to_light);
	float attenuation = light.atten_constant +
		light.atten_linear * distance +
		light.atten_exp * pow(distance, 2);

	if (attenuation > 5000)
		return vec3(0);

	vec3 l = normalize(frag_to_light);
	vec3 h = normalize(v + l);

	vec3 f = FresnelSchlick(max(dot(h, v), 0.0), f0);
	float ndf = DistributionGGX(h, n, roughness);
	float g = GeometrySmith(v, l, n, roughness);

	float n_dot_l = max(dot(n, l), 0.0);

	vec3 num = ndf * g * f;
	float denom = 4.0 * max(dot(n, v), 0.0) * max(n_dot_l, 0.0) + 0.0001;
	vec3 specular = num / denom;


	vec3 kd = vec3(1.0) - f;
	kd *= 1.0 - metallic;



	return (kd * albedo / PI + specular) * n_dot_l * (light.colour.xyz / attenuation);
}


vec3 CalcSpotlight(Spotlight light, vec3 v, vec3 f0, vec3 world_pos, vec3 n, float roughness, float metallic, vec3 albedo) {
	vec3 frag_to_light = light.pos.xyz - world_pos;
	float spot_factor = dot(normalize(frag_to_light), -light.dir.xyz);

	vec3 color = vec3(0);

	if (spot_factor < 0.0001 || spot_factor < light.aperture)
		return color;

	float distance = length(frag_to_light);
	float attenuation = light.atten_constant +
		light.atten_linear * distance +
		light.atten_exp * pow(distance, 2);

	if (attenuation > 5000)
		return vec3(0);

	vec3 l = normalize(frag_to_light);
	vec3 h = normalize(v + l);

	vec3 f = FresnelSchlick(max(dot(h, v), 0.0), f0);
	float ndf = DistributionGGX(h, n, roughness);
	float g = GeometrySmith(v, l, n, roughness);

	float n_dot_l = max(dot(n, l), 0.0);

	vec3 num = ndf * g * f;
	float denom = 4.0 * max(dot(n, v), 0.0) * max(n_dot_l, 0.0) + 0.0001;
	vec3 specular = num / denom;


	vec3 kd = vec3(1.0) - f;
	kd *= 1.0 - metallic;


	float spotlight_intensity = (1.0 - (1.0 - spot_factor) / max((1.0 - light.aperture), 1e-5));
	return max((kd * albedo / PI + specular) * n_dot_l * (light.colour.xyz / attenuation) * spotlight_intensity, vec3(0.0, 0.0, 0.0));
}
