#include "../../source/client/renderer/shader_shared.h"

[[vk::binding( 0, DescriptorSet_Material )]] StructuredBuffer< MaterialStaticUniforms > u_MaterialProperties;
[[vk::binding( 1, DescriptorSet_Material )]] Texture2D< float4 > u_Texture;
[[vk::binding( 2, DescriptorSet_Material )]] SamplerState u_Sampler;
