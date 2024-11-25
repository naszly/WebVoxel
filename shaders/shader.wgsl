override CHUNK_SIZE: f32 = 64.0;

struct Uniforms {
    projectionView : mat4x4<f32>,
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

struct VertexOut {
    @builtin(position) pos: vec4f,
    @location(0) vColor: vec4f,
    @location(1) vPos: vec3f,
    @location(2) vSize: f32,
}

struct FragmentIn {
    @builtin(position) fragPos: vec4f,
    @location(0) vColor: vec4f,
    @location(1) vPos: vec3f,
    @location(2) vSize: f32,
}

struct FragmentOut {
    @location(0) color: vec4f,
}

struct Billboard {
    pos : vec2f,
    size : vec2f,
}

fn quadricProj(voxelPosition: vec3f, voxelSize: f32, objectToScreenMatrix: mat4x4<f32>) -> Billboard {
    let quadricMat: vec4f = vec4f(1.0, 1.0, 1.0, -1.0);
    let sphereRadius: f32 = voxelSize * 0.5 * 1.732051;
    let sphereCenter: vec4f = vec4f(voxelPosition, 1.0);
    let modelViewProj: mat4x4<f32> = transpose(objectToScreenMatrix);

    var matT: mat3x4<f32> = mat3x4<f32>(
        mat3x4<f32>(modelViewProj[0], modelViewProj[1], modelViewProj[3]) * sphereRadius
    );
    matT[0].w = dot(sphereCenter, modelViewProj[0]);
    matT[1].w = dot(sphereCenter, modelViewProj[1]);
    matT[2].w = dot(sphereCenter, modelViewProj[3]);

    let matD: mat3x4<f32> = mat3x4<f32>(
        matT[0] * quadricMat,
        matT[1] * quadricMat,
        matT[2] * quadricMat
    );

    if (dot(matD[2], matT[2]) > 0.0) {
        return Billboard(vec2f(0.0, 0.0), vec2f(0.0, 0.0));
    }

    let eqCoefs: vec4f = vec4f(
        dot(matD[0], matT[2]),
        dot(matD[1], matT[2]),
        dot(matD[0], matT[0]),
        dot(matD[1], matT[1])
    ) / dot(matD[2], matT[2]);

    let outPosition: vec2f = eqCoefs.xy;
    var AABB: vec2f = sqrt(eqCoefs.xy * eqCoefs.xy - eqCoefs.zw);

    return Billboard(outPosition, AABB);
}

@vertex fn vs_main(vertex: VertexInput) -> VertexOut {
    var vertexPosition: vec2f = vertex.vertex_position.xy;
    var instanceVoxelPosition: vec4f = unpack4x8unorm(vertex.voxel_position) * 255;
    var voxelColor: vec4f = unpack4x8unorm(vertex.voxel_color);
    var chunkOffset: vec3f = vertex.chunk_position.xyz * CHUNK_SIZE;

    var voxelSize = instanceVoxelPosition.w;

    var voxelPosition = instanceVoxelPosition.xyz - u.cameraPosition + chunkOffset + vec3f(0.5 * voxelSize);

    var billboard: Billboard = quadricProj(voxelPosition, voxelSize, u.projectionView);

    var stochasticCoverage: f32 = billboard.size.x * u.viewportSize.x * billboard.size.y * u.viewportSize.y;
    if (stochasticCoverage < 0.8) {
        billboard.size = vec2f(0.0, 0.0);
    }

    vertexPosition *= billboard.size;
    vertexPosition += billboard.pos;

    var transformedPosition: vec4f = u.projectionView * vec4f(voxelPosition, 1.0);
    var vertexPositionZ: f32 = transformedPosition.z / transformedPosition.w;

    if (vertexPositionZ > 0.0 && vertexPositionZ < 1.0) {
        vertexPositionZ = length(voxelPosition) / u.farPlane;
    }

    var out: VertexOut;
    out.pos = vec4f(vertexPosition, vertexPositionZ, 1.0);
    out.vColor = voxelColor;
    out.vPos = voxelPosition;
    out.vSize = voxelSize;

    return out;
}

struct Box {
    center : vec3f,
    radius : vec3f,
    invRadius : vec3f,
    rotation : mat3x3<f32>,
}

struct Ray {
    origin : vec3f,
    direction : vec3f,
}

struct Hit {
    isHit : bool,
    distance : f32,
    normal : vec3f,
}

fn maxComponent(v : vec3f) -> f32 {
    return max(max(v.x, v.y), v.z);
}

fn allLessThan(v1 : vec2f, v2 : vec2f) -> bool {
    return v1.x < v2.x && v1.y < v2.y;
}

fn rayBoxTest(distanceToPlane : vec3f, rayOrigin : vec3f, rayDirection : vec3f, boxRadius : vec3f) -> vec3<bool> {
    var xyz : bool = distanceToPlane.x > 0.0f &&
        allLessThan(abs(rayOrigin.yz + rayDirection.yz * distanceToPlane.x), boxRadius.yz);

    var yzx : bool = distanceToPlane.y > 0.0f &&
        allLessThan(abs(rayOrigin.zx + rayDirection.zx * distanceToPlane.y), boxRadius.zx);

    var zxy : bool = distanceToPlane.z > 0.0f &&
        allLessThan(abs(rayOrigin.xy + rayDirection.xy * distanceToPlane.z), boxRadius.xy);

    return vec3<bool>(xyz, yzx, zxy);
}

fn intersectBox(box : Box, ray : Ray) -> Hit {
    let rayOrigin : vec3f = box.rotation * (ray.origin - box.center);
    let rayDirection : vec3f = ray.direction * box.rotation;

    var sgn : vec3f = -sign(rayDirection);

    var distanceToPlane : vec3f = (box.radius * sgn - rayOrigin) / rayDirection;

    var test : vec3<bool> = rayBoxTest(distanceToPlane, rayOrigin, rayDirection, box.radius);

    var hit : Hit;
    hit.isHit = false;
    hit.distance = 0.0f;
    hit.normal = vec3f(0.0f, 0.0f, 0.0f);

    if (test.x) {
        sgn = vec3f(sgn.x, 0.0f, 0.0f);
        hit.distance = distanceToPlane.x;
    } else if (test.y) {
        sgn = vec3f(0.0f, sgn.y, 0.0f);
        hit.distance = distanceToPlane.y;
    } else if (test.z) {
        sgn = vec3f(0.0f, 0.0f, sgn.z);
        hit.distance = distanceToPlane.z;
    } else {
        return hit;
    }

    hit.isHit = true;
    hit.normal = box.rotation * sgn;

    return hit;
}

fn screenToWorldSpace(screenPos : vec2f) -> vec3f {
    var pos = screenPos * 2.0f - 1.0f;
    var worldPos : vec4f = u.inverseProjectionViewMatrix * vec4f(pos.x, -pos.y, -1.0f, 1.0f);
    return worldPos.xyz / worldPos.w;
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
    box.rotation = mat3x3<f32>(1.0f, 0.0f, 0.0f,
                               0.0f, 1.0f, 0.0f,
                               0.0f, 0.0f, 1.0f);

    let hit : Hit = intersectBox(box, ray);

    if (hit.isHit) {
        let light : f32 = max(dot(hit.normal, lightDir), 0.0f);
        out.color = in.vColor * light + in.vColor * 0.1f;
        out.color.a = 1.0f;
    } else {
        discard;
    }

    return out;
}
