/*
===============================================
Pathos Engine - Created by Andrew Stephen "Overfloater" Lucas

Copyright 2016
All Rights Reserved.
===============================================
*/

#ifndef R_DLIGHT_H
#define R_DLIGHT_H

#include "r_fbo.h"
#include "r_vbo.h"
#include "r_glsl.h"
#include "dlight.h"

// Max number of dynlight visents
static constexpr Uint32 DLIGHT_VISENTS_ALLOCSIZE = 256;

enum vsmshadertype_t
{
	VSM_SHADER_VBLUR = 0,
	VSM_SHADER_HBLUR,
	VSM_SHADER_COPY
};

struct shadowmap_t
{
	shadowmap_t():
		reset(false),
		projective(false),
		used(false),
		freetime(-1),
		pfbo(nullptr)
		{}

	bool reset;
	bool projective;
	bool used;
	Double freetime;
	struct fbobind_t* pfbo;

	CArray<struct fbobind_t*> pblitfboarray;
};

struct dlight_sceneinfo_t
{
	dlight_sceneinfo_t():
		numvisents(0),
		drawframe(0)
		{
		}

	CArray<cl_entity_t*> pvisents;
	Uint32 numvisents;

	Int32 drawframe;
};

struct dl_vertex_t
{
	vec4_t origin;
	Float texcoord[2];
	byte padding[8];
};

// Flaslight sprite global
extern cache_model_t* g_pFlashlightSprite;

/*
=================================
CDynamicLightManager

=================================
*/
class CDynamicLightManager
{
public:
	// Minimum shadowmap size
	static const Uint32 SHADOWMAP_MIN_SIZE;
	// Time until an unused shadowmap is freed
	static const Float SHADOWMAP_RELEASE_DELAY;
	// Default directional shadowmap distance / extents
	static const Float CSM_EXTENTS;

public:
	struct vsm_shader_attribs
	{
		vsm_shader_attribs():
			a_origin(CGLSLShader::PROPERTY_UNAVAILABLE),
			a_texcoord(CGLSLShader::PROPERTY_UNAVAILABLE),
			u_modelview(CGLSLShader::PROPERTY_UNAVAILABLE),
			u_projection(CGLSLShader::PROPERTY_UNAVAILABLE),
			u_texture(CGLSLShader::PROPERTY_UNAVAILABLE),
			u_size(CGLSLShader::PROPERTY_UNAVAILABLE),
			d_type(CGLSLShader::PROPERTY_UNAVAILABLE)
			{}


		Int32 a_origin;
		Int32 a_texcoord;

		Int32 u_modelview;
		Int32 u_projection;

		Int32 u_texture;
		Int32 u_size;

		Int32 d_type;
	};

public:
	CDynamicLightManager( void );
	~CDynamicLightManager( void );

public:
	// Initializes class
	bool Init( void );
	// Shuts down the class
	void Shutdown( void );
	// Initializes for a game
	bool InitGame( void );
	// Clears lights
	void ClearGame( void );

	// Initializes OpenGL objects
	bool InitGL( void );
	// Clears OpenGL objects
	void ClearGL( void );

	// Sets visibility on leaves for dlights
	void SetVIS( void );
	// Draws shadow renderpasses
	bool DrawPasses( void );
	// Updates light states
	bool Update( void );

	// Clears a shadowmap
	void ClearProjectiveShadowMap( shadowmap_t *psm );
	// Clears a shadowmap
	void ClearCubemapShadowMap( shadowmap_t *psm );

	// Allocates a dynamic light
	cl_dlight_t* AllocDynamicPointLight( Int32 key, Int32 subkey, bool isstatic, bool noshadow, cl_entity_t *pentity );
	// Allocates a spotlight light
	cl_dlight_t* AllocDynamicSpotlight( Int32 key, Int32 subkey, bool isstatic, bool noshadow, cl_entity_t *pentity );

	// Returns the projective shadowmap size
	Int32 GetShadowmapSize( void ) const;
	// Returns the cubemap shadowmap size
	Int32 GetCubeShadowmapSize( void ) const;
	// Returns the dynamic light list
	CLinkedList<cl_dlight_t*>& GetLightList( void ) { return m_dlightsList; }

	// Releases any dynlights tied to an entity
	void ReleaseEntityDynamicLights( entindex_t entindex );

	// Returns directional sunlight shadowmap resolution
	Int32 GetCSMResolution( void ) const;
	// Returns directional sunlight shadowmap struct
	shadowmap_t* GetCSMShadowMap( void ) { return m_pCSMShadowMap; }
	// Returns directional sunlight matrix
	const Float* GetCSMMatrix( void ) const { return m_csmMatrix; }
	// Returns directional sunlight origin in world space
	const Vector& GetCSMLightOrigin( void ) const { return m_csmWorldOrigin; }
	// Returns directional sunlight direction
	const Vector& GetCSMSunDir( void ) const { return m_csmSunDir; }

private:


