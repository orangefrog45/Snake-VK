#define DT (1.0 / 120.0)

struct Particle {
    vec4 position_radius;
    vec4 velocity; // w component is sleep state (0 = asleep, 1 = awake)
};

struct CellKeyEntry {
    uint key;
    uint ptcl_idx;
};

uint GetCellKey(ivec3 cell, uint num_particles) {
    return ((cell.x * 73856093) ^ (cell.y * 10357) ^ (cell.z * 83492791)) % num_particles;
}

bool IsParticleAsleep(in Particle ptcl) {
    return ptcl.velocity.w < 0.5;
}

void SleepParticle(in Particle ptcl) {
    ptcl.velocity.w = 0.0;
}

void WakeParticle(in Particle ptcl) {
    ptcl.velocity.w = 1.0;
}

ivec3 GetParticleCellPos(in Particle ptcl) {
    return ivec3(ptcl.position_radius.xyz*5);
}
