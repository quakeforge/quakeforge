// ATI Extensions

// GL_ATI_element_array (R100, Chaplin)
#ifndef GL_ATI_element_array
#define GL_ATI_element_array 1

#define GL_ELEMENT_ARRAY_ATI										0x8768
#define GL_ELEMENT_ARRAY_TYPE_ATI									0x8769
#define GL_ELEMENT_ARRAY_POINTER_ATI   								0x876A
 
typedef GLvoid (GLAPIENTRY *QF_PFNGLELEMENTPOINTERATIPROC)
	(GLenum type, const GLvoid *pointer);
typedef GLvoid (GLAPIENTRY *QF_PFNGLDRAWELEMENTARRAYATIPROC)
	(GLenum mode, GLsizei count);
typedef GLvoid (GLAPIENTRY *QF_PFNGLDRAWRANGEELEMENTARRAYATIPROC)
	(GLenum mode, GLuint start, GLuint end, GLsizei count);

#endif // GL_ATI_element_array


// GL_ATI_envmap_bumpmap (R100+)
#ifndef GL_ATI_envmap_bumpmap
#define GL_ATI_envmap_bumpmap 1

#define GL_BUMP_ROT_MATRIX_ATI										0x8775
#define GL_BUMP_ROT_MATRIX_SIZE_ATI									0x8776
#define GL_BUMP_NUM_TEX_UNITS_ATI									0x8777
#define GL_BUMP_TEX_UNITS_ATI										0x8778
#define GL_DUDV_ATI													0x8779
#define GL_DU8DV8_ATI												0x877A
#define GL_BUMP_ENVMAP_ATI											0x877B
#define GL_BUMP_TARGET_ATI											0x877C

typedef GLvoid (GLAPIENTRY *QF_PFNGLTEXBUMPPARAMETERIVATIPROC)
	(GLenum pname, GLint *param);
typedef GLvoid (GLAPIENTRY *QF_PFNGLTEXBUMPPARAMETERFVATIPROC)
	(GLenum pname, GLfloat *param);
typedef GLvoid (GLAPIENTRY *QF_PFNGLGETTEXBUMPPARAMETERIVATIPROC)
	(GLenum pname, GLint *param);
typedef GLvoid (GLAPIENTRY *QF_PFNGLGETTEXBUMPPARAMETERFVATIPROC)
	(GLenum pname, GLfloat *param);

#endif	// GL_ATI_envmap_bumpmap


// GL_ATI_fragment_shader (R200+)
#ifndef GL_ATI_fragment_shader
#define GL_ATI_fragment_shader 1

