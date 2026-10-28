#pragma once

// Shared by the runtime pass and its offline shader compilation check.
namespace motogp::nobk::backdrop {
inline constexpr char shader[] = R"hlsl(
Texture2D<float4> source : register(t0);
SamplerState linear_clamp : register(s0);
cbuffer Parameters : register(b0) {
  float2 inverse_output_size;
  float2 sample_step;
  float2 feather;
  float strength;
  float composite;
};
float4 Vertex(uint id : SV_VertexID) : SV_Position {
  float2 uv = float2((id << 1) & 2, id & 2);
  return float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
}
float4 Pixel(float4 position : SV_Position) : SV_Target {
  float2 uv = position.xy * inverse_output_size;
  float3 color = source.SampleLevel(linear_clamp, uv, 0).rgb * 0.2270270270;
  color += source.SampleLevel(linear_clamp, uv + sample_step * 1.3846153846, 0).rgb * 0.3162162162;
  color += source.SampleLevel(linear_clamp, uv - sample_step * 1.3846153846, 0).rgb * 0.3162162162;
  color += source.SampleLevel(linear_clamp, uv + sample_step * 3.2307692308, 0).rgb * 0.0702702703;
  color += source.SampleLevel(linear_clamp, uv - sample_step * 3.2307692308, 0).rgb * 0.0702702703;
  float alpha = composite > 0.5 ? strength * (1 - smoothstep(feather.x, feather.y, uv.x)) : 1;
  return float4(color, alpha);
}
)hlsl";
}
