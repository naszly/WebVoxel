override CHUNK_SIZE: f32 = 64.0;
override AO: bool = false;
override LIGHTING: bool = false;
override FOG: bool = false;

const PI: f32 = 3.14159265359;
const HALF_PI: f32 = 1.57079632679;

struct Uniforms {
    transposedProjectionViewMatrix: mat4x4<f32>,
    inverseProjectionViewMatrix: mat4x4<f32>,
    cameraPosition: vec3f,
    fov: f32,
    viewportSize: vec2f,
    nearPlane: f32,
    farPlane: f32,
}
@group(0) @binding(0) var<uniform> u: Uniforms;

struct VertexInput {
    @location(0) vertexPosition: vec2f,
    @location(1) voxelPosition: u32,
    @location(2) voxelId: u32,
    @location(3) chunkPosition: vec3f,
}

struct VertexInputAo {
    @location(0) vertexPosition: vec2f,
    @location(1) voxelPosition: u32,
    @location(2) voxelId: u32,
    @location(3) chunkPosition: vec3f,
    @location(4) ambientOcclusion: u32,
}

struct VertexOut {
    @builtin(position) pos: vec4f,
    @location(0) vColor: vec4f,
    @location(1) vPos: vec3f,
    @location(2) vSize: f32,
    @location(3) @interpolate(flat) ambientOcclusion: u32,
}

struct FragmentIn {
    @builtin(position) fragPos: vec4f,
    @location(0) vColor: vec4f,
    @location(1) vPos: vec3f,
    @location(2) vSize: f32,
    @location(3) @interpolate(flat) ambientOcclusion: u32,
}

struct FragmentOut {
    @location(0) color: vec4f,
}

struct Billboard {
    pos: vec2f,
    size: vec2f,
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
    var aabb: vec2f = sqrt(eqCoefs.xy * eqCoefs.xy - eqCoefs.zw);

    return Billboard(outPosition, aabb);
}

fn processVertex(vertex: VertexInputAo) -> VertexOut {
    let vertexPosition: vec2f = vertex.vertexPosition.xy;
    let instanceVoxelPosition: vec4f = unpack4x8unorm(vertex.voxelPosition) * 255;
    var voxelColor: vec4f;
    let chunkOffset: vec3f = vertex.chunkPosition.xyz * CHUNK_SIZE;

    if (vertex.voxelId == 0) {
        voxelColor = vec4f(0.0, 0.0, 0.0, 1.0);
    } else if (vertex.voxelId == 1) {
        voxelColor = vec4f(1.0, 0.0, 0.0, 1.0);
    } else if (vertex.voxelId == 2) {
        voxelColor = vec4f(0.0, 1.0, 0.0, 1.0);
    } else if (vertex.voxelId == 3) {
        voxelColor = vec4f(0.0, 0.0, 1.0, 1.0);
    } else {
        voxelColor = vec4f(1.0, 1.0, 1.0, 1.0);
    }

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
    out.ambientOcclusion = vertex.ambientOcclusion;

    return out;
}

@vertex fn vsMain(vertex: VertexInput) -> VertexOut {
    let vertexAo: VertexInputAo = VertexInputAo(
        vertex.vertexPosition,
        vertex.voxelPosition,
        vertex.voxelId,
        vertex.chunkPosition,
        0
    );
    return processVertex(vertexAo);
}

@vertex fn vsMainAo(vertex: VertexInputAo) -> VertexOut {
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
        if (AO) {
            hit.uv = getXPlaneUv(rayOrigin, rayDirection, distanceToPlane.x, box.radius.x);
            hit.plane = select(0u, 1u, sgn.x > 0.0f);
        }
    } else if (test.y) {
        hit.normal = vec3f(0.0f, sgn.y, 0.0f);
        hit.distance = distanceToPlane.y;
        if (AO) {
            hit.uv = getYPlaneUv(rayOrigin, rayDirection, distanceToPlane.y, box.radius.y);
            hit.plane = select(2u, 3u, sgn.y > 0.0f);
        }
    } else if (test.z) {
        hit.normal = vec3f(0.0f, 0.0f, sgn.z);
        hit.distance = distanceToPlane.z;
        if (AO) {
            hit.uv = getZPlaneUv(rayOrigin, rayDirection, distanceToPlane.z, box.radius.z);
            hit.plane = select(4u, 5u, sgn.z > 0.0f);
        }
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

    return mix(0.25, 1.0, sin(factor * HALF_PI));
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

    const planeNx = 0; // Negative X (Left Plane)
    const planePx = 1; // Positive X (Right Plane)
    const planeNy = 2; // Negative Y (Bottom Plane)
    const planePy = 3; // Positive Y (Top Plane)
    const planeNz = 4; // Negative Z (Front Plane)
    const planePz = 5; // Positive Z (Back Plane)

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
        var color: vec3f = input.vColor.rgb;

        if (AO) {
            color = applyAmbientOcclusion(color, hit.uv, hit.plane, input.ambientOcclusion);
        }

        if (LIGHTING) {
            const lightDir: vec3f = normalize(vec3f(0.2f, 0.8f, -0.5f));
            const ambientLight: f32 = 0.3f;
            let directionalLight: f32 = max(dot(hit.normal, lightDir), 0.0f) * 0.7f;
            let light = ambientLight + directionalLight;
            color *= light;
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