#define GL_FRAGMENT_SHADER_ATI										0x8920
#define GL_REG_0_ATI												0x8921
#define GL_REG_1_ATI												0x8922
#define GL_REG_2_ATI												0x8923
#define GL_REG_3_ATI												0x8924
#define GL_REG_4_ATI												0x8925
#define GL_REG_5_ATI												0x8926
#define GL_REG_6_ATI												0x8927
#define GL_REG_7_ATI												0x8928
#define GL_REG_8_ATI												0x8929
#define GL_REG_9_ATI												0x892A
#define GL_REG_10_ATI												0x892B
#define GL_REG_11_ATI												0x892C
#define GL_REG_12_ATI												0x892D
#define GL_REG_13_ATI												0x892E
#define GL_REG_14_ATI												0x892F
#define GL_REG_15_ATI												0x8930
#define GL_REG_16_ATI												0x8931
#define GL_REG_17_ATI												0x8932
#define GL_REG_18_ATI												0x8933
#define GL_REG_19_ATI												0x8934
#define GL_REG_20_ATI												0x8935
#define GL_REG_21_ATI												0x8936
#define GL_REG_22_ATI												0x8937
#define GL_REG_23_ATI												0x8938
#define GL_REG_24_ATI												0x8939
#define GL_REG_25_ATI												0x893A
#define GL_REG_26_ATI												0x893B
#define GL_REG_27_ATI												0x893C
#define GL_REG_28_ATI												0x893D
#define GL_REG_29_ATI												0x893E
#define GL_REG_30_ATI												0x893F
#define GL_REG_31_ATI												0x8940
#define GL_CON_0_ATI												0x8941
#define GL_CON_1_ATI												0x8942
#define GL_CON_2_ATI												0x8943
#define GL_CON_3_ATI												0x8944
#define GL_CON_4_ATI												0x8945
#define GL_CON_5_ATI												0x8946
#define GL_CON_6_ATI												0x8947
#define GL_CON_7_ATI												0x8948
#define GL_CON_8_ATI												0x8949
#define GL_CON_9_ATI												0x894A
#define GL_CON_10_ATI												0x894B
#define GL_CON_11_ATI												0x894C
#define GL_CON_12_ATI												0x894D
#define GL_CON_13_ATI												0x894E
#define GL_CON_14_ATI												0x894F
#define GL_CON_15_ATI												0x8950
#define GL_CON_16_ATI												0x8951
#define GL_CON_17_ATI												0x8952
#define GL_CON_18_ATI												0x8953
#define GL_CON_19_ATI												0x8954
#define GL_CON_20_ATI												0x8955
#define GL_CON_21_ATI												0x8956
#define GL_CON_22_ATI												0x8957
#define GL_CON_23_ATI												0x8958
#define GL_CON_24_ATI												0x8959
#define GL_CON_25_ATI												0x895A
#define GL_CON_26_ATI												0x895B
#define GL_CON_27_ATI												0x895C
#define GL_CON_28_ATI												0x895D
#define GL_CON_29_ATI												0x895E
#define GL_CON_30_ATI												0x895F
#define GL_CON_31_ATI												0x8960
#define GL_MOV_ATI													0x8961
#define GL_ADD_ATI													0x8963
#define GL_MUL_ATI													0x8964
#define GL_SUB_ATI													0x8965
#define GL_DOT3_ATI													0x8966
#define GL_DOT4_ATI													0x8967
#define GL_MAD_ATI													0x8968
#define GL_LERP_ATI													0x8969
#define GL_CND_ATI													0x896A
#define GL_CND0_ATI													0x896B
#define GL_DOT2_ADD_ATI												0x896C
#define GL_SECONDARY_INTERPOLATOR_ATI								0x896D
#define GL_NUM_FRAGMENT_REGISTERS_ATI								0x896E
#define GL_NUM_FRAGMENT_CONSTANTS_ATI								0x896F
#define GL_NUM_PASSES_ATI											0x8970
#define GL_NUM_INSTRUCTIONS_PER_PASS_ATI							0x8971
#define GL_NUM_INSTRUCTIONS_TOTAL_ATI								0x8972
#define GL_NUM_INPUT_INTERPOLATOR_COMPONENTS_ATI					0x8973
#define GL_NUM_LOOPBACK_COMPONENTS_ATI								0x8974
#define GL_COLOR_ALPHA_PAIRING_ATI									0x8975
#define GL_SWIZZLE_STR_ATI											0x8976
#define GL_SWIZZLE_STQ_ATI											0x8977
#define GL_SWIZZLE_STR_DR_ATI										0x8978
#define GL_SWIZZLE_STQ_DQ_ATI										0x8979
#define GL_SWIZZLE_STRQ_ATI											0x897A
#define GL_SWIZZLE_STRQ_DQ_ATI										0x897B
#define GL_RED_BIT_ATI												0x00000001
#define GL_GREEN_BIT_ATI											0x00000002
#define GL_BLUE_BIT_ATI												0x00000004
#define GL_2X_BIT_ATI												0x00000001
#define GL_4X_BIT_ATI												0x00000002
#define GL_8X_BIT_ATI												0x00000004
#define GL_HALF_BIT_ATI												0x00000008
#define GL_QUARTER_BIT_ATI											0x00000010
#define GL_EIGHTH_BIT_ATI											0x00000020
#define GL_SATURATE_BIT_ATI											0x00000040
#define GL_COMP_BIT_ATI												0x00000002
#define GL_NEGATE_BIT_ATI											0x00000004
#define GL_BIAS_BIT_ATI												0x00000008
typedef GLuint (GLAPIENTRY *QF_PFNGLGENFRAGMENTSHADERSATIPROC) (GLuint range);
typedef GLvoid (GLAPIENTRY *QF_PFNGLBINDFRAGMENTSHADERATIPROC) (GLuint id);
typedef GLvoid (GLAPIENTRY *QF_PFNGLDELETEFRAGMENTSHADERATIPROC) (GLuint id);
typedef GLvoid (GLAPIENTRY *QF_PFNGLBEGINFRAGMENTSHADERATIPROC) (GLvoid);
typedef GLvoid (GLAPIENTRY *QF_PFNGLENDFRAGMENTSHADERATIPROC) (GLvoid);
typedef GLvoid (GLAPIENTRY *QF_PFNGLPASSTEXCOORDATIPROC)
	(GLuint dst, GLuint coord, GLenum swizzle);
