float parse_float (const char *str);
void parse_color (const char *str, vec3_t color);
float parse_light (const char *str, vec3_t color);
int parse_attenuation (const char *arg);
int parse_noise (const char *arg);
struct plitem_s;
void set_properties (entity_t *ent, struct plitem_s *dict);

void LoadProperties (const char *filename);
