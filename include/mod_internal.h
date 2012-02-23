#ifndef __mod_internal_h
#define __mod_internal_h

#include "QF/model.h"
#include "QF/skin.h"
#include "QF/plugin/vid_render.h"

extern vid_model_funcs_t *m_funcs;

void Mod_LoadExternalTextures (model_t *mod);
void Mod_LoadLighting (bsp_t *bsp);
void Mod_SubdivideSurface (msurface_t *fa);
void Mod_ProcessTexture(texture_t *tx);
void Mod_LoadAliasModel (model_t *mod, void *buffer, 
                         cache_allocator_t allocator);
void Mod_LoadSpriteModel (model_t *mod, void *buffer);

void Skin_Init (void);
skin_t *Skin_SetColormap (skin_t *skin, int cmap);
skin_t *Skin_SetSkin (skin_t *skin, int cmap, const char *skinname);
void Skin_SetTranslation (int cmap, int top, int bottom);

void sw_Skin_SetupSkin (skin_t *skin, int cmap);
void sw_Skin_ProcessTranslation (int cmap, const byte *translation);
void sw_Skin_InitTranslations (void);

void glsl_Skin_SetupSkin (skin_t *skin, int cmap);
void glsl_Skin_ProcessTranslation (int cmap, const byte *translation);
void glsl_Skin_InitTranslations (void);

void gl_Skin_SetupSkin (skin_t *skin, int cmap);
void gl_Skin_ProcessTranslation (int cmap, const byte *translation);
void gl_Skin_InitTranslations (void);
int gl_Skin_Init_Textures (int base);
void gl_Skin_SetPlayerSkin (int width, int height, const byte *data);
#endif// __mod_internal_h
