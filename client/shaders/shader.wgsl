override CHUNK_SIZE: f32 = 64.0;
override LIGHTING: bool = true;
override FOG: bool = true;

override POINT_LIGHT: bool = true;
const POINT_LIGHT_RANGE: f32 = 64.0;
const POINT_LIGHT_INTENSITY: f32 = 2.3;

const PI: f32 = 3.14159265359;
const HALF_PI: f32 = 1.57079632679;

struct Uniforms {
    projectionViewMatrix: mat4x4<f32>,
    inverseProjectionViewMatrix: mat4x4<f32>,
    cameraPosition: vec3f,
    fov: f32,
    viewportSize: vec2f,
    nearPlane: f32,
    farPlane: f32,
    time: f32,
}
struct BlockTextures {
    eastTextureId: u32,
    topTextureId: u32,
    northTextureId: u32,
    westTextureId: u32,
    bottomTextureId: u32,
    southTextureId: u32,
};
@group(0) @binding(0) var<uniform> u: Uniforms;
@group(0) @binding(1) var<storage, read> blockTextures: array<BlockTextures>;
@group(0) @binding(2) var textureArray: texture_2d_array<f32>;

struct VertexInput {
    @location(0) vertexPosition: vec2f,
    @location(1) voxelPosition: u32,
    @location(2) voxelData: u32,
    @location(3) chunkPosition: vec3f,
    @location(4) ambientOcclusion: u32,
    @location(5) light: u32,
}

struct VertexOut {
    @builtin(position) pos: vec4f,
    @location(0) vPos: vec3f,
    @location(1) vSize: f32,
    @location(2) @interpolate(flat) ambientOcclusion: u32,
    @location(3) @interpolate(flat) isTexturedVoxel: u32,
    @location(4) @interpolate(flat) emitsLight: u32,
    @location(5) @interpolate(flat) blockId: u32,
    @location(6) @interpolate(flat) voxelColor: vec3f,
    @location(7) faceLightNx: vec3f,
    @location(8) faceLightPx: vec3f,
    @location(9) faceLightNy: vec3f,
    @location(10) faceLightPy: vec3f,
    @location(11) faceLightNz: vec3f,
    @location(12) faceLightPz: vec3f,
}

struct FragmentIn {
    @builtin(position) fragPos: vec4f,
    @builtin(sample_index) sampleIndex: u32,
    @location(0) vPos: vec3f,
    @location(1) vSize: f32,
    @location(2) @interpolate(flat) ambientOcclusion: u32,
    @location(3) @interpolate(flat) isTexturedVoxel: u32,
    @location(4) @interpolate(flat) emitsLight: u32,
    @location(5) @interpolate(flat) blockId: u32,
    @location(6) @interpolate(flat) voxelColor: vec3f,
    @location(7) faceLightNx: vec3f,
    @location(8) faceLightPx: vec3f,
    @location(9) faceLightNy: vec3f,
    @location(10) faceLightPy: vec3f,
    @location(11) faceLightNz: vec3f,
    @location(12) faceLightPz: vec3f,
}

struct FragmentOut {
    @location(0) color: vec4f,
}

const planeNx = 0; // Negative X (Left Plane)
const planePx = 1; // Positive X (Right Plane)
const planeNy = 2; // Negative Y (Bottom Plane)
const planePy = 3; // Positive Y (Top Plane)
const planeNz = 4; // Negative Z (Front Plane)
const planePz = 5; // Positive Z (Back Plane)