	// Clears shadow maps
	void ClearShadowMaps( void );
	// Updates static lights
	void UpdateShadowingLights( void );

	// Allocates a cubemap shadowmap
	shadowmap_t *AllocCubemapShadowMap( bool allocblitmap );
	// Allocates a projective shadowmap
	shadowmap_t *AllocProjectiveShadowMap( bool allocblitmap );

	// Frees a dynamic light
	void FreeDynamicLight( cl_dlight_t* pdlight, bool ignoreStatic = false );
	// Sets up a dynamic light's pointers
	void SetupLight( cl_dlight_t* pdlight, bool spotlight, bool noshadow, bool isstatic, cl_entity_t *pentity );

	// Creates a projective FBO
	bool CreateProjectiveFBO( shadowmap_t& shadowmap );
	// Creates a cubemap FBO
	bool CreateCubemapFBO( shadowmap_t& shadowmap );

	// Creates blit FBOs for shadowmap
	bool CreateShadowmapBlitFBOs( shadowmap_t& shadowmap, Uint32 shadowmapSize, Uint32 numFBO );
	// Creates blit FBOs for shadowmap
	void ReleaseShadowmapBlitFBOs( shadowmap_t& shadowmap );

	// Blits shadowmaps for a dynamic light
	void BlitDynamicLightShadowMaps( cl_dlight_t* pdlight );

	// Initializes the FBOs
	bool InitFBOs( void );
	// Deletes all FBO objects
	void DeleteFBOs( void );
	// Checks FBO sizes for any changes
	bool CheckFBOs( void );

	// Returns a dynamic light by it's key and subkey
	cl_dlight_t* GetLightByKey( Int32 key, Int32 subkey );

	// Draws shadowmap passes for a light
	bool DrawShadowMapPasses( cl_dlight_t *dl, cl_entity_t** pvisents, Int32 numentities, bool isfinal );

private:
	// Draws a projective shadow pass
	bool DrawProjectivePass( cl_dlight_t *dl, cl_entity_t** pvisents, Int32 numentities, bool isfinal );
	// Draws single directional sunlight shadow pass
	bool DrawCSMPass( void );
	// Draws a cubemap shadow pass
	bool DrawCubemapPass( cl_dlight_t *dl, Vector vangles, Int32 index, cl_entity_t** pvisents, Int32 numentities, bool isfinal );
	// Tell if static shadows need to be redrawn
	static bool ShouldRedrawShadowMap( cl_dlight_t *dl, dlight_sceneinfo_t* psceneinfo, bool isstatic );

private:
	// VSM shader
	class CGLSLShader *m_pVSMShader;
	// VSM VBO object
	class CVBO *m_pVSMVBO;

	// Attribs for shader
	vsm_shader_attribs m_vsmAttribs;

	// Projective FBO pool
	CLinkedList<shadowmap_t*> m_projectivePoolList;

	// Cubemap FBO pool
	CLinkedList<shadowmap_t*> m_cubemapPoolList;

	// Blur FBO for projective lights
	fbobind_t m_blurFBO;
	fbobind_t m_renderFBO;

	// Blur FBO for cubemap lights
	fbobind_t m_cubeBlurFBO;
	fbobind_t m_cubeRenderFBO;

	// Current shadow map sizes
	Int32 m_shadowmapSize;
	Int32 m_cubeShadowmapSize;

	// Directional sunlight (CSM) shadowmap
	shadowmap_t* m_pCSMShadowMap;
	Int32 m_csmResolution;
	Float m_csmMatrix[16];
	Vector m_csmWorldOrigin;
	Vector m_csmSunDir;

	// Linkest list of dynamic lights
	CLinkedList<cl_dlight_t*> m_dlightsList;

private:
	// Controls projective shadowmap size
	CCVar* m_pCvarShadowmapSize;
	// Controls cubemap shadowmap size
	CCVar* m_pCvarCubeShadowmapSize;
	// Controls whether we use shadowmap blitting
	CCVar* m_pCvarShadowmapBlit;
	// Controls directional sunlight shadowmap
	CCVar* m_pCvarCSM;
	// Controls directional sunlight shadowmap resolution
	CCVar* m_pCvarCSMRes;
};
extern CDynamicLightManager gDynamicLights;

extern bool DL_IsLightVisible( const CFrustum& mainFrustum, const Vector& mins, const Vector& maxs, cl_dlight_t* pdl );
extern bool DL_CanShadow( const cl_dlight_t *dl );
extern void R_CheckShadowmapSizeCvarCallBack( CCVar* pCVar );

#endif