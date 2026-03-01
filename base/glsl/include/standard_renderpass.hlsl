#pragma once

#include "../../source/client/renderer/shader_shared.h"

[[vk::binding( 0, DescriptorSet_RenderPass )]] StructuredBuffer< ViewUniforms > u_View;
[[vk::binding( 1, DescriptorSet_RenderPass )]] Texture2D< float4 > u_BlueNoise;
[[vk::binding( 2, DescriptorSet_RenderPass )]] SamplerState u_StandardSampler;
[[vk::binding( 3, DescriptorSet_RenderPass )]] StructuredBuffer< ShadowmapUniforms > u_Shadowmap;
[[vk::binding( 4, DescriptorSet_RenderPass )]] Texture2DArray< float > u_ShadowmapTextureArray;
[[vk::binding( 5, DescriptorSet_RenderPass )]] SamplerComparisonState u_ShadowmapSampler;
[[vk::binding( 6, DescriptorSet_RenderPass )]] StructuredBuffer< TileCountsUniforms > u_TileCounts;
[[vk::binding( 7, DescriptorSet_RenderPass )]] StructuredBuffer< TileIndices > u_DecalTiles;
[[vk::binding( 8, DescriptorSet_RenderPass )]] StructuredBuffer< TileIndices > u_LightTiles;
[[vk::binding( 9, DescriptorSet_RenderPass )]] StructuredBuffer< Decal > u_Decals;
[[vk::binding( 10, DescriptorSet_RenderPass )]] StructuredBuffer< Light > u_Lights;
[[vk::binding( 11, DescriptorSet_RenderPass )]] Texture2DArray< float > u_SpriteAtlas;
