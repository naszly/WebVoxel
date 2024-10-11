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
    @location(0) vertex_position: vec3f,
    @location(1) instance_position: u32,
    @location(2) instance_color: u32,
}

struct VertexOut {
    @builtin(position) pos: vec4f,
    @location(0) vColor: vec4f,
    @location(1) vPos: vec3f,
}

struct FragmentIn {
    @builtin(position) fragPos: vec4f,
    @location(0) vColor: vec4f,
    @location(1) vPos: vec3f,
}

struct FragmentOut {
    @location(0) color: vec4f,
    @builtin(frag_depth) depth: f32,
}

struct Billboard {
    pos : vec4f,
    size : f32,
}

@vertex fn vs_main(vertex : VertexInput) -> VertexOut {
    var billboardPos : vec2f = vertex.vertex_position.xy;
    var instancePos: vec3f = unpack4x8unorm(vertex.instance_position).xyz * 255;
    var instanceColor: vec4f = unpack4x8unorm(vertex.instance_color);

    var viewDir : vec3f = normalize(-(instancePos - u.cameraPosition));
    var right : vec3f = normalize(cross(vec3f(0.0f, 1.0f, 0.0f), viewDir));
    var up : vec3f = normalize(cross(viewDir, right));

    let s : f32 = 1.7f;

    var pos : vec3f = ((instancePos - u.cameraPosition) + billboardPos.x * s * right + billboardPos.y * s * up);

    var out: VertexOut;
    out.pos = u.projectionView * vec4f(pos, 1.0f);
    out.vColor = instanceColor;
    out.vPos = (instancePos - u.cameraPosition);
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
    let lightDir : vec3f = normalize(vec3f(0.2f, 0.8f, -0.5f));
    var ray : Ray;
    ray.origin = vec3f(0.0, 0.0, 0.0);
    ray.direction = normalize(screenToWorldSpace(in.fragPos.xy / u.viewportSize.xy));

    var box : Box;
    box.center = in.vPos;
    box.radius = vec3f(0.5f, 0.5f, 0.5f);
    box.rotation = mat3x3<f32>(1.0f, 0.0f, 0.0f,
                               0.0f, 1.0f, 0.0f,
                               0.0f, 0.0f, 1.0f);

    let hit : Hit = intersectBox(box, ray);

    var out : FragmentOut;

    if (hit.isHit) {
        let light : f32 = 1.0;//max(dot(hit.normal, lightDir), 0.0f);
        out.color = in.vColor * light;
        out.color.a = 1.0f;
        out.depth = hit.distance / (u.farPlane - u.nearPlane);
        return out;
    } else {
        discard;
        /*out.color = vec4f(0.0f, 0.0f, 0.0f, 0.0f);
        out.depth = 0.5f;
        return out;*/
    }
}
