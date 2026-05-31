
cbuffer ParticleParams : register(b0)
{
    float dt;
    float totalTime;
    float2 pad1;
    float3 emitterPos;
    float pad2;
    float3 emitDir;
    float pad3;
};

struct Particle
{
    float3 position;
    float life;
    float3 velocity;
    float size;
    float4 color;
    float4 pad1;
    float4 pad2;
    float4 pad3;
    float4 pad4;
};

RWStructuredBuffer<Particle> particles : register(u1);

float rand(float2 co)
{
    return frac(sin(dot(co, float2(12.9898, 78.233))) * 43758.5453);
}

[numthreads(64, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint idx = id.x;
    if (idx >= 10000)
        return; //  Vulkan protects against out-of-bounds access, but it's good to be safe
    
    Particle p = particles[idx];
    p.life -= dt;
    
    if (p.life <= 0.0)
    {
        // create a new particle
        p.life = 2.0 + rand(float2(idx, totalTime)) * 2.0; // Lives 2 to 4 seconds
        p.pad1.x = p.life; // Store the maximum lifetime to know the percentage!
        
        // Emission from a circular area (so the fire is not from a single point)
        float angle = rand(float2(idx, totalTime)) * 6.2831;
        float radius = rand(float2(idx, totalTime + 1.0)) * 2.0;
        p.position = emitterPos + float3(cos(angle) * radius, 0, sin(angle) * radius);
        
        // Velocity and random direction
        float rx = rand(float2(idx, totalTime + 1.0)) * 2.0 - 1.0;
        float ry = rand(float2(idx, totalTime + 2.0)) * 0.5 + 0.5; // Fire rises
        float rz = rand(float2(idx, totalTime + 3.0)) * 2.0 - 1.0;
        
        p.velocity = emitDir * 2.0 + float3(rx, ry * 5.0, rz);
        p.size = 1.0 + rand(float2(idx, totalTime + 4.0)) * 1.5;
        
        // color - start bright yellow/orange
        p.color = float4(1.0, 0.9, 0.2, 1.0);
    }
    else
    {
        // --- PHYSICS AND AGING ---
        float lifePct = p.life / p.pad1.x; // Goes from 1.0 (birth) to 0.0 (death)
        
        // Movement
        p.velocity.y += 1.5 * dt; // Slight buoyancy            
        p.position += p.velocity * dt;
        
        // Size - smoke expands as it ages
        p.size += dt * 2.0;
        
        // COLOR - color transition based on age
        if (lifePct > 0.7)
        {
            // Fire: From yellow to orange/red
            p.color = lerp(float4(1.0, 0.3, 0.0, 1.0), float4(1.0, 0.9, 0.2, 1.0), (lifePct - 0.7) / 0.3);
        }
        else
        {
            // Smoke: From red to dark gray
            p.color = lerp(float4(0.2, 0.2, 0.2, 0.0), float4(1.0, 0.3, 0.0, 1.0), lifePct / 0.7);
        }
    }
    
    particles[idx] = p;
}