typedef GLvoid (GLAPIENTRY *QF_PFNGLSAMPLEMAPATIPROC)
	(GLuint dst, GLuint interp, GLenum swizzle);
typedef GLvoid (GLAPIENTRY *QF_PFNGLCOLORFRAGMENTOP1ATIPROC)
	(GLenum op, GLuint dst, GLuint dstMask, GLuint dstMod,
	 GLuint arg1, GLuint arg1Rep, GLuint arg1Mod);
typedef GLvoid (GLAPIENTRY *QF_PFNGLCOLORFRAGMENTOP2ATIPROC)
	(GLenum op, GLuint dst, GLuint dstMask, GLuint dstMod,
	 GLuint arg1, GLuint arg1Rep, GLuint arg1Mod,
	 GLuint arg2, GLuint arg2Rep, GLuint arg2Mod);
typedef GLvoid (GLAPIENTRY *QF_PFNGLCOLORFRAGMENTOP3ATIPROC)
	(GLenum op, GLuint dst, GLuint dstMask,
	 GLuint dstMod, GLuint arg1, GLuint arg1Rep, GLuint arg1Mod,
	 GLuint arg2, GLuint arg2Rep, GLuint arg2Mod,
	 GLuint arg3, GLuint arg3Rep, GLuint arg3Mod);
typedef GLvoid (GLAPIENTRY *QF_PFNGLALPHAFRAGMENTOP1ATIPROC)
	(GLenum op, GLuint dst, GLuint dstMod,
	 GLuint arg1, GLuint arg1Rep, GLuint arg1Mod);
typedef GLvoid (GLAPIENTRY *QF_PFNGLALPHAFRAGMENTOP2ATIPROC)
	(GLenum op, GLuint dst, GLuint dstMod,
	 GLuint arg1, GLuint arg1Rep, GLuint arg1Mod,
	 GLuint arg2, GLuint arg2Rep, GLuint arg2Mod);
typedef GLvoid (GLAPIENTRY *QF_PFNGLALPHAFRAGMENTOP3ATIPROC)
	(GLenum op, GLuint dst, GLuint dstMod,
	 GLuint arg1, GLuint arg1Rep, GLuint arg1Mod,
	 GLuint arg2, GLuint arg2Rep, GLuint arg2Mod,
	 GLuint arg3, GLuint arg3Rep, GLuint arg3Mod);
typedef GLvoid (GLAPIENTRY *QF_PFNGLSETFRAGMENTSHADERCONSTANTATIPROC)
	(GLuint dst, const GLfloat *value);
 
#endif // GL_ATI_fragment_shader

 
// GL_ATI_pn_triangles (Supported on R200+)
#ifndef GL_ATI_pn_triangles
#define GL_ATI_pn_triangles 1

#define GL_PN_TRIANGLES_ATI											0x87F0
#define GL_MAX_PN_TRIANGLES_TESSELATION_LEVEL_ATI					0x87F1
#define GL_PN_TRIANGLES_POINT_MODE_ATI								0x87F2
#define GL_PN_TRIANGLES_NORMAL_MODE_ATI								0x87F3
#define GL_PN_TRIANGLES_TESSELATION_LEVEL_ATI						0x87F4
#define GL_PN_TRIANGLES_POINT_MODE_LINEAR_ATI						0x87F5
#define GL_PN_TRIANGLES_POINT_MODE_CUBIC_ATI						0x87F6
#define GL_PN_TRIANGLES_NORMAL_MODE_LINEAR_ATI						0x87F7
#define GL_PN_TRIANGLES_NORMAL_MODE_QUADRATIC_ATI					0x87F8

typedef GLvoid (GLAPIENTRY *QF_PFNGLPNTRIANGLESIATIPROC)
	(GLenum pname, GLint param);
