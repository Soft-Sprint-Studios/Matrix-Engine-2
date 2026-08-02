/*
 * MIT License
 *
 * Copyright (c) 2025-2026 Soft Sprint Studios
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef R_VIDEO_H
#define R_VIDEO_H

#include "ref_params.h"

struct cl_entity_t;
struct msurface_t;
struct en_texalloc_t;
struct plm_t;

struct cl_video_t
{
	cl_video_t() :
		pentity(nullptr),
		psurface(nullptr),
		start_vertex(0),
		num_vertexes(0),
		ptexture_y(nullptr),
		ptexture_cb(nullptr),
		ptexture_cr(nullptr),
		pplm(nullptr),
		pfile_data(nullptr),
		width(0),
		height(0),
		last_time(0),
		time(0),
		next_frame_time(0),
		trigger_count(0),
		has_finished(false)
		{}

	cl_entity_t* pentity;

	Vector mins;
	Vector maxs;

	Vector origin;
	msurface_t* psurface;

	Uint32 start_vertex;
	Uint32 num_vertexes;

	en_texalloc_t* ptexture_y;
	en_texalloc_t* ptexture_cb;
	en_texalloc_t* ptexture_cr;

	Double time;
	Double next_frame_time;
	Uint32 trigger_count;
	bool has_finished;

	plm_t* pplm;
	const byte* pfile_data;
	Uint32 width;
	Uint32 height;
	Double last_time;
	CString filename;
};

struct video_vertex_t
{
	video_vertex_t()
	{
		memset(origin, 0, sizeof(origin));
		memset(texcoord, 0, sizeof(texcoord));
		memset(padding, 0, sizeof(padding));
	}

	vec4_t origin;
	Float texcoord[2];
	byte padding[8];
};

struct video_attribs
{
	video_attribs() :
		a_vertex(CGLSLShader::PROPERTY_UNAVAILABLE),
		a_texcoord(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_modelview(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_projection(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_texture_y(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_texture_cb(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_texture_cr(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_fogcolor(CGLSLShader::PROPERTY_UNAVAILABLE),
		u_fogparams(CGLSLShader::PROPERTY_UNAVAILABLE),
		d_fog(CGLSLShader::PROPERTY_UNAVAILABLE)
		{}

	Int32 a_vertex;
	Int32 a_texcoord;

	Int32 u_modelview;
	Int32 u_projection;

	Int32 u_texture_y;
	Int32 u_texture_cb;
	Int32 u_texture_cr;

	Int32 u_fogcolor;
	Int32 u_fogparams;

	Int32 d_fog;
};

/*
====================
CVideoManager

====================
*/
class CVideoManager
{
public:
	CVideoManager(void);
	~CVideoManager(void);

public:
	// Initializes the class
	bool InitGL(void);
	// Shuts down the class
	void ClearGL(void);

	// Clears the game
	bool InitGame(void);
	// Clears the game
	void ClearGame(void);

	// Draw video entities
	bool DrawVideos(void);

	// Allocates a new video surface
	void AllocNewVideo(cl_entity_t* pentity);

private:
	// Currently managed video
	cl_video_t* m_pCurrentVideo;
	// Array of video entities
	CArray<cl_video_t*> m_videosArray;

private:
	// GLSL shader
	class CGLSLShader* m_pShader;
	// VBO
	class CVBO* m_pVBO;

	// Video shader attributes
	video_attribs m_attribs;
};

extern CVideoManager gVideoManager;
#endif