#ifdef NH_DEFINE
#undef NH_DEFINE
#define D_CacheSurface sw32_D_CacheSurface
#define D_DrawParticle sw32_D_DrawParticle
#define D_DrawPoly sw32_D_DrawPoly
#define D_DrawSkyScans sw32_D_DrawSkyScans
#define D_DrawSpans sw32_D_DrawSpans
#define D_DrawSprite sw32_D_DrawSprite
#define D_DrawSurfaces sw32_D_DrawSurfaces
#define D_DrawZPoint sw32_D_DrawZPoint
#define D_DrawZSpans sw32_D_DrawZSpans
#define D_FillRect sw32_D_FillRect
#define D_FlushCaches sw32_D_FlushCaches
#define D_Init sw32_D_Init
#define D_InitCaches sw32_D_InitCaches
#define D_MipLevelForScale sw32_D_MipLevelForScale
#define D_PolysetCalcGradients sw32_D_PolysetCalcGradients
#define D_PolysetDraw sw32_D_PolysetDraw
#define D_PolysetScanLeftEdge sw32_D_PolysetScanLeftEdge
#define D_PolysetSetEdgeTable sw32_D_PolysetSetEdgeTable
#define D_RasterizeAliasPolySmooth sw32_D_RasterizeAliasPolySmooth
#define D_SetupFrame sw32_D_SetupFrame
#define D_SpriteDrawSpans sw32_D_SpriteDrawSpans
#define D_SurfaceCacheAddress sw32_D_SurfaceCacheAddress
#define D_SurfaceCacheForRes sw32_D_SurfaceCacheForRes
#define D_TurnZOn sw32_D_TurnZOn
#define D_UpdateRects sw32_D_UpdateRects
#define D_ViewChanged sw32_D_ViewChanged
#define D_WarpScreen sw32_D_WarpScreen
#define Draw_AltString sw32_Draw_AltString
#define Draw_BlendScreen sw32_Draw_BlendScreen
#define Draw_CachePic sw32_Draw_CachePic
#define Draw_Character sw32_Draw_Character
#define Draw_ConsoleBackground sw32_Draw_ConsoleBackground
#define Draw_Crosshair sw32_Draw_Crosshair
#define Draw_CrosshairAt sw32_Draw_CrosshairAt
#define Draw_DestroyPic sw32_Draw_DestroyPic
#define Draw_FadeScreen sw32_Draw_FadeScreen
#define Draw_Fill sw32_Draw_Fill
#define Draw_Init sw32_Draw_Init
#define Draw_MakePic sw32_Draw_MakePic
#define Draw_Pic sw32_Draw_Pic
#define Draw_PicFromWad sw32_Draw_PicFromWad
#define Draw_Picf sw32_Draw_Picf
#define Draw_String sw32_Draw_String
#define Draw_SubPic sw32_Draw_SubPic
#define Draw_TextBox sw32_Draw_TextBox
#define Draw_TileClear sw32_Draw_TileClear
#define Draw_UncachePic sw32_Draw_UncachePic
#define Draw_nString sw32_Draw_nString
#define R_AliasCheckBBox sw32_R_AliasCheckBBox
#define R_AliasClipTriangle sw32_R_AliasClipTriangle
#define R_AliasClipAndProjectFinalVert sw32_R_AliasClipAndProjectFinalVert
#define R_AliasDrawModel sw32_R_AliasDrawModel
#define R_AliasProjectFinalVert sw32_R_AliasProjectFinalVert
#define R_AliasSetUpTransform sw32_R_AliasSetUpTransform
#define R_AliasTransformAndProjectFinalVerts sw32_R_AliasTransformAndProjectFinalVerts
#define R_AliasTransformFinalVert sw32_R_AliasTransformFinalVert
#define R_AliasTransformVector sw32_R_AliasTransformVector
#define R_Alias_clip_bottom sw32_R_Alias_clip_bottom
#define R_Alias_clip_left sw32_R_Alias_clip_left
#define R_Alias_clip_right sw32_R_Alias_clip_right
#define R_Alias_clip_top sw32_R_Alias_clip_top
#define R_IQMDrawModel sw32_R_IQMDrawModel
#define R_BeginEdgeFrame sw32_R_BeginEdgeFrame
#define R_ClearState sw32_R_ClearState
#define R_ClipEdge sw32_R_ClipEdge
#define R_DrawParticles sw32_R_DrawParticles
#define R_DrawSolidClippedSubmodelPolygons sw32_R_DrawSolidClippedSubmodelPolygons
#define R_DrawSprite sw32_R_DrawSprite
#define R_DrawSubmodelPolygons sw32_R_DrawSubmodelPolygons
#define R_DrawSurface sw32_R_DrawSurface
#define R_EmitEdge sw32_R_EmitEdge
#define R_GenerateSpans sw32_R_GenerateSpans
#define R_InitParticles sw32_R_InitParticles
#define R_InitSky sw32_R_InitSky
#define R_InitTurb sw32_R_InitTurb
#define R_InsertNewEdges sw32_R_InsertNewEdges
#define R_LineGraph sw32_R_LineGraph
#define R_LoadSky_f sw32_R_LoadSky_f
#define R_LoadSkys sw32_R_LoadSkys
#define R_MakeSky sw32_R_MakeSky
#define R_NewMap sw32_R_NewMap
#define R_Particle_New sw32_R_Particle_New
#define R_Particle_NewRandom sw32_R_Particle_NewRandom
#define R_Particles_Init_Cvars sw32_R_Particles_Init_Cvars
#define R_PrintAliasStats sw32_R_PrintAliasStats
#define R_PrintTimes sw32_R_PrintTimes
#define R_ReadPointFile_f sw32_R_ReadPointFile_f
#define R_RemoveEdges sw32_R_RemoveEdges
#define R_RenderBmodelFace sw32_R_RenderBmodelFace
#define R_RenderFace sw32_R_RenderFace
#define R_RenderPoly sw32_R_RenderPoly
#define R_RenderView sw32_R_RenderView
#define R_RenderWorld sw32_R_RenderWorld
#define R_RotateBmodel sw32_R_RotateBmodel
#define R_ScanEdges sw32_R_ScanEdges
#define R_SetSkyFrame sw32_R_SetSkyFrame
#define R_SetupFrame sw32_R_SetupFrame
#define R_StepActiveU sw32_R_StepActiveU
#define R_Textures_Init sw32_R_Textures_Init
#define R_TimeRefresh_f sw32_R_TimeRefresh_f
#define R_TransformFrustum sw32_R_TransformFrustum
#define R_TransformPlane sw32_R_TransformPlane
#define R_ViewChanged sw32_R_ViewChanged
#define R_ZDrawSubmodelPolys sw32_R_ZDrawSubmodelPolys
#define SCR_CaptureBGR sw32_SCR_CaptureBGR
#define SCR_ScreenShot sw32_SCR_ScreenShot
#define SCR_ScreenShot_f sw32_SCR_ScreenShot_f
#define R_RenderFrame sw32_R_RenderFrame
#define TransformVector sw32_TransformVector
#define Turbulent sw32_Turbulent
#define acolormap sw32_acolormap
#define aliastransform sw32_aliastransform
#define aliasxcenter sw32_aliasxcenter
#define aliasxscale sw32_aliasxscale
#define aliasycenter sw32_aliasycenter
#define aliasyscale sw32_aliasyscale
#define auxedges sw32_auxedges
#define bbextents sw32_bbextents
#define bbextentt sw32_bbextentt
#define c_faceclip sw32_c_faceclip
#define c_surf sw32_c_surf
#define cacheblock sw32_cacheblock
#define cachewidth sw32_cachewidth
#define d_initial_rover sw32_d_initial_rover
#define d_minmip sw32_d_minmip
#define d_pix_max sw32_d_pix_max
#define d_pix_min sw32_d_pix_min
#define d_pix_shift sw32_d_pix_shift
#define d_pzbuffer sw32_d_pzbuffer
#define d_roverwrapped sw32_d_roverwrapped
#define d_scalemip sw32_d_scalemip
#define d_scantable sw32_d_scantable
#define d_sdivzorigin sw32_d_sdivzorigin
#define d_sdivzstepu sw32_d_sdivzstepu
#define d_sdivzstepv sw32_d_sdivzstepv
#define d_tdivzorigin sw32_d_tdivzorigin
#define d_tdivzstepu sw32_d_tdivzstepu
#define d_tdivzstepv sw32_d_tdivzstepv
#define d_viewbuffer sw32_d_viewbuffer
#define d_vrectbottom_particle sw32_d_vrectbottom_particle
#define d_vrectright_particle sw32_d_vrectright_particle
#define d_vrectx sw32_d_vrectx
#define d_vrecty sw32_d_vrecty
#define d_y_aspect_shift sw32_d_y_aspect_shift
#define d_ziorigin sw32_d_ziorigin
#define d_zistepu sw32_d_zistepu
#define d_zistepv sw32_d_zistepv
#define d_zitable sw32_d_zitable
#define d_zrowbytes sw32_d_zrowbytes
#define d_zwidth sw32_d_zwidth
#define edge_max sw32_edge_max
#define edge_p sw32_edge_p
#define insubmodel sw32_insubmodel
#define intsintable sw32_intsintable
#define newedges sw32_newedges
#define numbtofpolys sw32_numbtofpolys
#define pauxverts sw32_pauxverts
#define pfinalverts sw32_pfinalverts
#define pfrustum_indexes sw32_pfrustum_indexes
#define pixelAspect sw32_pixelAspect
#define r_affinetridesc sw32_r_affinetridesc
#define r_aliastransition sw32_r_aliastransition
#define r_aliasuvscale sw32_r_aliasuvscale
#define r_ambientlight sw32_r_ambientlight
#define r_amodels_drawn sw32_r_amodels_drawn
#define r_apverts sw32_r_apverts
#define r_ceilv1 sw32_r_ceilv1
#define r_clipflags sw32_r_clipflags
#define r_currentbkey sw32_r_currentbkey
#define r_currentkey sw32_r_currentkey
#define r_dowarp sw32_r_dowarp
#define r_dowarpold sw32_r_dowarpold
#define r_drawculledpolys sw32_r_drawculledpolys
#define r_drawnpolycount sw32_r_drawnpolycount
#define r_drawpolys sw32_r_drawpolys
#define r_drawsurf sw32_r_drawsurf
#define r_easter_eggs_f sw32_r_easter_eggs_f
#define r_edges sw32_r_edges
#define r_emitted sw32_r_emitted
#define r_frustum_indexes sw32_r_frustum_indexes
#define r_lastvertvalid sw32_r_lastvertvalid
#define r_leftclipped sw32_r_leftclipped
#define r_leftenter sw32_r_leftenter
#define r_leftexit sw32_r_leftexit
#define r_lzi1 sw32_r_lzi1
#define r_maxedgesseen sw32_r_maxedgesseen
#define r_maxsurfsseen sw32_r_maxsurfsseen
#define r_nearzi sw32_r_nearzi
#define r_nearzionly sw32_r_nearzionly
#define r_numallocatededges sw32_r_numallocatededges
#define r_outofedges sw32_r_outofedges
#define r_outofsurfaces sw32_r_outofsurfaces
#define r_particles_style_f sw32_r_particles_style_f
#define r_pedge sw32_r_pedge
#define r_pixbytes sw32_r_pixbytes
#define r_plightvec sw32_r_plightvec
#define r_polycount sw32_r_polycount
#define r_resfudge sw32_r_resfudge
#define r_rightclipped sw32_r_rightclipped
#define r_rightenter sw32_r_rightenter
#define r_rightexit sw32_r_rightexit
#define r_shadelight sw32_r_shadelight
#define r_skymade sw32_r_skymade
#define r_skysource sw32_r_skysource
#define r_skyspeed sw32_r_skyspeed
#define r_skytime sw32_r_skytime
#define r_spritedesc sw32_r_spritedesc
#define r_u1 sw32_r_u1
#define r_v1 sw32_r_v1
#define r_viewchanged sw32_r_viewchanged
#define r_warpbuffer sw32_r_warpbuffer
#define r_worldmodelorg sw32_r_worldmodelorg
#define r_worldpolysbacktofront sw32_r_worldpolysbacktofront
#define r_zpointdesc sw32_r_zpointdesc
#define removeedges sw32_removeedges
#define sadjust sw32_sadjust
#define sc_rover sw32_sc_rover
#define scale_for_mip sw32_scale_for_mip
#define screenedge sw32_screenedge
#define screenwidth sw32_screenwidth
#define sintable sw32_sintable
#define surf_max sw32_surf_max
#define surface_p sw32_surface_p
extern struct surf_s *sw32_surfaces;
//#define surfaces sw32_surfaces
#define tadjust sw32_tadjust
#define view_clipplanes sw32_view_clipplanes
#define xcenter sw32_xcenter
#define xscale sw32_xscale
#define xscaleinv sw32_xscaleinv
#define xscaleshrink sw32_xscaleshrink
#define ycenter sw32_ycenter
#define yscale sw32_yscale
#define yscaleinv sw32_yscaleinv
#define yscaleshrink sw32_yscaleshrink
#define zspantable sw32_zspantable
#else
#undef D_CacheSurface
#undef D_DrawParticle
#undef D_DrawPoly
#undef D_DrawSkyScans
#undef D_DrawSpans
#undef D_DrawSprite
#undef D_DrawSurfaces
#undef D_DrawZPoint
#undef D_DrawZSpans
#undef D_FillRect
#undef D_FlushCaches
#undef D_Init
#undef D_InitCaches
#undef D_MipLevelForScale
#undef D_PolysetCalcGradients
#undef D_PolysetDraw
#undef D_PolysetScanLeftEdge
#undef D_PolysetSetEdgeTable
#undef D_RasterizeAliasPolySmooth
#undef D_SetupFrame
#undef D_SpriteDrawSpans
#undef D_SurfaceCacheAddress
#undef D_SurfaceCacheForRes
#undef D_TurnZOn
#undef D_UpdateRects
#undef D_ViewChanged
#undef D_WarpScreen
#undef Draw_AltString
#undef Draw_BlendScreen
#undef Draw_CachePic
#undef Draw_Character
#undef Draw_ConsoleBackground
#undef Draw_Crosshair
#undef Draw_CrosshairAt
#undef Draw_DestroyPic
#undef Draw_FadeScreen
#undef Draw_Fill
#undef Draw_Init
#undef Draw_MakePic
#undef Draw_Pic
#undef Draw_PicFromWad
#undef Draw_Picf
#undef Draw_String
#undef Draw_SubPic
#undef Draw_TextBox
#undef Draw_TileClear
#undef Draw_UncachePic
#undef Draw_nString
#undef R_AliasCheckBBox
#undef R_AliasClipTriangle
#undef R_AliasClipAndProjectFinalVert
#undef R_AliasDrawModel
#undef R_AliasProjectFinalVert
#undef R_AliasSetUpTransform
#undef R_AliasTransformAndProjectFinalVerts
#undef R_AliasTransformFinalVert
#undef R_AliasTransformVector
#undef R_Alias_clip_bottom
#undef R_Alias_clip_left
#undef R_Alias_clip_right
#undef R_Alias_clip_top
#undef R_IQMDrawModel
#undef R_BeginEdgeFrame
#undef R_ClearState
#undef R_ClipEdge
#undef R_DrawParticles
#undef R_DrawSolidClippedSubmodelPolygons
#undef R_DrawSprite
#undef R_DrawSubmodelPolygons
#undef R_DrawSurface
#undef R_EmitEdge
#undef R_GenerateSpans
#undef R_Init
#undef R_InitParticles
#undef R_InitSky
#undef R_InitTurb
#undef R_InsertNewEdges
#undef R_LineGraph
#undef R_LoadSky_f
#undef R_LoadSkys
#undef R_MakeSky
#undef R_NewMap
#undef R_Particle_New
#undef R_Particle_NewRandom
#undef R_Particles_Init_Cvars
#undef R_PrintAliasStats
#undef R_PrintTimes
#undef R_ReadPointFile_f
#undef R_RemoveEdges
#undef R_RenderBmodelFace
#undef R_RenderFace
#undef R_RenderPoly
#undef R_RenderView
#undef R_RenderWorld
#undef R_RotateBmodel
#undef R_ScanEdges
#undef R_SetSkyFrame
#undef R_SetupFrame
#undef R_StepActiveU
#undef R_Textures_Init
#undef R_TimeRefresh_f
#undef R_TransformFrustum
#undef R_TransformPlane
#undef R_ViewChanged
#undef R_ZDrawSubmodelPolys
#undef SCR_CaptureBGR
#undef SCR_ScreenShot
#undef SCR_ScreenShot_f
#undef R_RenderFrame
#undef TransformVector
#undef Turbulent
#undef VID_InitBuffers
#undef VID_ShiftPalette
#undef acolormap
#undef aliasxcenter
#undef aliasxscale
#undef aliasycenter
#undef aliasyscale
#undef auxedges
#undef bbextents
#undef bbextentt
#undef c_faceclip
#undef c_surf
#undef cacheblock
#undef cachewidth
#undef d_initial_rover
#undef d_minmip
#undef d_pix_max
#undef d_pix_min
#undef d_pix_shift
#undef d_pzbuffer
#undef d_roverwrapped
#undef d_scalemip
#undef d_scantable
#undef d_sdivzorigin
#undef d_sdivzstepu
#undef d_sdivzstepv
#undef d_tdivzorigin
#undef d_tdivzstepu
#undef d_tdivzstepv
#undef d_viewbuffer
#undef d_vrectbottom_particle
#undef d_vrectright_particle
#undef d_vrectx
#undef d_vrecty
#undef d_y_aspect_shift
#undef d_zitable
#undef d_zrowbytes
#undef d_zwidth
#undef edge_max
#undef edge_p
#undef insubmodel
#undef intsintable
#undef newedges
#undef numbtofpolys
#undef pauxverts
#undef pfinalverts
#undef pfrustum_indexes
#undef pixelAspect
#undef r_affinetridesc
#undef r_aliastransition
#undef r_aliasuvscale
#undef r_ambientlight
#undef r_amodels_drawn
#undef r_apverts
#undef r_ceilv1
#undef r_clipflags
#undef r_currentbkey
#undef r_currentkey
#undef r_dowarp
#undef r_dowarpold
#undef r_drawculledpolys
#undef r_drawnpolycount
#undef r_drawpolys
#undef r_drawsurf
#undef r_easter_eggs_f
#undef r_edges
#undef r_emitted
#undef r_frustum_indexes
#undef r_lastvertvalid
#undef r_leftclipped
#undef r_leftenter
#undef r_leftexit
#undef r_lzi1
#undef r_maxedgesseen
#undef r_maxsurfsseen
#undef r_nearzi
#undef r_nearzionly
#undef r_numallocatededges
#undef r_outofedges
#undef r_outofsurfaces
#undef r_particles_style_f
#undef r_pedge
#undef r_pixbytes
#undef r_plightvec
#undef r_polycount
#undef r_resfudge
#undef r_rightclipped
#undef r_rightenter
#undef r_rightexit
#undef r_shadelight
#undef r_skymade
#undef r_skysource
#undef r_skyspeed
#undef r_skytime
#undef r_spritedesc
#undef r_u1
#undef r_v1
#undef r_viewchanged
#undef r_warpbuffer
#undef r_worldmodelorg
#undef r_worldpolysbacktofront
#undef r_zpointdesc
#undef removeedges
#undef sadjust
#undef sc_rover
#undef scale_for_mip
#undef screenedge
#undef screenwidth
#undef sintable
#undef surf_max
#undef surface_p
//#undef surfaces
#undef tadjust
#undef view_clipplanes
#undef xcenter
#undef xscale
#undef xscaleinv
#undef xscaleshrink
#undef ycenter
#undef yscale
#undef yscaleinv
#undef yscaleshrink
#undef zspantable
#endif
