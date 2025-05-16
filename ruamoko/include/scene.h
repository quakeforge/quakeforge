#ifndef __ruamoko_scene_h
#define __ruamoko_scene_h

typedef struct light_s {
	vec4        color;
	vec4        position;
	vec4        direction;
	vec4        attenuation;
} light_t;

typedef struct clipinfo_s {
	string      name;
	uint        num_frames;
	uint        num_channels;
	uint        channel_type;//FIXME use an enum
} clipinfo_t;

typedef struct animation_s {
	int         frame;
	float       syncbase;	// randomize time base for local animations
	float       frame_start_time;
	float       frame_interval;
	int         pose1;
	int         pose2;
	float       blend;
	int         nolerp;		// don't lerp this frame (pose data invalid)
} animation_t;

typedef @handle(long) scene_h scene_t;
typedef @handle(long) entity_h entity_t;
typedef @handle(long) transform_h transform_t;
typedef @handle(long) lightingdata_h lightingdata_t;
typedef @handle(int) model_h model_t;
typedef @handle(int) cliphandle_h cliphandle_t;
typedef @handle(int) armhandle_h armhandle_t;
typedef @handle(int) animstate_h animstate_t;

scene_t Scene_NewScene (void);
void Scene_DeleteScene (scene_t scene);
entity_t Scene_CreateEntity (scene_t scene);
void Scene_DestroyEntity (entity_t ent);
void Scene_SetLighting (scene_t scene, lightingdata_t ldata);
void Scene_SetCamera (scene_t scene, entity_t ent);

transform_t Entity_GetTransform (entity_t ent);
void Entity_SetModel (entity_t ent, model_t model);
int Entity_GetPoseMotors (entity_t ent, void *motors, double time);
animation_t *Entity_GetAnimation (entity_t ent);// NOTE: pointer is ephemeral
void Entity_SetAnimation (entity_t ent, animation_t *anim);
void Entity_SetAnimstate (entity_t ent, animstate_t anim);

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
void Model_GetJoints (model_t model, void *joints);
int Model_NumClips (model_t model);
int Model_GetInverseMotors (model_t model, void *motors);
clipinfo_t Model_GetClipInfo (model_t model, uint clip);
void *Model_GetChannelInfo (model_t model, void *data);
void *Model_GetFrameData (model_t model, uint clip, void *data);

cliphandle_t qfa_find_clip (string name);
armhandle_t qfa_find_armature (string name);
animstate_t qfa_create_animation (cliphandle_t *clips, uint num_clips,
								  armhandle_t armature, model_t model);
void qfa_free_animation (animstate_t anim);
void qfa_update_anim (animstate_t anim, float dt);
void qfa_reset_anim (animstate_t anim);
void qfa_set_clip_weight (animstate_t anim, uint clip, float weight);
void qfa_set_clip_loop (animstate_t anim, uint clip, bool loop);
void qfa_set_clip_disabled (animstate_t anim, uint clip, bool disabled);
void qfa_get_pose_motors (animstate_t anim, void *motors);

#endif//__ruamoko_scene_h
