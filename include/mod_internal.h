#ifndef __mod_internal_h
#define __mod_internal_h

#include "QF/model.h"
#include "QF/skin.h"

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
void Skin_SetupSkin (skin_t *skin, int cmap);
void Skin_SetTranslation (int cmap, int top, int bottom);
void Skin_ProcessTranslation (int cmap, const byte *translation);
void Skin_InitTranslations (void);
int Skin_Init_Textures (int base);
void Skin_SetPlayerSkin (int width, int height, const byte *data);
#endif// __mod_internal_h