fn calculateBillboard(voxelPosition: vec3f, voxelSize: f32, vertexPosition: vec2f) -> vec4f {
    // Calculate the 8 corners of the voxel's bounding box in world space
    let halfSize = 0.5 * voxelSize;
    let corners = array<vec3f, 8>(
        voxelPosition + vec3f(-halfSize, -halfSize, -halfSize),
        voxelPosition + vec3f(-halfSize, -halfSize,  halfSize),
        voxelPosition + vec3f(-halfSize,  halfSize, -halfSize),
        voxelPosition + vec3f(-halfSize,  halfSize,  halfSize),
        voxelPosition + vec3f( halfSize, -halfSize, -halfSize),
        voxelPosition + vec3f( halfSize, -halfSize,  halfSize),
        voxelPosition + vec3f( halfSize,  halfSize, -halfSize),
        voxelPosition + vec3f( halfSize,  halfSize,  halfSize)
    );

    // Project corners to NDC and track which are in front of the camera (p.w > 0)
    var minNdc = vec2f(1.0, 1.0);
    var maxNdc = vec2f(-1.0, -1.0);
    var offLeft = 0u;
    var offRight = 0u;
    var offTop = 0u;
    var offBottom = 0u;
    var inFront = 0u;
    for (var i = 0u; i < 8u; i = i + 1u) {
        var p = u.projectionViewMatrix * vec4f(corners[i], 1.0);
        if (p.w > 0.0) {
            p /= p.w;
            minNdc = min(minNdc, p.xy);
            maxNdc = max(maxNdc, p.xy);
            if (p.x < -1.0) { offLeft++; }
            if (p.x >  1.0) { offRight++; }
            if (p.y < -1.0) { offBottom++; }
            if (p.y >  1.0) { offTop++; }
            inFront++;
        }
    }

    // If all corners are behind the camera or all onscreen corners are offscreen, skip rendering
    if (inFront == 0u || offLeft == inFront || offRight == inFront || offTop == inFront || offBottom == inFront) {
        return vec4f(0.0, 0.0, 0.0, 1.0);
    }

    // Expand bounds slightly before clamping to avoid missing edge pixels
    let epsilon = vec2f(0.0001, 0.0001);
    minNdc = clamp(minNdc - epsilon, vec2f(-1.0), vec2f(1.0));
    maxNdc = clamp(maxNdc + epsilon, vec2f(-1.0), vec2f(1.0));

    // Calculate the center and size of the billboard in NDC
    let centerNdc = 0.5 * (minNdc + maxNdc);
    let sizeNdc = maxNdc - minNdc;

    // Offset the billboard quad using the input vertexPosition
    let billboardNdc = centerNdc + vertexPosition * 0.5 * sizeNdc;

    // Use normalized depth for the billboard
    let depth = length(voxelPosition) / u.farPlane;

    return vec4f(billboardNdc, depth, 1.0);
}

fn unpackPackedLight(packed: u32) -> array<vec3f, 6> {
    var nx = f32((packed >> 0u) & 0x1Fu) / 31.0;
    var px = f32((packed >> 5u) & 0x1Fu) / 31.0;
    var ny = f32((packed >> 10u) & 0x1Fu) / 31.0;
    var py = f32((packed >> 15u) & 0x1Fu) / 31.0;
    var nz = f32((packed >> 20u) & 0x1Fu) / 31.0;
    var pz = f32((packed >> 25u) & 0x1Fu) / 31.0;

    let exponent = 2.2;
    nx = pow(nx, exponent);
    px = pow(px, exponent);
    ny = pow(ny, exponent);
    py = pow(py, exponent);
    nz = pow(nz, exponent);
    pz = pow(pz, exponent);

    return array<vec3f, 6>(vec3f(nx), vec3f(px), vec3f(ny), vec3f(py), vec3f(nz), vec3f(pz));
}

fn processVertex(vertex: VertexInput) -> VertexOut {
    let vertexPosition: vec2f = vertex.vertexPosition.xy;
    let instanceVoxelPosition: vec4f = unpack4x8unorm(vertex.voxelPosition) * 255;
    let chunkOffset: vec3f = vertex.chunkPosition.xyz * CHUNK_SIZE;

    let voxelSize = instanceVoxelPosition.w;
    let voxelPosition = instanceVoxelPosition.xyz - u.cameraPosition + chunkOffset + vec3f(0.5 * voxelSize);

    let faceLights = unpackPackedLight(vertex.light);

    var out: VertexOut;
    out.pos = calculateBillboard(voxelPosition, voxelSize, vertexPosition);
    out.vPos = voxelPosition;
    out.vSize = voxelSize;
    out.ambientOcclusion = vertex.ambientOcclusion;
    out.isTexturedVoxel = ((vertex.voxelData >> 24u) & 1u);
    out.emitsLight = ((vertex.voxelData >> 25u) & 1u);
    out.blockId = vertex.voxelData & 0xFFFFFFu;
    out.voxelColor = vec3f(
        f32((vertex.voxelData >> 16u) & 0xFFu) / 255.0,
        f32((vertex.voxelData >> 8u) & 0xFFu) / 255.0,
        f32(vertex.voxelData & 0xFFu) / 255.0
    );
    out.faceLightNx = faceLights[planeNx];
    out.faceLightPx = faceLights[planePx];
    out.faceLightNy = faceLights[planeNy];
    out.faceLightPy = faceLights[planePy];
    out.faceLightNz = faceLights[planeNz];
    out.faceLightPz = faceLights[planePz];
    return out;
}

