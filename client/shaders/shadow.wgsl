override CHUNK_SIZE: f32 = 64.0;

struct Uniforms {
    projectionViewMatrix: mat4x4<f32>,
    inverseProjectionViewMatrix: mat4x4<f32>,
    cameraPosition: vec3f,
    fov: f32,
    viewportSize: vec2f,
    nearPlane: f32,
    farPlane: f32,
    cameraDir: vec3f,
    time: f32,
};

@group(0) @binding(0) var<uniform> u: Uniforms;

struct VertexInput {
    @location(0) vertexPosition: vec2f,
    @location(1) voxelPosition: u32,
    @location(2) voxelData: u32,
    @location(3) chunkPosition: vec3f
};

struct VertexOut {
    @builtin(position) pos: vec4f,
    @location(0) vPos: vec3f,
    @location(1) vSize: f32
}

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

    //u.projectionViewMatrix
    let p = u.projectionViewMatrix * vec4f(voxelPosition, 1.0);
    let depth = clamp(p.z / p.w, 0.0, 1.0);

    return vec4f(billboardNdc, depth, 1.0);
}

@vertex fn vsShadow(vertex: VertexInput) -> VertexOut {
    let vertexPosition: vec2f = vertex.vertexPosition.xy;
    let instanceVoxelPosition: vec4f = unpack4x8unorm(vertex.voxelPosition) * 255;
    let chunkOffset: vec3f = vertex.chunkPosition.xyz * CHUNK_SIZE;

    let voxelSize = instanceVoxelPosition.w;
    let voxelPosition = instanceVoxelPosition.xyz - u.cameraPosition + chunkOffset + vec3f(0.5 * voxelSize);

    var out: VertexOut;
    out.pos = calculateBillboard(voxelPosition, voxelSize, vertexPosition);
    out.vPos = voxelPosition;
    out.vSize = voxelSize;
    return out;
}

struct FragmentIn {
    @builtin(position) fragPos: vec4f,
    @location(0) vPos: vec3f,
    @location(1) vSize: f32,
}

struct FragmentOut {
    @builtin(frag_depth) depth: f32,
};

struct Box {
    center: vec3f,
    radius: vec3f,
}

struct Ray {
    origin: vec3f,
    inverseDirection: vec3f,
}

struct Hit {
    isHit: bool,
    distance: f32,
}

fn intersectBox(box: Box, ray: Ray) -> Hit {
    let minB = box.center - box.radius;
    let maxB = box.center + box.radius;

    let t0s = (minB - ray.origin) * ray.inverseDirection;
    let t1s = (maxB - ray.origin) * ray.inverseDirection;

    let tsmaller = min(t0s, t1s);
    let tbigger = max(t0s, t1s);

    let tmin = max(max(tsmaller.x, tsmaller.y), tsmaller.z);
    let tmax = min(min(tbigger.x, tbigger.y), tbigger.z);

    var hit: Hit;
    hit.isHit = (tmax >= max(tmin, 0.0));
    // using value in the middle of the box for shadow mapping to reduce shadow acne
    hit.distance = (tmin + tmax) * 0.5;
    return hit;
}

fn computeRay(input: FragmentIn) -> Ray {
    let ndc = (input.fragPos.xy / u.viewportSize.xy) * 2.0 - 1.0;
    let worldPos4 = u.inverseProjectionViewMatrix * vec4f(ndc.x, -ndc.y, 0.0, 1.0);
    let worldPos = worldPos4.xyz / worldPos4.w;
    let rayOrigin = worldPos;
    let inverseDirection = 1 / normalize(u.cameraDir);

    var ray: Ray;
    ray.origin = rayOrigin;
    ray.inverseDirection = inverseDirection;
    return ray;
}

fn getBox(input: FragmentIn) -> Box {
    var box: Box;
    box.center = input.vPos;
    box.radius = vec3f(input.vSize * 0.5);
    return box;
}

@fragment fn fsShadow(input: FragmentIn) -> FragmentOut {
    let ray = computeRay(input);
    let box = getBox(input);
    let hit = intersectBox(box, ray);

    var depth: f32 = 1.0;
    if (hit.isHit) {
        depth = hit.distance / (u.farPlane - u.nearPlane);
    }

    return FragmentOut(depth);
}
