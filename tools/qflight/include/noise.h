float noise3d (vec3_t v, int num);
float noiseXYZ (float x, float y, float z, int num);
float noise_scaled (vec3_t v, float s, int num);
float noise_perlin (vec3_t v, float p, int num);
void snap_vector (vec3_t v_old, vec3_t v_new, float scale);
