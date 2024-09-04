#ifndef __ruamoko_scene_h
#define __ruamoko_scene_h

typedef struct light_s {
	vec4        color;
	vec4        position;
	vec4        direction;
	vec4        attenuation;
} light_t;

typedef @handle(long) scene_h scene_t;
typedef @handle(long) entity_h entity_t;
typedef @handle(long) transform_h transform_t;
typedef @handle(long) lightingdata_h lightingdata_t;
typedef @handle(int) model_h model_t;

scene_t Scene_NewScene (void);
void Scene_DeleteScene (scene_t scene);
entity_t Scene_CreateEntity (scene_t scene);
void Scene_DestroyEntity (entity_t ent);
void Scene_SetLighting (scene_t scene, lightingdata_t ldata);
void Scene_SetCamera (scene_t scene, entity_t ent);

transform_t Entity_GetTransform (entity_t ent);
void Entity_SetModel (entity_t ent, model_t model);

unsigned Transform_ChildCount (transform_t transform);
transform_t Transform_GetChild (transform_t transform,
                                 unsigned childIndex);
void Transform_SetParent (transform_t transform, transform_t parent);
transform_t Transform_GetParent (transform_t transform);
void Transform_SetTag (transform_t transform, unsigned tag);
unsigned Transform_GetTag (transform_t transform);
mat4x4 Transform_GetLocalMatrix (transform_t transform);
mat4x4 Transform_GetLocalInverse (transform_t transform);
mat4x4 Transform_GetWorldMatrix (transform_t transform);
mat4x4 Transform_GetWorldInverse (transform_t transform);
vec4 Transform_GetLocalPosition (transform_t transform);
void Transform_SetLocalPosition (transform_t transform, vec4 position);
vec4 Transform_GetLocalRotation (transform_t transform);
void Transform_SetLocalRotation (transform_t transform, vec4 rotation);
vec4 Transform_GetLocalScale (transform_t transform);
void Transform_SetLocalScale (transform_t transform, vec4 scale);
vec4 Transform_GetWorldPosition (transform_t transform);
void Transform_SetWorldPosition (transform_t transform, vec4 position);
vec4 Transform_GetWorldRotation (transform_t transform);
void Transform_SetWorldRotation (transform_t transform, vec4 rotation);
vec4 Transform_GetWorldScale (transform_t transform);
void Transform_SetLocalTransform (transform_t transform, vec4 scale,
								  vec4 rotation, vec4 position);
vec4 Transform_Forward (transform_t transform);
vec4 Transform_Right (transform_t transform);
vec4 Transform_Up (transform_t transform);

lightingdata_t Light_CreateLightingData (scene_t scene);
void Light_DestroyLightingData (lightingdata_t ldata);
void Light_ClearLights (lightingdata_t ldata);
void Light_AddLight (lightingdata_t ldata, light_t light, int style);
void Light_EnableSun (lightingdata_t ldata);

model_t Model_Load (string path);
void Model_Unload (model_t model);
int Model_NumJoints (model_t model);
void Model_GetJoints (model_t model, void *j);
int Model_NumFrames (model_t model);

#endif//__ruamoko_scene_h