typedef GLvoid (GLAPIENTRY *QF_PFNGLPNTRIANGLESFATIPROC)
	(GLenum pname, GLfloat param);

#endif // GL_ATI_pn_triangles


// GL_ATI_texture_mirror_once (Rage 128 & R100+)
#ifndef GL_ATI_texture_mirror_once
#define GL_ATI_texture_mirror_once 1

#define GL_MIRROR_CLAMP_ATI											0x8742
#define GL_MIRROR_CLAMP_TO_EDGE_ATI									0x8743

#endif // GL_ATI_texture_mirror_once


// GL_ATI_vertex_array_object (R100 & Chaplin)
#ifndef GL_ATI_vertex_array_object
#define GL_ATI_vertex_array_object 1

#define GL_STATIC_ATI												0x8760
#define GL_DYNAMIC_ATI												0x8761
#define GL_PRESERVE_ATI												0x8762
#define GL_DISCARD_ATI												0x8763
#define GL_OBJECT_BUFFER_SIZE_ATI									0x8764
#define GL_OBJECT_BUFFER_USAGE_ATI									0x8765
#define GL_ARRAY_OBJECT_BUFFER_ATI									0x8766
#define GL_ARRAY_OBJECT_OFFSET_ATI									0x8767

typedef GLuint (GLAPIENTRY *QF_PFNGLNEWOBJECTBUFFERATIPROC)
	(GLsizei size, const GLvoid *pointer, GLenum usage);
typedef GLboolean (GLAPIENTRY *QF_PFNGLISOBJECTBUFFERATIPROC) (GLuint buffer);
typedef GLvoid (GLAPIENTRY *QF_PFNGLUPDATEOBJECTBUFFERATIPROC)
	(GLuint buffer, GLuint offset, GLsizei size, const GLvoid *pointer,
	 GLenum preserve);
typedef GLvoid (GLAPIENTRY *QF_PFNGLGETOBJECTBUFFERFVATIPROC)
	(GLuint buffer, GLenum pname, GLfloat *params);
typedef GLvoid (GLAPIENTRY *QF_PFNGLGETOBJECTBUFFERIVATIPROC)
	(GLuint buffer, GLenum pname, GLint *params);
typedef GLvoid (GLAPIENTRY *QF_PFNGLFREEOBJECTBUFFERATIPROC) (GLuint buffer);
typedef GLvoid (GLAPIENTRY *QF_PFNGLARRAYOBJECTATIPROC)
	(GLenum array, GLint size, GLenum type, GLsizei stride, GLuint buffer,
	 GLuint offset);
typedef GLvoid (GLAPIENTRY *QF_PFNGLGETARRAYOBJECTFVATIPROC)
	(GLenum array, GLenum pname, GLfloat *params);
typedef GLvoid (GLAPIENTRY *QF_PFNGLGETARRAYOBJECTIVATIPROC)
	(GLenum array, GLenum pname, GLint *params);
typedef GLvoid (GLAPIENTRY *QF_PFNGLVARIANTARRAYOBJECTATIPROC)
	(GLuint id, GLenum type, GLsizei stride, GLuint buffer, GLuint offset);
typedef GLvoid (GLAPIENTRY *QF_PFNGLGETVARIANTARRAYOBJECTFVATIPROC)
	(GLuint id, GLenum pname, GLfloat *params);
typedef GLvoid (GLAPIENTRY *QF_PFNGLGETVARIANTARRAYOBJECTIVATIPROC)
	(GLuint id, GLenum pname, GLint *params);
#endif // GL_ATI_vertex_array_object


// GL_ATI_vertex_streams (Rage 128 & R100+)
#ifndef GL_ATI_vertex_streams
#define GL_ATI_vertex_streams 1
 
#define GL_MAX_VERTEX_STREAMS_ATI									0x876B
#define GL_VERTEX_SOURCE_ATI										0x876C
#define GL_VERTEX_STREAM0_ATI										0x876D
#define GL_VERTEX_STREAM1_ATI										0x876E
#define GL_VERTEX_STREAM2_ATI										0x876F
#define GL_VERTEX_STREAM3_ATI										0x8770
#define GL_VERTEX_STREAM4_ATI										0x8771
#define GL_VERTEX_STREAM5_ATI										0x8772
#define GL_VERTEX_STREAM6_ATI										0x8773
#define GL_VERTEX_STREAM7_ATI										0x8774
 