@vertex fn vsMain(vertex: VertexInput) -> VertexOut {
    return processVertex(vertex);
}

struct Box {
    center: vec3f,
    radius: vec3f,
}

struct Ray {
    origin: vec3f,
    direction: vec3f,
}

struct Hit {
    isHit: bool,
    distance: f32,
    uv: vec2f,
    normal: vec3f,
    plane: u32,
}

fn rayBoxTest(distanceToPlane: vec3f, rayOrigin: vec3f, rayDirection: vec3f, boxRadius: vec3f) -> vec3<bool> {
    let pointOnXPlane = rayOrigin.yz + rayDirection.yz * distanceToPlane.x;
    let intersectsXPlane = all(abs(pointOnXPlane) < boxRadius.yz);

    let pointOnYPlane = rayOrigin.xz + rayDirection.xz * distanceToPlane.y;
    let intersectsYPlane = all(abs(pointOnYPlane) < boxRadius.xz);

    let pointOnZPlane = rayOrigin.xy + rayDirection.xy * distanceToPlane.z;
    let intersectsZPlane = all(abs(pointOnZPlane) < boxRadius.xy);

    return vec3<bool>(intersectsXPlane, intersectsYPlane, intersectsZPlane);
}

fn getXPlaneUv(rayOrigin: vec3f, rayDirection: vec3f, distance: f32, boxRadius: f32) -> vec2f {
    return (rayOrigin.yz + rayDirection.yz * distance) / boxRadius * 0.5f + 0.5f;
}
fn getYPlaneUv(rayOrigin: vec3f, rayDirection: vec3f, distance: f32, boxRadius: f32) -> vec2f {
    return (rayOrigin.xz + rayDirection.xz * distance) / boxRadius * 0.5f + 0.5f;
}
fn getZPlaneUv(rayOrigin: vec3f, rayDirection: vec3f, distance: f32, boxRadius: f32) -> vec2f {
    return (rayOrigin.xy + rayDirection.xy * distance) / boxRadius * 0.5f + 0.5f;
}

fn intersectBox(box: Box, ray: Ray) -> Hit {
    let rayOrigin: vec3f = ray.origin - box.center;
    let rayDirection: vec3f = ray.direction;

    var sgn: vec3f = -sign(rayDirection);

    let distanceToPlane: vec3f = (box.radius * sgn - rayOrigin) / rayDirection;

    let test: vec3<bool> = rayBoxTest(distanceToPlane, rayOrigin, rayDirection, box.radius);

    var hit: Hit;
    hit.isHit = false;
    hit.distance = 0.0f;
    hit.normal = vec3f(0.0f, 0.0f, 0.0f);

    if (test.x) {
        hit.normal = vec3f(sgn.x, 0.0f, 0.0f);
        hit.distance = distanceToPlane.x;
        hit.uv = getXPlaneUv(rayOrigin, rayDirection, distanceToPlane.x, box.radius.x);
        hit.plane = select(0u, 1u, sgn.x > 0.0f);
    } else if (test.y) {
        hit.normal = vec3f(0.0f, sgn.y, 0.0f);
        hit.distance = distanceToPlane.y;
        hit.uv = getYPlaneUv(rayOrigin, rayDirection, distanceToPlane.y, box.radius.y);
        hit.plane = select(2u, 3u, sgn.y > 0.0f);
    } else if (test.z) {
        hit.normal = vec3f(0.0f, 0.0f, sgn.z);
        hit.distance = distanceToPlane.z;
        hit.uv = getZPlaneUv(rayOrigin, rayDirection, distanceToPlane.z, box.radius.z);
        hit.plane = select(4u, 5u, sgn.z > 0.0f);
    } else {
        return hit;
    }

    hit.isHit = true;

    return hit;
}

