override CHUNK_SIZE: f32 = 64.0;
override AO: bool = false;

const PI: f32 = 3.14159265359;
const HALF_PI: f32 = 1.57079632679;

struct Uniforms {
    transposedProjectionViewMatrix : mat4x4<f32>,
    inverseProjectionViewMatrix : mat4x4<f32>,
    cameraPosition : vec3f,
    fov : f32,
    viewportSize : vec2f,
    nearPlane : f32,
    farPlane : f32,
}
@group(0) @binding(0) var<uniform> u : Uniforms;

struct VertexInput {
    @location(0) vertex_position: vec2f,
    @location(1) voxel_position: u32,
    @location(2) voxel_color: u32,
    @location(3) chunk_position: vec3f,
}

struct VertexInputAO {
    @location(0) vertex_position: vec2f,
    @location(1) voxel_position: u32,
    @location(2) voxel_color: u32,
    @location(3) chunk_position: vec3f,
    @location(4) ambient_occlusion: u32,
}

struct VertexOut {
    @builtin(position) pos: vec4f,
    @location(0) vColor: vec4f,
    @location(1) vPos: vec3f,
    @location(2) vSize: f32,
    @location(3) @interpolate(flat) ambient_occlusion: u32,
}

struct FragmentIn {
    @builtin(position) fragPos: vec4f,
    @builtin(sample_index) sampleIndex: u32,
    @location(0) vColor: vec4f,
    @location(1) vPos: vec3f,
    @location(2) vSize: f32,
    @location(3) @interpolate(flat) ambient_occlusion: u32,
}

struct FragmentOut {
    @location(0) color: vec4f,
}

struct Billboard {
    pos : vec2f,
    size : vec2f,
}

fn quadricProj(voxelPosition: vec3f, voxelSize: f32) -> Billboard {
    let quadricMat: vec4f = vec4f(1.0, 1.0, 1.0, -1.0);
    let sphereRadius: f32 = voxelSize * 0.5 * 1.732051;
    let sphereCenter: vec4f = vec4f(voxelPosition, 1.0);
    let viewProj: mat4x4<f32> = u.transposedProjectionViewMatrix;

    let projX = dot(sphereCenter, viewProj[0]);
    let projY = dot(sphereCenter, viewProj[1]);
    let projW = dot(sphereCenter, viewProj[3]);

    var matT: mat3x4<f32> = mat3x4<f32>(
        vec4f(viewProj[0].xyz * sphereRadius, projX),
        vec4f(viewProj[1].xyz * sphereRadius, projY),
        vec4f(viewProj[3].xyz * sphereRadius, projW)
    );

    let matD: mat3x4<f32> = mat3x4<f32>(
        matT[0] * quadricMat,
        matT[1] * quadricMat,
        matT[2] * quadricMat
    );

    let discriminant: f32 = dot(matD[2], matT[2]);
    if (projW < 0.0 || discriminant > 0.0) {
        return Billboard(vec2f(0.0, 0.0), vec2f(0.0, 0.0));
    }

    let eqCoefs: vec4f = vec4f(
        dot(matD[0], matT[2]),
        dot(matD[1], matT[2]),
        dot(matD[0], matT[0]),
        dot(matD[1], matT[1])
    ) / discriminant;

    let outPosition: vec2f = eqCoefs.xy;
    var AABB: vec2f = sqrt(eqCoefs.xy * eqCoefs.xy - eqCoefs.zw);

    return Billboard(outPosition, AABB);
}

fn process_vertex(vertex: VertexInputAO) -> VertexOut {
    let vertexPosition: vec2f = vertex.vertex_position.xy;
    let instanceVoxelPosition: vec4f = unpack4x8unorm(vertex.voxel_position) * 255;
    let voxelColor: vec4f = unpack4x8unorm(vertex.voxel_color);
    let chunkOffset: vec3f = vertex.chunk_position.xyz * CHUNK_SIZE;

    let voxelSize = instanceVoxelPosition.w;

    let voxelPosition = instanceVoxelPosition.xyz - u.cameraPosition + chunkOffset + vec3f(0.5 * voxelSize);

    let billboard: Billboard = quadricProj(voxelPosition, voxelSize);

    let stochasticCoverage: f32 = billboard.size.x * u.viewportSize.x * billboard.size.y * u.viewportSize.y;
    if (stochasticCoverage < 0.8) {
        var out: VertexOut;
        out.pos = vec4f(0.0, 0.0, -1.0, 0.0);
        return out;
    }

    let depth = length(voxelPosition) / u.farPlane;

    var out: VertexOut;
    out.pos = vec4f(vertexPosition * billboard.size + billboard.pos, depth, 1.0);
    out.vColor = voxelColor;
    out.vPos = voxelPosition;
    out.vSize = voxelSize;
    out.ambient_occlusion = vertex.ambient_occlusion;

    return out;
}

