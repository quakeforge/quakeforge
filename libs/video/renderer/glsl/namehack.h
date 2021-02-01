#ifdef NH_DEFINE
#undef NH_DEFINE
#define Draw_Init glsl_Draw_Init
#define Draw_Character glsl_Draw_Character
#define Draw_String glsl_Draw_String
#define Draw_nString glsl_Draw_nString
#define Draw_AltString glsl_Draw_AltString
#define Draw_ConsoleBackground glsl_Draw_ConsoleBackground
#define Draw_Crosshair glsl_Draw_Crosshair
#define Draw_CrosshairAt glsl_Draw_CrosshairAt
#define Draw_TileClear glsl_Draw_TileClear
#define Draw_Fill glsl_Draw_Fill
#define Draw_TextBox glsl_Draw_TextBox
#define Draw_FadeScreen glsl_Draw_FadeScreen
#define Draw_BlendScreen glsl_Draw_BlendScreen
#define Draw_CachePic glsl_Draw_CachePic
#define Draw_UncachePic glsl_Draw_UncachePic
#define Draw_MakePic glsl_Draw_MakePic
#define Draw_DestroyPic glsl_Draw_DestroyPic
#define Draw_PicFromWad glsl_Draw_PicFromWad
#define Draw_Pic glsl_Draw_Pic
#define Draw_Picf glsl_Draw_Picf
#define Draw_SubPic glsl_Draw_SubPic
#define Fog_DisableGFog glsl_Fog_DisableGFog
#define Fog_EnableGFog glsl_Fog_EnableGFog
#define Fog_GetColor glsl_Fog_GetColor
#define Fog_GetDensity glsl_Fog_GetDensity
#define Fog_Init glsl_Fog_Init
#define Fog_ParseWorldspawn glsl_Fog_ParseWorldspawn
#define Fog_SetupFrame glsl_Fog_SetupFrame
#define Fog_StartAdditive glsl_Fog_StartAdditive
#define Fog_StopAdditive glsl_Fog_StopAdditive
#define Fog_Update glsl_Fog_Update
#define R_AddTexture glsl_R_AddTexture
#define R_BlendLightmaps glsl_R_BlendLightmaps
#define R_BuildLightMap glsl_R_BuildLightMap
#define R_CalcLightmaps glsl_R_CalcLightmaps
#define R_ClearParticles glsl_R_ClearParticles
#define R_ClearState glsl_R_ClearState
#define R_ClearTextures glsl_R_ClearTextures
#define R_DrawAliasModel glsl_R_DrawAliasModel
#define R_DrawBrushModel glsl_R_DrawBrushModel
#define R_DrawParticles glsl_R_DrawParticles
#define R_DrawSky glsl_R_DrawSky
#define R_DrawSkyChain glsl_R_DrawSkyChain
#define R_DrawSprite glsl_R_DrawSprite
#define R_DrawSpriteModel glsl_R_DrawSpriteModel
#define R_DrawWaterSurfaces glsl_R_DrawWaterSurfaces
#define R_DrawWorld glsl_R_DrawWorld
#define R_InitBubble glsl_R_InitBubble
#define R_InitGraphTextures glsl_R_InitGraphTextures
#define R_InitParticles glsl_R_InitParticles
#define R_InitSky glsl_R_InitSky
#define R_InitSprites glsl_R_InitSprites
#define R_LineGraph glsl_R_LineGraph
#define R_LoadSky_f glsl_R_LoadSky_f
#define R_LoadSkys glsl_R_LoadSkys
#define R_NewMap glsl_R_NewMap
#define R_Particle_New glsl_R_Particle_New
#define R_Particle_NewRandom glsl_R_Particle_NewRandom
#define R_Particles_Init_Cvars glsl_R_Particles_Init_Cvars
#define R_ReadPointFile_f glsl_R_ReadPointFile_f
#define R_RenderDlights glsl_R_RenderDlights
#define R_RenderView glsl_R_RenderView
#define R_RotateForEntity glsl_R_RotateForEntity
#define R_SetupFrame glsl_R_SetupFrame
#define R_SpriteBegin glsl_R_SpriteBegin
#define R_SpriteEnd glsl_R_SpriteEnd
#define R_TimeRefresh_f glsl_R_TimeRefresh_f
#define R_ViewChanged glsl_R_ViewChanged
#define SCR_CaptureBGR glsl_SCR_CaptureBGR
#define SCR_ScreenShot glsl_SCR_ScreenShot
#define SCR_ScreenShot_f glsl_SCR_ScreenShot_f
#define R_RenderFrame glsl_R_RenderFrame
#define c_alias_polys glsl_c_alias_polys
#define c_brush_polys glsl_c_brush_polys
#define r_easter_eggs_f glsl_r_easter_eggs_f
#define r_particles_style_f glsl_r_particles_style_f
#define r_world_matrix glsl_r_world_matrix
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
#undef R_RenderFrame
#undef c_alias_polys
#undef c_brush_polys
#undef r_easter_eggs_f
#undef r_particles_style_f
#undef r_world_matrix
#endif