typedef GLvoid (GLAPIENTRY *QF_PFNGLCLIENTACTIVEVERTEXSTREAMATIPROC)
	(GLenum stream);
typedef GLvoid (GLAPIENTRY *QF_PFNGLVERTEXBLENDENVIATIPROC)
	(GLenum pname, GLint param);
typedef GLvoid (GLAPIENTRY *QF_PFNGLVERTEXBLENDENVFATIPROC)
	(GLenum pname, GLfloat param);
typedef GLvoid (GLAPIENTRY *QF_PFNGLVERTEXSTREAM2SATIPROC)
	(GLenum stream, GLshort x, GLshort y);
typedef GLvoid (GLAPIENTRY *QF_PFNGLVERTEXSTREAM2SVATIPROC)
	(GLenum stream, const GLshort *v);
typedef GLvoid (GLAPIENTRY *QF_PFNGLVERTEXSTREAM2IATIPROC)
	(GLenum stream, GLint x, GLint y);
typedef GLvoid (GLAPIENTRY *QF_PFNGLVERTEXSTREAM2IVATIPROC)
	(GLenum stream, const GLint *v);
typedef GLvoid (GLAPIENTRY *QF_PFNGLVERTEXSTREAM2FATIPROC)
	(GLenum stream, GLfloat x, GLfloat y);
typedef GLvoid (GLAPIENTRY *QF_PFNGLVERTEXSTREAM2FVATIPROC)
	(GLenum stream, const GLfloat *v);
typedef GLvoid (GLAPIENTRY *QF_PFNGLVERTEXSTREAM2DATIPROC)
	(GLenum stream, GLdouble x, GLdouble y);
typedef GLvoid (GLAPIENTRY *QF_PFNGLVERTEXSTREAM2DVATIPROC)
	(GLenum stream, const GLdouble *v);
typedef GLvoid (GLAPIENTRY *QF_PFNGLVERTEXSTREAM3SATIPROC)
	(GLenum stream, GLshort x, GLshort y, GLshort z);
typedef GLvoid (GLAPIENTRY *QF_PFNGLVERTEXSTREAM3SVATIPROC)
	(GLenum stream, const GLshort *v);
typedef GLvoid (GLAPIENTRY *QF_PFNGLVERTEXSTREAM3IATIPROC)
	(GLenum stream, GLint x, GLint y, GLint z);
typedef GLvoid (GLAPIENTRY *QF_PFNGLVERTEXSTREAM3IVATIPROC)
	(GLenum stream, const GLint *v);
typedef GLvoid (GLAPIENTRY *QF_PFNGLVERTEXSTREAM3FATIPROC)
	(GLenum stream, GLfloat x, GLfloat y, GLfloat z);
typedef GLvoid (GLAPIENTRY *QF_PFNGLVERTEXSTREAM3FVATIPROC)
	(GLenum stream, const GLfloat *v);
typedef GLvoid (GLAPIENTRY *QF_PFNGLVERTEXSTREAM3DATIPROC)
	(GLenum stream, GLdouble x, GLdouble y, GLdouble z);
typedef GLvoid (GLAPIENTRY *QF_PFNGLVERTEXSTREAM3DVATIPROC)
	(GLenum stream, const GLdouble *v);
typedef GLvoid (GLAPIENTRY *QF_PFNGLVERTEXSTREAM4SATIPROC)
	(GLenum stream, GLshort x, GLshort y, GLshort z, GLshort w);
typedef GLvoid (GLAPIENTRY *QF_PFNGLVERTEXSTREAM4SVATIPROC)
	(GLenum stream, const GLshort *v);
typedef GLvoid (GLAPIENTRY *QF_PFNGLVERTEXSTREAM4IATIPROC)
	(GLenum stream, GLint x, GLint y, GLint z, GLint w);
typedef GLvoid (GLAPIENTRY *QF_PFNGLVERTEXSTREAM4IVATIPROC)
	(GLenum stream, const GLint *v);