fn screenToWorldSpace(screenPos: vec2f) -> vec3f {
    let pos = screenPos * 2.0f - 1.0f;
    let worldPos: vec4f = u.inverseProjectionViewMatrix * vec4f(pos.x, -pos.y, -1.0f, 1.0f);
    return worldPos.xyz / worldPos.w;
}

fn getAmbientOcclusion(cornerNuNv: u32, cornerNuPv: u32, cornerPuNv: u32, cornerPuPv: u32,
                       edgeNu: u32, edgePu: u32, edgeNv: u32, edgePv: u32,
                       uv: vec2f, ambientOcclusion: u32) -> f32 {

    let hasEdgeNu = bool(edgeNu & ambientOcclusion);
    let hasEdgePu = bool(edgePu & ambientOcclusion);
    let hasEdgeNv = bool(edgeNv & ambientOcclusion);
    let hasEdgePv = bool(edgePv & ambientOcclusion);

    let hasCornerNuNv = bool(cornerNuNv & ambientOcclusion);
    let hasCornerNuPv = bool(cornerNuPv & ambientOcclusion);
    let hasCornerPuNv = bool(cornerPuNv & ambientOcclusion);
    let hasCornerPuPv = bool(cornerPuPv & ambientOcclusion);

    var factor = 1.0;

    if (hasEdgeNu) {
        factor *= uv.x;
    }
    if (hasEdgePu) {
        factor *= 1.0 - uv.x;
    }
    if (hasEdgeNv) {
        factor *= uv.y;
    }
    if (hasEdgePv) {
        factor *= 1.0 - uv.y;
    }

    if (!hasEdgeNu && !hasEdgeNv && hasCornerNuNv) {
        factor *= 1.0 - (1.0 - uv.x) * (1.0 - uv.y);
    }
    if (!hasEdgeNu && !hasEdgePv && hasCornerNuPv) {
        factor *= 1.0 - (1.0 - uv.x) * uv.y;
    }
    if (!hasEdgePu && !hasEdgeNv && hasCornerPuNv) {
        factor *= 1.0 - uv.x * (1.0 - uv.y);
    }
    if (!hasEdgePu && !hasEdgePv && hasCornerPuPv) {
        factor *= 1.0 - uv.x * uv.y;
    }

    return mix(0.5, 1.0, sin(factor * HALF_PI));
}

fn applyAmbientOcclusion(color: vec3f, uv: vec2f, plane: u32, ambientOcclusion: u32) -> vec3f {

    const cornerNxNyNz = 1 << 0;
    const cornerNxNyPz = 1 << 1;
    const cornerNxPyNz = 1 << 2;
    const cornerNxPyPz = 1 << 3;
    const cornerPxNyNz = 1 << 4;
    const cornerPxNyPz = 1 << 5;
    const cornerPxPyNz = 1 << 6;
    const cornerPxPyPz = 1 << 7;

    const edgeNxNy = 1 << 8;
    const edgeNxPy = 1 << 9;
    const edgePxNy = 1 << 10;
    const edgePxPy = 1 << 11;
    const edgeNxNz = 1 << 12;
    const edgeNxPz = 1 << 13;
    const edgePxNz = 1 << 14;
    const edgePxPz = 1 << 15;
    const edgeNyNz = 1 << 16;
    const edgeNyPz = 1 << 17;
    const edgePyNz = 1 << 18;
    const edgePyPz = 1 << 19;

    const corners = array<array<u32, 4>, 6>(
        array<u32, 4>(cornerNxNyNz, cornerNxNyPz, cornerNxPyNz, cornerNxPyPz), // planeNx
        array<u32, 4>(cornerPxNyNz, cornerPxNyPz, cornerPxPyNz, cornerPxPyPz), // planePx
        array<u32, 4>(cornerNxNyNz, cornerNxNyPz, cornerPxNyNz, cornerPxNyPz), // planeNy
        array<u32, 4>(cornerNxPyNz, cornerNxPyPz, cornerPxPyNz, cornerPxPyPz), // planePy
        array<u32, 4>(cornerNxNyNz, cornerNxPyNz, cornerPxNyNz, cornerPxPyNz), // planeNz
        array<u32, 4>(cornerNxNyPz, cornerNxPyPz, cornerPxNyPz, cornerPxPyPz)  // planePz
    );

    const edges = array<array<u32, 4>, 6>(
        array<u32, 4>(edgeNxNy, edgeNxPy, edgeNxNz, edgeNxPz), // planeNx
        array<u32, 4>(edgePxNy, edgePxPy, edgePxNz, edgePxPz), // planePx
        array<u32, 4>(edgeNxNy, edgePxNy, edgeNyNz, edgeNyPz), // planeNy
        array<u32, 4>(edgeNxPy, edgePxPy, edgePyNz, edgePyPz), // planePy
        array<u32, 4>(edgeNxNz, edgePxNz, edgeNyNz, edgePyNz), // planeNz
        array<u32, 4>(edgeNxPz, edgePxPz, edgeNyPz, edgePyPz)  // planePz
    );

    return color * getAmbientOcclusion(
        corners[plane][0], corners[plane][1], corners[plane][2], corners[plane][3],
        edges[plane][0], edges[plane][1], edges[plane][2], edges[plane][3],
        uv, ambientOcclusion
    );
}

