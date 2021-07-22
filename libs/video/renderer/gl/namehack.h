#ifdef NH_DEFINE
#undef NH_DEFINE
#define Draw_Init gl_Draw_Init
#define Draw_Character gl_Draw_Character
#define Draw_String gl_Draw_String
#define Draw_nString gl_Draw_nString
#define Draw_AltString gl_Draw_AltString
#define Draw_ConsoleBackground gl_Draw_ConsoleBackground
#define Draw_Crosshair gl_Draw_Crosshair
#define Draw_CrosshairAt gl_Draw_CrosshairAt
#define Draw_TileClear gl_Draw_TileClear
#define Draw_Fill gl_Draw_Fill
#define Draw_TextBox gl_Draw_TextBox
#define Draw_FadeScreen gl_Draw_FadeScreen
#define Draw_BlendScreen gl_Draw_BlendScreen
#define Draw_CachePic gl_Draw_CachePic
#define Draw_UncachePic gl_Draw_UncachePic
#define Draw_MakePic gl_Draw_MakePic
#define Draw_DestroyPic gl_Draw_DestroyPic
#define Draw_PicFromWad gl_Draw_PicFromWad
#define Draw_Pic gl_Draw_Pic
#define Draw_Picf gl_Draw_Picf
#define Draw_SubPic gl_Draw_SubPic
#define Fog_DisableGFog gl_Fog_DisableGFog
#define Fog_EnableGFog gl_Fog_EnableGFog
#define Fog_GetColor gl_Fog_GetColor
#define Fog_GetDensity gl_Fog_GetDensity
#define Fog_Init gl_Fog_Init
#define Fog_ParseWorldspawn gl_Fog_ParseWorldspawn
#define Fog_SetupFrame gl_Fog_SetupFrame
#define Fog_StartAdditive gl_Fog_StartAdditive
#define Fog_StopAdditive gl_Fog_StopAdditive
#define Fog_Update gl_Fog_Update
#define R_AddTexture gl_R_AddTexture
#define R_BlendLightmaps gl_R_BlendLightmaps
#define R_CalcLightmaps gl_R_CalcLightmaps
#define R_ClearParticles gl_R_ClearParticles
#define R_ClearState gl_R_ClearState
#define R_ClearTextures gl_R_ClearTextures
#define R_DrawAliasModel gl_R_DrawAliasModel
#define R_DrawBrushModel gl_R_DrawBrushModel
#define R_DrawParticles gl_R_DrawParticles
#define R_DrawSky gl_R_DrawSky
#define R_DrawSkyChain gl_R_DrawSkyChain
#define R_DrawSpriteModel gl_R_DrawSpriteModel
#define R_DrawWaterSurfaces gl_R_DrawWaterSurfaces
#define R_DrawWorld gl_R_DrawWorld
#define R_InitBubble gl_R_InitBubble
#define R_InitGraphTextures gl_R_InitGraphTextures
#define R_InitParticles gl_R_InitParticles
#define R_InitSky gl_R_InitSky
#define R_InitSprites gl_R_InitSprites
#define R_InitSurfaceChains gl_R_InitSurfaceChains
#define R_LineGraph gl_R_LineGraph
#define R_LoadSky_f gl_R_LoadSky_f
#define R_LoadSkys gl_R_LoadSkys
#define R_NewMap gl_R_NewMap
#define R_Particle_New gl_R_Particle_New
#define R_Particle_NewRandom gl_R_Particle_NewRandom
#define R_Particles_Init_Cvars gl_R_Particles_Init_Cvars
#define R_ReadPointFile_f gl_R_ReadPointFile_f
#define R_RenderDlights gl_R_RenderDlights
#define R_RenderView gl_R_RenderView
#define R_RotateForEntity gl_R_RotateForEntity
#define R_SetupFrame gl_R_SetupFrame
#define R_TimeRefresh_f gl_R_TimeRefresh_f
#define R_ViewChanged gl_R_ViewChanged
#define SCR_CaptureBGR gl_SCR_CaptureBGR
#define SCR_ScreenShot gl_SCR_ScreenShot
#define SCR_ScreenShot_f gl_SCR_ScreenShot_f
#define R_RenderFrame gl_R_RenderFrame
#define c_alias_polys gl_c_alias_polys
#define c_brush_polys gl_c_brush_polys
#define r_easter_eggs_f gl_r_easter_eggs_f
#define r_particles_style_f gl_r_particles_style_f
#define r_world_matrix gl_r_world_matrix
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
#undef R_RenderFrame
#undef c_alias_polys
#undef c_brush_polys
#undef r_easter_eggs_f
#undef r_particles_style_f
#undef r_world_matrix
#endif
