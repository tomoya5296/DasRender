/* 
 * Copyright (c) 2018 NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <optixu/optixu_math_namespace.h>
#include "optixPathTracer.h"
#include "random.h"

using namespace optix;

struct PerRayData_pathtrace
{
    float3 result;
    float3 radiance;
    float3 attenuation;
    float3 origin;
    float3 direction;
    float3 shading_normal;
    float3 texture;
    float depth;
    unsigned int seed;
    int bounce;
    int countEmitted;
    int done;
    bool inShadow;
};

struct PerRayData_pathtrace_shadow
{
    bool inShadow;
};

// Scene wide variables
rtDeclareVariable(float,         scene_epsilon, , );
rtDeclareVariable(rtObject,      top_object, , );
rtDeclareVariable(uint2,         launch_index, rtLaunchIndex, );

rtDeclareVariable(PerRayData_pathtrace, current_prd, rtPayload, );



//-----------------------------------------------------------------------------
//
//  Camera program -- main ray tracing loop
//
//-----------------------------------------------------------------------------

rtDeclareVariable(float3,        eye, , );
rtDeclareVariable(float3,        U, , );
rtDeclareVariable(float3,        V, , ); 
rtDeclareVariable(float3,        W, , );
rtDeclareVariable(float3,        bad_color, , );
rtDeclareVariable(unsigned int,  frame_number, , );
rtDeclareVariable(unsigned int,  num_samples, , );
rtDeclareVariable(unsigned int,  rr_begin_bounce, , );
rtDeclareVariable(unsigned int,  rr_max_bounce, , );
rtDeclareVariable(unsigned int,  pathtrace_ray_type, , );
rtDeclareVariable(unsigned int,  pathtrace_shadow_ray_type, , );

rtBuffer<float4, 2>              output_buffer;
rtBuffer<float4, 2>              depth_buffer;
rtBuffer<float4, 2>              texture_buffer;
rtBuffer<float4, 2>              normal_buffer;
rtBuffer<float4, 2>              shadow_buffer;
rtBuffer<ParallelogramLight>     lights;


RT_PROGRAM void pathtrace_camera()
{
    size_t2 screen = output_buffer.size();

    float2 inv_screen = 1.0f/make_float2(screen) * 2.f;
    float2 pixel = (make_float2(launch_index)) * inv_screen - 1.f;

    float2 jitter_scale = inv_screen / (sqrt(float(num_samples)));
    unsigned int samples_per_pixel = num_samples;
    float3 result = make_float3(0.0f);
    float3 result_shading_normal = make_float3(0.0f);
    float3 result_texture = make_float3(0.0f);
    float3 result_inShadow = make_float3(0.0f);
    float result_depth = 0.0f;

    unsigned int seed = tea<16>(screen.x*launch_index.y+launch_index.x, frame_number);
    do 
    {
        //
        // Sample pixel using jittering
        //
        unsigned int x = samples_per_pixel%int(sqrt(float(num_samples)));
        unsigned int y = samples_per_pixel/int(sqrt(float(num_samples)));
        float2 jitter = make_float2(x-rnd(seed), y-rnd(seed));
        float2 d = pixel + jitter*jitter_scale;
        float3 ray_origin = eye;
        float3 ray_direction = normalize(d.x*U + d.y*V + W);

        // Initialze per-ray data
        PerRayData_pathtrace prd;
        prd.result = make_float3(0.f);
        prd.attenuation = make_float3(1.f);
        prd.radiance = make_float3(0.f);
        prd.shading_normal = make_float3(0.0f);
        prd.texture = make_float3(0.0f);
        prd.depth = 0.0f;
        prd.countEmitted = false;
        prd.done = false;
        prd.seed = seed;
        prd.bounce = 0;
        prd.inShadow = false;

        // Each iteration is a segment of the ray path.  The closest hit will
        // return new segments to be traced here.
        for(;;)
        {
            Ray ray = make_Ray(ray_origin, ray_direction, pathtrace_ray_type, scene_epsilon, RT_DEFAULT_MAX);
            rtTrace(top_object, ray, prd);

            if(prd.done)
            {
                // We have hit the background or a luminaire
                prd.result += prd.radiance * prd.attenuation;
                if(prd.bounce == 0){
                    result_shading_normal += prd.shading_normal;
                    result_texture += prd.texture;
                    result_depth += prd.depth;
                    float3 direct_color = prd.radiance * prd.attenuation;
                    result_inShadow += make_float3(direct_color.x * 0.2126 + direct_color.y * 0.7152 + direct_color.z * 0.0722);
                }
                break;
            }

            // Russian roulette termination 
            if(prd.bounce >= rr_begin_bounce)
            {
                float pcont = fmaxf(prd.attenuation);
                if(rnd(prd.seed) >= pcont || prd.bounce > rr_max_bounce )
                    break;
                prd.attenuation /= pcont;
            }

            prd.bounce++;
            prd.result += prd.radiance * prd.attenuation;
            if(prd.bounce == 1){
                result_shading_normal += prd.shading_normal;
                result_texture += prd.texture;
                result_depth += prd.depth;
                float3 direct_color = prd.radiance * prd.attenuation;
                result_inShadow += make_float3(direct_color.x * 0.2126 + direct_color.y * 0.7152 + direct_color.z * 0.0722);
                
            }

            // Update ray data for the next path segment
            ray_origin = prd.origin;
            ray_direction = prd.direction;
        }

        result += prd.result;
        seed = prd.seed;
    } while (--samples_per_pixel);

    //
    // Update the output buffer
    //
    float3 pixel_color = result / num_samples;
    float3 normal_color = result_shading_normal / num_samples;
    float3 texture_color = result_texture / num_samples;
    float3 depth_color = make_float3(result_depth) / num_samples;
    float3 shadow_color = result_inShadow / num_samples;

    if (frame_number > 1)
    {
        float a = 1.0f / (float)frame_number;
        float3 old_color = make_float3(output_buffer[launch_index]);
        output_buffer[launch_index] = make_float4( lerp( old_color, pixel_color, a ), 1.0f );
    }
    else
    {
        output_buffer[launch_index] = make_float4(pixel_color, 1.0f);
        normal_buffer[launch_index] = make_float4(normal_color, 1.0f);
        texture_buffer[launch_index] = make_float4(texture_color, 1.0f);
        depth_buffer[launch_index] = make_float4(depth_color, 1.0f);
        shadow_buffer[launch_index] = make_float4(shadow_color, 1.0f);
    }
}


//-----------------------------------------------------------------------------
//
//  Emissive surface closest-hit
//
//-----------------------------------------------------------------------------

rtDeclareVariable(float3,        emission_color, , );
rtDeclareVariable(float3,     diffuse_color, , );
rtDeclareVariable(float3,     geometric_normal, attribute geometric_normal, );
rtDeclareVariable(float3,     shading_normal,   attribute shading_normal, );
rtDeclareVariable(optix::Ray, ray,              rtCurrentRay, );
rtDeclareVariable(float,      t_hit,            rtIntersectionDistance, );

RT_PROGRAM void diffuseEmitter()
{
    float3 world_shading_normal = normalize( rtTransformNormal( RT_OBJECT_TO_WORLD, shading_normal ) );
    current_prd.shading_normal = world_shading_normal;
    current_prd.texture =  make_float3(0.0);
    current_prd.depth = t_hit;
    current_prd.radiance = current_prd.countEmitted ? make_float3(0.f) : emission_color;
    current_prd.done = true;
    current_prd.inShadow = false;
}


//-----------------------------------------------------------------------------
//
//  Lambertian surface closest-hit
//
//-----------------------------------------------------------------------------

RT_PROGRAM void diffuse()
{
    float3 world_shading_normal   = normalize( rtTransformNormal( RT_OBJECT_TO_WORLD, shading_normal ) );
    current_prd.shading_normal = world_shading_normal;
    current_prd.texture =  diffuse_color;
    current_prd.depth = t_hit;
    float3 world_geometric_normal = normalize( rtTransformNormal( RT_OBJECT_TO_WORLD, geometric_normal ) );
    float3 ffnormal = faceforward( world_shading_normal, -ray.direction, world_geometric_normal );

    float3 hitpoint = ray.origin + t_hit * ray.direction;

    //
    // Generate a reflection ray.  This will be traced back in ray-gen.
    //
    current_prd.origin = hitpoint;

    float z1=rnd(current_prd.seed);
    float z2=rnd(current_prd.seed);
    float3 p;
    cosine_sample_hemisphere(z1, z2, p);
    optix::Onb onb( ffnormal );
    onb.inverse_transform( p );
    current_prd.direction = p;

    // NOTE: f/pdf = 1 since we are perfectly importance sampling lambertian
    // with cosine density.
    current_prd.attenuation = current_prd.attenuation * diffuse_color;

    //
    // Next event estimation (compute direct lighting).
    //
    current_prd.countEmitted = true;
    unsigned int num_lights = lights.size();
    float3 result = make_float3(0.0f);

    for(int i = 0; i < num_lights; ++i)
    {
        // Choose random point on light
        ParallelogramLight light = lights[i];
        const float z1 = rnd(current_prd.seed);
        const float z2 = rnd(current_prd.seed);
        const float3 light_pos = light.corner + light.v1 * z1 + light.v2 * z2;

        // Calculate properties of light sample (for area based pdf)
        const float  Ldist = length(light_pos - hitpoint);
        const float3 L     = normalize(light_pos - hitpoint);
        const float  nDl   = dot( ffnormal, L );
        const float  LnDl  = dot( light.normal, L );

        // cast shadow ray
        PerRayData_pathtrace_shadow shadow_prd;
        shadow_prd.inShadow = false;
        // Note: bias both ends of the shadow ray, in case the light is also present as geometry in the scene.
        Ray shadow_ray = make_Ray( hitpoint, L, pathtrace_shadow_ray_type, scene_epsilon, Ldist - scene_epsilon );
        rtTrace(top_object, shadow_ray, shadow_prd);
        current_prd.inShadow = shadow_prd.inShadow;

        if ( nDl > 0.0f && LnDl > 0.0f )
        {
            if(!shadow_prd.inShadow)
            {
                const float A = length(cross(light.v1, light.v2));
                // convert area based pdf to solid angle
                const float weight = nDl * LnDl * A / (M_PIf * Ldist * Ldist);
                result += light.emission * weight;
            }
        }
    }

    current_prd.radiance = result;
}


//-----------------------------------------------------------------------------
//
//  Shadow any-hit
//
//-----------------------------------------------------------------------------

rtDeclareVariable(PerRayData_pathtrace_shadow, current_prd_shadow, rtPayload, );

RT_PROGRAM void shadow()
{
    current_prd_shadow.inShadow = true;
    rtTerminateRay();
}

RT_PROGRAM void specular_closest_hit_radiance(){

    float3 world_geo_normal   = normalize( rtTransformNormal( RT_OBJECT_TO_WORLD, geometric_normal ) );
    float3 world_shading_normal = normalize( rtTransformNormal( RT_OBJECT_TO_WORLD, shading_normal ) );
    float3 ffnormal     = faceforward( world_shading_normal, -ray.direction, world_geo_normal );
    float3 hitpoint = ray.origin + t_hit * ray.direction;
    float3 R = reflect(ray.direction, ffnormal );

    current_prd.shading_normal = world_shading_normal;
    current_prd.texture =  make_float3(1.0);
    current_prd.depth = t_hit;

    current_prd.origin = hitpoint;
    current_prd.direction = R;
    current_prd.countEmitted = false;
}

//
// Dielectric surface shader
//
rtDeclareVariable(float3,       cutoff_color, , );
rtDeclareVariable(float,        fresnel_exponent, , );
rtDeclareVariable(float,        fresnel_minimum, , );
rtDeclareVariable(float,        fresnel_maximum, , );
rtDeclareVariable(float,        refraction_index, , );
rtDeclareVariable(int,          refraction_maxdepth, , );
rtDeclareVariable(int,          reflection_maxdepth, , );
rtDeclareVariable(float3,       refraction_color, , );
rtDeclareVariable(float3,       reflection_color, , );
rtDeclareVariable(float3,       extinction_constant, , );


RT_PROGRAM void glass_closest_hit_radiance(){

    float3 world_shading_normal = normalize( rtTransformNormal( RT_OBJECT_TO_WORLD, shading_normal ) );
    current_prd.shading_normal = world_shading_normal;
    current_prd.texture =  reflection_color;
    current_prd.depth = t_hit;

    float3 hitpoint = ray.origin + t_hit * ray.direction;

    float reflection = 1.0f;
    float3 result = make_float3(0.0f);

    float cos_theta = dot(ray.direction, world_shading_normal);

    float3 t;
     if( refract(t, ray.direction, world_shading_normal, refraction_index) ){
         //check for external or internal reflections
         if(cos_theta < 0.0f)//internal
           cos_theta = -cos_theta;
         else//external
           cos_theta = dot(t, world_shading_normal);

         reflection = fresnel_schlick(cos_theta, fresnel_exponent, fresnel_minimum, fresnel_maximum);

         float probability = 0.25 + 0.5 * reflection;
         if(rnd(current_prd.seed) < probability){
             float3 R = reflect(ray.direction, world_shading_normal );
             current_prd.origin = hitpoint + R * scene_epsilon;
             current_prd.attenuation = current_prd.attenuation * reflection * reflection_color / probability;
             current_prd.direction = R;
         }else{
             current_prd.origin = hitpoint + t * scene_epsilon;
             current_prd.attenuation = current_prd.attenuation * (1.0f - reflection) * refraction_color / (1.0f - probability);
             current_prd.direction = t;
         }
     }
     else{ //total reflection
           float3 R = reflect(ray.direction, world_shading_normal );
           current_prd.origin = hitpoint + R * scene_epsilon;
           current_prd.attenuation = current_prd.attenuation * reflection_color;
           current_prd.direction = R;
     }
    current_prd.countEmitted = false;
    
    //Compute direct lighting visibility.
    
    // unsigned int num_lights = lights.size();

    // for(int i = 0; i < num_lights; ++i)
    // {
    //     // Choose random point on light
    //     ParallelogramLight light = lights[i];
    //     const float z1 = rnd(current_prd.seed);
    //     const float z2 = rnd(current_prd.seed);
    //     const float3 light_pos = light.corner + light.v1 * z1 + light.v2 * z2;

    //     // Calculate properties of light sample (for area based pdf)
    //     const float  Ldist = length(light_pos - hitpoint);
    //     const float3 L     = normalize(light_pos - hitpoint);

    //     // cast shadow ray
    //     PerRayData_pathtrace_shadow shadow_prd;
    //     shadow_prd.inShadow = false;//TODO fix
    //     // Note: bias both ends of the shadow ray, in case the light is also present as geometry in the scene.
    //     Ray shadow_ray = make_Ray( hitpoint, L, pathtrace_shadow_ray_type, scene_epsilon, Ldist - scene_epsilon );
    //     rtTrace(top_object, shadow_ray, shadow_prd);
    //     current_prd.inShadow = shadow_prd.inShadow;
    // }        
}

//-----------------------------------------------------------------------------
//
//  Exception program
//
//-----------------------------------------------------------------------------

RT_PROGRAM void exception()
{
    output_buffer[launch_index] = make_float4(bad_color, 1.0f);
    depth_buffer[launch_index] = make_float4(bad_color, 1.0f);
    texture_buffer[launch_index] = make_float4(bad_color, 1.0f);
    normal_buffer[launch_index] = make_float4(bad_color, 1.0f);
    shadow_buffer[launch_index] = make_float4(bad_color, 1.0f);
}


//-----------------------------------------------------------------------------
//
//  Miss program
//
//-----------------------------------------------------------------------------

rtDeclareVariable(float3, bg_color, , );

RT_PROGRAM void miss()
{
    current_prd.radiance = bg_color;
    current_prd.done = true;
}


