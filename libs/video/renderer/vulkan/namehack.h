#ifdef NH_DEFINE
#undef NH_DEFINE
#define Draw_Init vulkan_Draw_Init
#define Draw_Character vulkan_Draw_Character
#define Draw_String vulkan_Draw_String
#define Draw_nString vulkan_Draw_nString
#define Draw_AltString vulkan_Draw_AltString
#define Draw_ConsoleBackground vulkan_Draw_ConsoleBackground
#define Draw_Crosshair vulkan_Draw_Crosshair
#define Draw_CrosshairAt vulkan_Draw_CrosshairAt
#define Draw_TileClear vulkan_Draw_TileClear
#define Draw_Fill vulkan_Draw_Fill
#define Draw_TextBox vulkan_Draw_TextBox
#define Draw_FadeScreen vulkan_Draw_FadeScreen
#define Draw_BlendScreen vulkan_Draw_BlendScreen
#define Draw_CachePic vulkan_Draw_CachePic
#define Draw_UncachePic vulkan_Draw_UncachePic
#define Draw_MakePic vulkan_Draw_MakePic
#define Draw_DestroyPic vulkan_Draw_DestroyPic
#define Draw_PicFromWad vulkan_Draw_PicFromWad
#define Draw_Pic vulkan_Draw_Pic
#define Draw_Picf vulkan_Draw_Picf
#define Draw_SubPic vulkan_Draw_SubPic
#define Fog_DisableGFog vulkan_Fog_DisableGFog
#define Fog_EnableGFog vulkan_Fog_EnableGFog
#define Fog_GetColor vulkan_Fog_GetColor
#define Fog_GetDensity vulkan_Fog_GetDensity
#define Fog_Init vulkan_Fog_Init
#define Fog_ParseWorldspawn vulkan_Fog_ParseWorldspawn
#define Fog_SetupFrame vulkan_Fog_SetupFrame
#define Fog_StartAdditive vulkan_Fog_StartAdditive
#define Fog_StopAdditive vulkan_Fog_StopAdditive
#define Fog_Update vulkan_Fog_Update
#define R_AddTexture vulkan_R_AddTexture
#define R_BlendLightmaps vulkan_R_BlendLightmaps
#define R_BuildLightMap vulkan_R_BuildLightMap
#define R_CalcLightmaps vulkan_R_CalcLightmaps
#define R_ClearParticles vulkan_R_ClearParticles
#define R_ClearState vulkan_R_ClearState
#define R_ClearTextures vulkan_R_ClearTextures
#define R_DrawAliasModel vulkan_R_DrawAliasModel
#define R_DrawBrushModel vulkan_R_DrawBrushModel
#define R_DrawParticles vulkan_R_DrawParticles
#define R_DrawSky vulkan_R_DrawSky
#define R_DrawSkyChain vulkan_R_DrawSkyChain
#define R_DrawSprite vulkan_R_DrawSprite
#define R_DrawSpriteModel vulkan_R_DrawSpriteModel
#define R_DrawWaterSurfaces vulkan_R_DrawWaterSurfaces
#define R_DrawWorld vulkan_R_DrawWorld
#define R_InitBubble vulkan_R_InitBubble
#define R_InitGraphTextures vulkan_R_InitGraphTextures
#define R_InitParticles vulkan_R_InitParticles
#define R_InitSky vulkan_R_InitSky
#define R_InitSprites vulkan_R_InitSprites
#define R_InitSurfaceChains vulkan_R_InitSurfaceChains
#define R_LineGraph vulkan_R_LineGraph
#define R_LoadSky_f vulkan_R_LoadSky_f
#define R_LoadSkys vulkan_R_LoadSkys
#define R_NewMap vulkan_R_NewMap
#define R_Particle_New vulkan_R_Particle_New
#define R_Particle_NewRandom vulkan_R_Particle_NewRandom
#define R_Particles_Init_Cvars vulkan_R_Particles_Init_Cvars
#define R_ReadPointFile_f vulkan_R_ReadPointFile_f
#define R_RenderDlights vulkan_R_RenderDlights
#define R_RenderView vulkan_R_RenderView
#define R_RotateForEntity vulkan_R_RotateForEntity
#define R_SetupFrame vulkan_R_SetupFrame
#define R_SpriteBegin vulkan_R_SpriteBegin
#define R_SpriteEnd vulkan_R_SpriteEnd
#define R_TimeRefresh_f vulkan_R_TimeRefresh_f
#define R_ViewChanged vulkan_R_ViewChanged
#define SCR_CaptureBGR vulkan_SCR_CaptureBGR
#define SCR_ScreenShot vulkan_SCR_ScreenShot
#define SCR_ScreenShot_f vulkan_SCR_ScreenShot_f
#define c_alias_polys vulkan_c_alias_polys
#define c_brush_polys vulkan_c_brush_polys
#define r_easter_eggs_f vulkan_r_easter_eggs_f
#define r_particles_style_f vulkan_r_particles_style_f
#define r_world_matrix vulkan_r_world_matrix
#else
#undef Fog_DisableGFog
#undef Fog_EnableGFog
#undef Fog_GetColor
#undef Fog_GetDensity
#undef Fog_Init
#undef Fog_ParseWorldspawn
#undef Fog_SetupFrame
#undef Fog_StartAdditive
#undef Fog_StopAdditive
#undef Fog_Update
#undef R_AddTexture
#undef R_BlendLightmaps
#undef R_BuildLightMap
#undef R_CalcLightmaps
#undef R_ClearParticles
#undef R_ClearState
#undef R_ClearTextures
#undef R_DrawAliasModel
#undef R_DrawBrushModel
#undef R_DrawParticles
#undef R_DrawSky
#undef R_DrawSkyChain
#undef R_DrawSpriteModel
#undef R_DrawWaterSurfaces
#undef R_DrawWorld
#undef R_Init
#undef R_InitBubble
#undef R_InitGraphTextures
#undef R_InitParticles
#undef R_InitSky
#undef R_InitSprites
#undef R_InitSurfaceChains
#undef R_LineGraph
#undef R_LoadSky_f
#undef R_LoadSkys
#undef R_NewMap
#undef R_Particle_New
#undef R_Particle_NewRandom
#undef R_Particles_Init_Cvars
#undef R_ReadPointFile_f
#undef R_RenderDlights
#undef R_RenderView
#undef R_RotateForEntity
#undef R_SetupFrame
#undef R_TimeRefresh_f
#undef R_ViewChanged
#undef SCR_CaptureBGR
#undef SCR_ScreenShot
#undef SCR_ScreenShot_f
#undef c_alias_polys
#undef c_brush_polys
#undef r_easter_eggs_f
#undef r_particles_style_f
#undef r_world_matrix
#endif
