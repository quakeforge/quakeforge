#include <scene.h>

scene_t Scene_NewScene (void) = #0;
void Scene_DeleteScene (scene_t scene) = #0;
entity_t Scene_CreateEntity (scene_t scene) = #0;
void Scene_DestroyEntity (entity_t ent) = #0;
void Scene_SetLighting (scene_t scene, lightingdata_t ldata) = #0;

transform_t Entity_GetTransform (entity_t ent) = #0;
void Entity_SetModel (entity_t ent, model_t model) = #0;

unsigned Transform_ChildCount (transform_t transform) = #0;
transform_t Transform_GetChild (transform_t transform,
                                 unsigned childIndex) = #0;
void Transform_SetParent (transform_t transform, transform_t parent) = #0;
transform_t Transform_GetParent (transform_t transform) = #0;
void Transform_SetTag (transform_t transform, unsigned tag) = #0;
unsigned Transform_GetTag (transform_t transform) = #0;
mat4x4 Transform_GetLocalMatrix (transform_t transform) = #0;
mat4x4 Transform_GetLocalInverse (transform_t transform) = #0;
mat4x4 Transform_GetWorldMatrix (transform_t transform) = #0;
mat4x4 Transform_GetWorldInverse (transform_t transform) = #0;
vec4 Transform_GetLocalPosition (transform_t transform) = #0;
void Transform_SetLocalPosition (transform_t transform, vec4 position) = #0;
vec4 Transform_GetLocalRotation (transform_t transform) = #0;
void Transform_SetLocalRotation (transform_t transform, vec4 rotation) = #0;
vec4 Transform_GetLocalScale (transform_t transform) = #0;
void Transform_SetLocalScale (transform_t transform, vec4 scale) = #0;
vec4 Transform_GetWorldPosition (transform_t transform) = #0;
void Transform_SetWorldPosition (transform_t transform, vec4 position) = #0;
vec4 Transform_GetWorldRotation (transform_t transform) = #0;
void Transform_SetWorldRotation (transform_t transform, vec4 rotation) = #0;
vec4 Transform_GetWorldScale (transform_t transform) = #0;
void Transform_SetLocalTransform (transform_t transform, vec4 scale,
								  vec4 rotation, vec4 position) = #0;
vec4 Transform_Forward (transform_t transform) = #0;
vec4 Transform_Right (transform_t transform) = #0;
vec4 Transform_Up (transform_t transform) = #0;

lightingdata_t Light_CreateLightingData (scene_t scene) = #0;
void Light_DestroyLightingData (lightingdata_t ldata) = #0;
void Light_ClearLights (lightingdata_t ldata) = #0;
void Light_AddLight (lightingdata_t ldata, light_t light) = #0;
void Light_EnableSun (lightingdata_t ldata) = #0;

model_t Model_Load (string path) = #0;
void Model_Unload (model_t model) = #0;