@vertex fn vs_main(vertex: VertexInput) -> VertexOut {
    let vertexAO: VertexInputAO = VertexInputAO(
        vertex.vertex_position,
        vertex.voxel_position,
        vertex.voxel_color,
        vertex.chunk_position,
        0
    );
    return process_vertex(vertexAO);
}

@vertex fn vs_main_ao(vertex: VertexInputAO) -> VertexOut {
    return process_vertex(vertex);
}

struct Box {
    center : vec3f,
    radius : vec3f,
}

struct Ray {
    origin : vec3f,
    direction : vec3f,
}

struct Hit {
    isHit : bool,
    distance : f32,
    uv : vec2f,
    normal : vec3f,
    plane : u32,
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

fn getXPlaneUV(rayOrigin : vec3f, rayDirection : vec3f, distance : f32, boxRadius : f32) -> vec2f {
    return (rayOrigin.yz + rayDirection.yz * distance) / boxRadius * 0.5f + 0.5f;
}
fn getYPlaneUV(rayOrigin : vec3f, rayDirection : vec3f, distance : f32, boxRadius : f32) -> vec2f {
    return (rayOrigin.xz + rayDirection.xz * distance) / boxRadius * 0.5f + 0.5f;
}
fn getZPlaneUV(rayOrigin : vec3f, rayDirection : vec3f, distance : f32, boxRadius : f32) -> vec2f {
    return (rayOrigin.xy + rayDirection.xy * distance) / boxRadius * 0.5f + 0.5f;
}

fn intersectBox(box : Box, ray : Ray) -> Hit {
    let rayOrigin : vec3f = ray.origin - box.center;
    let rayDirection : vec3f = ray.direction;

    var sgn : vec3f = -sign(rayDirection);

    let distanceToPlane : vec3f = (box.radius * sgn - rayOrigin) / rayDirection;

    let test : vec3<bool> = rayBoxTest(distanceToPlane, rayOrigin, rayDirection, box.radius);

    var hit : Hit;
    hit.isHit = false;
    hit.distance = 0.0f;
    hit.normal = vec3f(0.0f, 0.0f, 0.0f);

    if (test.x) {
        hit.normal = vec3f(sgn.x, 0.0f, 0.0f);
        hit.distance = distanceToPlane.x;
        if (AO) {
            hit.uv = getXPlaneUV(rayOrigin, rayDirection, distanceToPlane.x, box.radius.x);
            hit.plane = select(0u, 1u, sgn.x > 0.0f);
        }
    } else if (test.y) {
        hit.normal = vec3f(0.0f, sgn.y, 0.0f);
        hit.distance = distanceToPlane.y;
        if (AO) {
            hit.uv = getYPlaneUV(rayOrigin, rayDirection, distanceToPlane.y, box.radius.y);
            hit.plane = select(2u, 3u, sgn.y > 0.0f);
        }
    } else if (test.z) {
        hit.normal = vec3f(0.0f, 0.0f, sgn.z);
        hit.distance = distanceToPlane.z;
        if (AO) {
            hit.uv = getZPlaneUV(rayOrigin, rayDirection, distanceToPlane.z, box.radius.z);
            hit.plane = select(4u, 5u, sgn.z > 0.0f);
        }
    } else {
        return hit;
    }

    hit.isHit = true;

    return hit;
}

fn screenToWorldSpace(screenPos : vec2f) -> vec3f {
    let pos = screenPos * 2.0f - 1.0f;
    let worldPos : vec4f = u.inverseProjectionViewMatrix * vec4f(pos.x, -pos.y, -1.0f, 1.0f);
    return worldPos.xyz / worldPos.w;
}

fn getAmbientOcclusion(cornerNuNv : u32, cornerNuPv : u32, cornerPuNv : u32, cornerPuPv : u32,
              edgeNu : u32, edgePu : u32, edgeNv : u32, edgePv : u32,
              uv : vec2f, ambientOcclusion : u32) -> f32 {

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

    return mix(0.25, 1.0, sin(factor * HALF_PI));
}

fn applyAmbientOcclusion(color : vec3f, uv : vec2f, plane : u32, ambientOcclusion : u32) -> vec3f {

    const CornerNxNyNz = 1 << 0;
    const CornerNxNyPz = 1 << 1;
    const CornerNxPyNz = 1 << 2;
    const CornerNxPyPz = 1 << 3;
    const CornerPxNyNz = 1 << 4;
    const CornerPxNyPz = 1 << 5;
    const CornerPxPyNz = 1 << 6;
    const CornerPxPyPz = 1 << 7;

    const EdgeNxNy = 1 << 8;
    const EdgeNxPy = 1 << 9;
    const EdgePxNy = 1 << 10;
    const EdgePxPy = 1 << 11;
    const EdgeNxNz = 1 << 12;
    const EdgeNxPz = 1 << 13;
    const EdgePxNz = 1 << 14;
    const EdgePxPz = 1 << 15;
    const EdgeNyNz = 1 << 16;
    const EdgeNyPz = 1 << 17;
    const EdgePyNz = 1 << 18;
    const EdgePyPz = 1 << 19;

    const PlaneNx = 0; // Negative X (Left Plane)
    const PlanePx = 1; // Positive X (Right Plane)
    const PlaneNy = 2; // Negative Y (Bottom Plane)
    const PlanePy = 3; // Positive Y (Top Plane)
    const PlaneNz = 4; // Negative Z (Front Plane)
    const PlanePz = 5; // Positive Z (Back Plane)

    const Corners = array<array<u32, 4>, 6>(
        array<u32, 4>(CornerNxNyNz, CornerNxNyPz, CornerNxPyNz, CornerNxPyPz), // PlaneNx
        array<u32, 4>(CornerPxNyNz, CornerPxNyPz, CornerPxPyNz, CornerPxPyPz), // PlanePx
        array<u32, 4>(CornerNxNyNz, CornerNxNyPz, CornerPxNyNz, CornerPxNyPz), // PlaneNy
        array<u32, 4>(CornerNxPyNz, CornerNxPyPz, CornerPxPyNz, CornerPxPyPz), // PlanePy
        array<u32, 4>(CornerNxNyNz, CornerNxPyNz, CornerPxNyNz, CornerPxPyNz), // PlaneNz
        array<u32, 4>(CornerNxNyPz, CornerNxPyPz, CornerPxNyPz, CornerPxPyPz)  // PlanePz
    );

    const Edges = array<array<u32, 4>, 6>(
        array<u32, 4>(EdgeNxNy, EdgeNxPy, EdgeNxNz, EdgeNxPz), // PlaneNx
        array<u32, 4>(EdgePxNy, EdgePxPy, EdgePxNz, EdgePxPz), // PlanePx
        array<u32, 4>(EdgeNxNy, EdgePxNy, EdgeNyNz, EdgeNyPz), // PlaneNy
        array<u32, 4>(EdgeNxPy, EdgePxPy, EdgePyNz, EdgePyPz), // PlanePy
        array<u32, 4>(EdgeNxNz, EdgePxNz, EdgeNyNz, EdgePyNz), // PlaneNz
        array<u32, 4>(EdgeNxPz, EdgePxPz, EdgeNyPz, EdgePyPz)  // PlanePz
    );

    return color * getAmbientOcclusion(
        Corners[plane][0], Corners[plane][1], Corners[plane][2], Corners[plane][3],
        Edges[plane][0], Edges[plane][1], Edges[plane][2], Edges[plane][3],
        uv, ambientOcclusion
    );
}

@fragment fn fs_main(in : FragmentIn) -> FragmentOut {
    var out : FragmentOut;

    // crosshair
    if (length(in.fragPos.xy - u.viewportSize.xy * 0.5) < 2.0) {
        out.color = vec4f(1.0f, 1.0f, 1.0f, 1.0f);
        return out;
    }

    let lightDir : vec3f = normalize(vec3f(0.2f, 0.8f, -0.5f));
    var ray : Ray;
    ray.origin = vec3f(0.0, 0.0, 0.0);
    ray.direction = normalize(screenToWorldSpace(in.fragPos.xy / u.viewportSize.xy));

    var box : Box;
    box.center = in.vPos;
    box.radius = vec3f(in.vSize * 0.5);

    let hit : Hit = intersectBox(box, ray);

    if (hit.isHit) {
        let ambientLight : f32 = 0.3f;
        let directionalLight : f32 = max(dot(hit.normal, lightDir), 0.0f) * 0.7f;

        let light = ambientLight + directionalLight;

        let fog : f32 = sqrt(clamp(hit.distance / u.farPlane, 0.0f, 1.0f)) * 0.3f;
        let fogColor : vec3f = vec3f(0.41f, 0.42f, 0.5f);

        var color : vec3f;

        if (AO) {
            color = applyAmbientOcclusion(in.vColor.rgb, hit.uv, hit.plane, in.ambient_occlusion);
        } else {
            color = in.vColor.rgb;
        }

        color *= light;

        out.color = vec4f(mix(color, fogColor, fog), 1.0f);
    } else {
        discard;
    }

    return out;
}
