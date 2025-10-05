struct VertexOutput {
    @builtin(position) position : vec4<f32>,
    @location(0) uv : vec2<f32>,
};

@vertex fn vs_main(@location(0) pos: vec2<f32>) -> VertexOutput {
    var out: VertexOutput;
    out.position = vec4<f32>(pos, 0.0, 1.0);
    out.uv = vec2<f32>(pos.x * 0.5 + 0.5, -pos.y * 0.5 + 0.5);
    return out;
}

@group(0) @binding(0) var sceneTex: texture_2d<f32>;
@group(0) @binding(1) var sceneSampler: sampler;
@group(0) @binding(2) var<uniform> resolution: vec2<f32>;

const DIR_STRENGTH : f32 = 0.8;

fn luma(rgb: vec3<f32>) -> f32 {
    return dot(rgb, vec3<f32>(0.2126, 0.7152, 0.0722));
}

fn sampleColor(uv: vec2<f32>) -> vec3<f32> {
    return textureSample(sceneTex, sceneSampler, clamp(uv, vec2<f32>(0.0), vec2<f32>(1.0))).rgb;
}

@fragment fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    let invRes = 1.0 / resolution;
    let dx = vec2(invRes.x, 0.0);
    let dy = vec2(0.0, invRes.y);

    // Center + 4-neighbor samples
    let rgbC = sampleColor(in.uv);
    let lC = luma(rgbC);
    let lN = luma(sampleColor(in.uv + dy));
    let lS = luma(sampleColor(in.uv - dy));
    let lE = luma(sampleColor(in.uv + dx));
    let lW = luma(sampleColor(in.uv - dx));

    let lMin = min(lC, min(min(lN, lS), min(lE, lW)));
    let lMax = max(lC, max(max(lN, lS), max(lE, lW)));
    let range = lMax - lMin;

    // Gradient (edge normal approx)
    var dir = vec2(lW - lE, lN - lS);
    let mag = max(abs(dir.x), abs(dir.y)) + 1e-6;
    dir /= mag;

    // First pass directional smoothing
    let edgeDir = vec2(-dir.y, dir.x) * DIR_STRENGTH;
    let offset = edgeDir * invRes;
    let rgbA = sampleColor(in.uv + offset);
    let rgbB = sampleColor(in.uv - offset);
    var result = (rgbA + rgbB + 2.0 * rgbC) * 0.25;

    return vec4(result, 1.0);
}