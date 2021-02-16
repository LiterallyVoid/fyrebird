#version 450

layout(set = 0, binding = 0) uniform info {
    uvec2 image_size;
} data;

layout(set = 0, binding = 1) readonly buffer Input {
    mat4 objects[];
} objects;

layout(set = 0, binding = 2) buffer Output {
    vec4 pixels[];
} image;

bool cast_sphere(vec3 start, vec3 dir, inout mat4 result, vec4 sphere) {
    vec3 rel = sphere.xyz - start;

    float along = dot(rel, dir);
    if (along < 0.0) {
        return false;
    }

    float dst = dot(rel, rel);
    float b = (sphere.w * sphere.w) - (dst - (along * along));
    if (b < 0.0) {
        return false;
    }

    float distance = along - sqrt(b);
    if (distance < result[0].w) {
        vec3 impact = start + dir * distance;
        result[0].xyz = normalize(impact - sphere.xyz);
        result[1].xyz = impact;
        result[0].w = distance;
        return true;
    }
    return false;
}

bool cast_plane(vec3 start, vec3 dir, inout mat4 result, vec4 plane) {
    float distance = -dot(vec4(start, 1.0), plane) / dot(dir, plane.xyz);
    if (distance > 0.0 && distance < result[0].w) {
        vec3 impact = start + dir * distance;
        result[0].xyz = -plane.xyz;
        result[1].xyz = impact;
        result[0].w = distance;
        return true;
    }
    return false;
}

void material(inout mat4 result, vec3 albedo, float roughness, vec3 emissive) {
    result[2] = vec4(albedo, roughness);
    result[3] = vec4(emissive, 0.0);
}

void cast_ray(vec3 start, vec3 dir, inout mat4 result) {
    if (cast_sphere(start, dir, result, vec4(1.0, -0.5, -0.5, 0.5))) {
        material(result, vec3(1.0, 1.0, 1.0), 0.1, vec3(0.0));
    }
    if (cast_sphere(start, dir, result, vec4(0.0, -1.5, -0.75, 0.25))) {
        material(result, vec3(0.95, 0.98, 0.97), 0.0, vec3(0.0));
    }
    if (cast_sphere(start, dir, result, vec4(-1.0, 0.5, -0.5, 0.5))) {
        material(result, vec3(1.0, 1.0, 1.0), 1.0, vec3(0.0));
    }
    if (cast_plane(start, dir, result, vec4(0.0, 0.0, -1.0, -1.0))) {
        material(result, vec3(1.0, 1.0, 1.0), 1.0, vec3(0.0));
    }
    if (cast_plane(start, dir, result, vec4(0.0, 0.0, 1.0, -2.0))) {
        material(result, vec3(0.0), 0.0, vec3(1.0));
    }
    if (cast_plane(start, dir, result, vec4(1.0, 0.0, 0.0, -2.0))) {
        material(result, vec3(0.6, 0.1, 0.6), 1.0, vec3(0.0));
    }
    if (cast_plane(start, dir, result, vec4(-1.0, 0.0, 0.0, -2.0))) {
        material(result, vec3(1.0, 0.5, 0.3), 1.0, vec3(0.0));
    }
    if (cast_plane(start, dir, result, vec4(0.0, 1.0, 0.0, -3.0))) {
        material(result, vec3(0.3, 0.6, 1.0), 1.0, vec3(0.0));
    }
}

float rand(inout uint seed) {
    seed ^= (seed << 13);
    seed ^= (seed >> 17);
    seed ^= (seed << 5);
    return float(seed) * (1.0 / 4294967296.0);
}

vec3 trace(vec3 start, vec3 dir, inout uint seed) {
    vec3 total = vec3(0.001);
    vec3 mul = vec3(1.0);
    for (int i = 0; i < 8; i++) {
        mat4 result = mat4(0.0);
        result[0].w = 100000.0;
        cast_ray(start, dir, result);
        if (result[0].w > 90000.0) {
            break;
        }

        vec3 normal = result[0].xyz;
        vec3 impact = result[1].xyz;
        vec3 albedo = result[2].xyz;
        vec3 emission = result[3].xyz;
        total += emission * mul;

        mul *= albedo;
        if ((mul.r + mul.g + mul.b) < 0.00001) {
            break;
        }

        vec3 vec;
        for (int i = 0; i < 4; i++) {
            float x = rand(seed) * 2.0 - 1.0;
            float y = rand(seed) * 2.0 - 1.0;
            float z = rand(seed) * 2.0 - 1.0;
            vec = vec3(x, y, z);
            if (length(vec) < 1.0) {
                break;
            }
        }
        vec = normalize(vec);
        if (dot(vec, normal) < 0.0) {
            vec *= -1.0;
        }

        start = impact + normal * 0.0001;
        dir = mix(reflect(dir, normal), vec, result[2].w);
    }

    return total;
}

void main() {
    uvec2 gid = gl_GlobalInvocationID.xy;

    uint seed = gid.y * 1920 + gid.x;

    int samples = 128;

    vec3 color = vec3(0.0);
    for (int i = 0; i < samples; i++) {
        vec2 gid_f = vec2(gid);
        gid_f.x += rand(seed);
        gid_f.y += rand(seed);
        vec2 pos = (gid_f / vec2(data.image_size)) * 2.0 - 1.0;
        pos.y *= -1.0;
        vec2 scale = vec2((1.0 / tan(1.5707 * 0.5)));
        scale.x *= float(data.image_size.x) / float(data.image_size.y);
        pos *= scale;

        vec3 start = vec3(0.0, -3.0, 0.0);
        vec3 dir = normalize(vec3(pos.x, 1.0, pos.y));

        color += trace(start, dir, seed) / float(samples);
    }
    image.pixels[gid.y * data.image_size.x + gid.x] = vec4(color, 1.0);
}