fn getTextureId(blockId: u32, plane: u32) -> u32 {
    let bt = blockTextures[blockId];
    switch (plane) {
        case planeNx: { return bt.westTextureId; }
        case planePx: { return bt.eastTextureId; }
        case planeNy: { return bt.bottomTextureId; }
        case planePy: { return bt.topTextureId; }
        case planeNz: { return bt.northTextureId; }
        case planePz: { return bt.southTextureId; }
        default: { return bt.eastTextureId; }
    }
}

fn remapUv(uv: vec2f, plane: u32) -> vec2f {
    switch (plane) {
        case planeNx: { return vec2f(1.0 - uv.y, 1.0 - uv.x); }
        case planePx: { return vec2f(uv.y, 1.0 - uv.x); }
        case planeNz: { return vec2f(uv.x, 1.0 - uv.y); }
        case planePz: { return vec2f(1.0 - uv.x, 1.0 - uv.y); }
        default: { return uv; }
    }
}

struct TorchFlicker {
    color: vec3f,
    intensity: f32,
    rangeScale: f32,
    posOffset: vec3f,
}

fn hash11u(x: u32) -> f32 {
    var h = x;
    h ^= h * 0x3d20adeau;
    h ^= h >> 15;
    h *= 0x05526c56u;
    h ^= h >> 15;
    return f32(h & 0x00FFFFFFu) * (1.0 / 16777216.0);
}

fn torchFlicker(t: f32) -> TorchFlicker {
    // 1) Low‑freq base wave
    let baseWave = sin(t * 1.35); // [-1,1]

    // 2) Smoothed value noise
    let noiseRate = 3.0;                    // samples per second
    let nf      = t * noiseRate;
    let seg     = u32(floor(nf));
    let f       = fract(nf);
    let fHerm   = f * f * (3.0 - 2.0 * f);  // smoothstep
    let r0      = hash11u(seg + 101u);
    let r1      = hash11u(seg + 102u);
    let noise01 = mix(r0, r1, fHerm);       // [0,1]
    let noise   = noise01 * 2.0 - 1.0;      // [-1,1]

    // 3) Combine & gentle shaping
    let composite = baseWave * 0.55 + noise * 0.45;       // ~[-1,1]
    let c01 = composite * 0.5 + 0.5;                      // [0,1]
    let shaped = mix(c01 * c01, c01, 0.4);                // soften highs
    let centered = (shaped - 0.5) * 2.0;                  // back to ~[-1,1]

    // 4) Final intensity
    let base = 0.89;
    let amp  = 0.066;
    let flicker = base + centered * amp;                  // ~[base-amp, base+amp]

    // 5) Range scaling
    let rangeScale = 0.97 + (flicker - base) * 0.25;

    // 6) Color warmth
    let warmLow  = vec3f(1.0, 0.63, 0.37);
    let warmHigh = vec3f(1.0, 0.86, 0.55);
    let warmth = clamp((flicker - (base - 0.03)) / 0.13, 0.0, 1.0);
    let color = mix(warmLow, warmHigh, warmth);

    // 7) Positional jitter via slower smoothed noise
    let jRate = 0.8;
    let jf    = t * jRate;
    let jseg  = u32(floor(jf));
    let jfFr  = fract(jf);
    let jfSm  = jfFr * jfFr * (3.0 - 2.0 * jfFr);
    let jx = mix(hash11u(jseg + 1001u), hash11u(jseg + 1002u), jfSm) * 2.0 - 1.0;
    let jy = mix(hash11u(jseg + 2001u), hash11u(jseg + 2002u), jfSm) * 2.0 - 1.0;
    let jz = mix(hash11u(jseg + 3001u), hash11u(jseg + 3002u), jfSm) * 2.0 - 1.0;
    let posOffset = vec3f(jx * 0.025, jy * 0.018, jz * 0.025);

    return TorchFlicker(color, flicker, rangeScale, posOffset);
}