typedef GLvoid (GLAPIENTRY *QF_PFNGLVERTEXSTREAM4FATIPROC)
	(GLenum stream, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef GLvoid (GLAPIENTRY *QF_PFNGLVERTEXSTREAM4FVATIPROC)
	(GLenum stream, const GLfloat *v);
typedef GLvoid (GLAPIENTRY *QF_PFNGLVERTEXSTREAM4DATIPROC)
	(GLenum stream, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef GLvoid (GLAPIENTRY *QF_PFNGLVERTEXSTREAM4DVATIPROC)
	(GLenum stream, const GLdouble *v);
typedef GLvoid (GLAPIENTRY *QF_PFNGLNORMALSTREAM3BATIPROC)
	(GLenum stream, GLbyte x, GLbyte y, GLbyte z);
typedef GLvoid (GLAPIENTRY *QF_PFNGLNORMALSTREAM3BVATIPROC)
	(GLenum stream, const GLbyte *v);
typedef GLvoid (GLAPIENTRY *QF_PFNGLNORMALSTREAM3SATIPROC)
	(GLenum stream, GLshort x, GLshort y, GLshort z);
typedef GLvoid (GLAPIENTRY *QF_PFNGLNORMALSTREAM3SVATIPROC)
	(GLenum stream, const GLshort *v);
typedef GLvoid (GLAPIENTRY *QF_PFNGLNORMALSTREAM3IATIPROC)
	(GLenum stream, GLint x, GLint y, GLint z);
typedef GLvoid (GLAPIENTRY *QF_PFNGLNORMALSTREAM3IVATIPROC)
	(GLenum stream, const GLint *v);
typedef GLvoid (GLAPIENTRY *QF_PFNGLNORMALSTREAM3FATIPROC)
	(GLenum stream, GLfloat x, GLfloat y, GLfloat z);
typedef GLvoid (GLAPIENTRY *QF_PFNGLNORMALSTREAM3FVATIPROC)
	(GLenum stream, const GLfloat *v);
typedef GLvoid (GLAPIENTRY *QF_PFNGLNORMALSTREAM3DATIPROC)
	(GLenum stream, GLdouble x, GLdouble y, GLdouble z);
typedef GLvoid (GLAPIENTRY *QF_PFNGLNORMALSTREAM3DVATIPROC)
	(GLenum stream, const GLdouble *v);
#endif // GL_ATI_vertex_streams


// ATIX Extensions

// GL_ATIX_point_sprites (R100)
#ifndef GL_ATIX_point_sprites
#define GL_ATIX_point_sprites 1
 
#define GL_TEXTURE_POINT_MODE_ATIX									0x60b0
#define GL_TEXTURE_POINT_ONE_COORD_ATIX								0x60b1
#define GL_TEXTURE_POINT_SPRITE_ATIX								0x60b2
#define GL_POINT_SPRITE_CULL_MODE_ATIX								0x60b3
#define GL_POINT_SPRITE_CULL_CENTER_ATIX							0x60b4
#define GL_POINT_SPRITE_CULL_CLIP_ATIX								0x60b5

#endif // GL_ATIX_point_sprites


// GL_ATIX_texture_env_combine3 (R100)
#ifndef GL_ATIX_texture_env_combine3
#define GL_ATIX_texture_env_combine3
 
#define GL_MODULATE_ADD_ATIX										0x8744
#define GL_MODULATE_SIGNED_ADD_ATIX									0x8745
#define GL_MODULATE_SUBTRACT_ATIX									0x8746
 
#endif // GL_ATIX_texture_env_combine3

 
// GL_ATIX_texture_env_route (R100)
#ifndef GL_ATIX_texture_env_route
#define GL_ATIX_texture_env_route 1
 
#define GL_SECONDARY_COLOR_ATIX										0x8747
#define GL_TEXTURE_OUTPUT_RGB_ATIX									0x8748
#define GL_TEXTURE_OUTPUT_ALPHA_ATIX								0x8749
 
#endif // GL_ATIX_texture_env_route

 
// GL_ATIX_vertex_shader_output_point_size (Rage 128 (sw), R100 (sw) & Chaplin)
#ifndef GL_ATIX_vertex_shader_output_point_size
#define GL_ATIX_vertex_shader_output_point_size 1

#define GL_OUTPUT_POINT_SIZE_ATIX									0x610E
 
#endif // GL_ATIX_vertex_shader_output_point_size
