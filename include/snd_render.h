
// !!! if this is changed, it must be changed in asm_i386.h too !!!
typedef struct
{
	int left;
	int right;
} portable_samplepair_t;

// !!! if this is changed, it much be changed in asm_i386.h too !!!
typedef struct
{
	int 	length;
	int 	loopstart;
	int 	speed;
	int 	width;
	int 	stereo;
	int		bytes;
	byte	data[4];		// variable sized
} sfxcache_t;

// !!! if this is changed, it much be changed in asm_i386.h too !!!
typedef struct
{
	sfx_t	*sfx;			// sfx number
	int		leftvol;		// 0-255 volume
	int		rightvol;		// 0-255 volume
	int		end;			// end time in global paintsamples
	int	 	pos;			// sample position in sfx
	int		looping;		// where to loop, -1 = no looping
	int		entnum;			// to allow overriding a specific sound
	int		entchannel;		//
	vec3_t	origin;			// origin of sound effect
	vec_t	dist_mult;		// distance multiplier (attenuation/clipK)
	int	master_vol;		// 0-255 master volume
	int	phase;	// phase shift between l-r in samples
	int	oldphase;	// phase shift between l-r in samples
} channel_t;

typedef struct
{
	int		rate;
	int		width;
	int		channels;
	int		loopstart;
	int		samples;
	int		dataofs;		// chunk starts this many bytes from file start
} wavinfo_t;

void SND_PaintChannels(int endtime);
void SND_Init (void);
void SND_Shutdown (void);
void SND_AmbientOff (void);
void SND_AmbientOn (void);
void SND_TouchSound (const char *sample);
void SND_ClearBuffer (void);
void SND_StaticSound (sfx_t *sfx, const vec3_t origin, float vol,
					  float attenuation);
void SND_StartSound (int entnum, int entchannel, sfx_t *sfx,
					 const vec3_t origin, float fvol,  float attenuation);
void SND_StopSound (int entnum, int entchannel);
sfx_t *SND_PrecacheSound (const char *sample);
void SND_ClearPrecache (void);
void SND_Update (const vec3_t origin, const vec3_t v_forward,
				 const vec3_t v_right, const vec3_t v_up);
void SND_StopAllSounds (qboolean clear);
void SND_BeginPrecaching (void);
void SND_EndPrecaching (void);
void SND_ExtraUpdate (void);
void SND_LocalSound (const char *s);
void SND_BlockSound (void);
void SND_UnblockSound (void);

void SND_ResampleSfx (sfxcache_t *sc, byte * data);
sfxcache_t *SND_GetCache (long samples, int rate, int inwidth, int channels,
						  sfx_t *sfx, cache_allocator_t allocator);

void SND_InitScaletable (void);
// picks a channel based on priorities, empty slots, number of channels
channel_t *SND_PickChannel(int entnum, int entchannel);
// spatializes a channel
void SND_Spatialize(channel_t *ch);

void SND_CallbackLoad (void *object, cache_allocator_t allocator);
sfxcache_t *SND_LoadOgg (QFile *file, sfx_t *sfx, cache_allocator_t allocator);
wavinfo_t SND_GetWavinfo (const char *name, byte * wav, int wavlength);

void SND_WriteLinearBlastStereo16 (void);
void SND_PaintChannelFrom8 (channel_t *ch, sfxcache_t *sc, int count);
void SND_PaintChannelFrom16 (channel_t *ch, sfxcache_t *sc, int count);

wavinfo_t GetWavinfo (const char *name, byte *wav, int wavlength);

extern	channel_t   channels[MAX_CHANNELS];
// 0 to MAX_DYNAMIC_CHANNELS-1	= normal entity sounds
// MAX_DYNAMIC_CHANNELS to MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS -1 = water, etc
// MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS to total_channels = static sounds

extern	int			total_channels;