@fragment fn fsMain(input: FragmentIn) -> FragmentOut {
    var output: FragmentOut;

    var ray: Ray;
    ray.origin = vec3f(0.0, 0.0, 0.0);
    ray.direction = normalize(screenToWorldSpace(input.fragPos.xy / u.viewportSize.xy));

    var box: Box;
    box.center = input.vPos;
    box.radius = vec3f(input.vSize * 0.5);

    let hit: Hit = intersectBox(box, ray);

    if (hit.isHit) {
        var albedo: vec3f;
        if (input.isTexturedVoxel != 0) {
            let textureId: u32 = getTextureId(input.blockId, hit.plane);
            let uv = remapUv(hit.uv, hit.plane);
            albedo = textureLoad(textureArray, vec2i(uv * 16.0f), textureId, 0).rgb;
        } else {
            albedo = input.voxelColor;
        }

        var color: vec3f;

        // Apply ambient occlusion and lighting only to non-emissive voxels
        if (input.emitsLight == 0) {
            albedo = applyAmbientOcclusion(albedo, hit.uv, hit.plane, input.ambientOcclusion);
        }

        var lightingFactor = vec3f(1.0);
        if (LIGHTING) {
            let ambient : f32 = 0.033;
            let faceLight = array<vec3f,6>(
                input.faceLightNx, input.faceLightPx,
                input.faceLightNy, input.faceLightPy,
                input.faceLightNz, input.faceLightPz
            )[hit.plane];
            let baseLight = vec3f(ambient) + faceLight * (1.0 - ambient);

            var pointLight = vec3f(0.0);

            if (POINT_LIGHT) {
                let t = u.time;
                let flick = torchFlicker(t);

                let hitPoint = ray.direction * hit.distance;
                let lightPos = flick.posOffset;
                let toLight = lightPos - hitPoint;
                let dist = length(toLight);
                let range = POINT_LIGHT_RANGE * flick.rangeScale;

                if (dist < range) {
                    let lightDir = toLight / dist;
                    let attenuation = pow(max(0.0, 1.0 - dist / range), 2.2);
                    let ndl = max(dot(hit.normal, lightDir), 0.0);

                    pointLight = flick.color * ndl * attenuation * (POINT_LIGHT_INTENSITY * flick.intensity);
                }
            }

            let combinedLight = baseLight + pointLight * (vec3f(1.0) - baseLight);

            lightingFactor = combinedLight / (combinedLight + vec3f(0.75));;
        }

        color = albedo * lightingFactor;
        color = clamp(color, vec3f(0.0), vec3f(1.0));

        if (input.emitsLight == 1) { // Emissive voxel
            let t = u.time + dot(input.vPos, vec3f(12.9898, 78.2332, 37.7193));
            let flick = torchFlicker(t);
            color *= flick.intensity;
            color = clamp(color, vec3f(0.0), vec3f(1.0));
        }

        if (FOG) {
            let fogFactor: f32 = sqrt(clamp(hit.distance / u.farPlane, 0.0f, 1.0f)) * 0.3f;
            let fogColor: vec3f = vec3f(0.41f, 0.42f, 0.5f);
            color = mix(color, fogColor, fogFactor);
        }

        output.color = vec4f(color, 1.0f);
    } else {
        discard;
    }

    return output;
}