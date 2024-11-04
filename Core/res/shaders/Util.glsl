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

/*
    static.frag
    by Spatial
    05 July 2013
*/


// A single iteration of Bob Jenkins' One-At-A-Time hashing algorithm.
uint hash( uint x ) {
    x += ( x << 10u );
    x ^= ( x >>  6u );
    x += ( x <<  3u );
    x ^= ( x >> 11u );
    x += ( x << 15u );
    return x;
}



// Compound versions of the hashing algorithm I whipped together.
uint hash( uvec2 v ) { return hash( v.x ^ hash(v.y)                         ); }
uint hash( uvec3 v ) { return hash( v.x ^ hash(v.y) ^ hash(v.z)             ); }
uint hash( uvec4 v ) { return hash( v.x ^ hash(v.y) ^ hash(v.z) ^ hash(v.w) ); }



// Construct a float with half-open range [0:1] using low 23 bits.
// All zeroes yields 0.0, all ones yields the next smallest representable value below 1.0.
float floatConstruct( uint m ) {
    const uint ieeeMantissa = 0x007FFFFFu; // binary32 mantissa bitmask
    const uint ieeeOne      = 0x3F800000u; // 1.0 in IEEE binary32

    m &= ieeeMantissa;                     // Keep only mantissa bits (fractional part)
    m |= ieeeOne;                          // Add fractional part to 1.0

    float  f = uintBitsToFloat( m );       // Range [1:2]
    return f - 1.0;                        // Range [0:1]
}



// Pseudo-random value in range [0:1].
float random( float x ) { return floatConstruct(hash(floatBitsToUint(x))); }

vec3 random(vec3 seed) {
  return vec3(random(seed.x), random(seed.y), random(seed.z));
}

vec3 RandomVectorOnHemisphere(vec3 n, vec3 seed) {
  vec3 rnd = normalize(random(seed));
  if (dot(rnd, n) < 0.0)
    rnd *= -1.0;
  
  return rnd;
}