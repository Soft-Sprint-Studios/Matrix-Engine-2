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
#include "includes.h"
#include "r_vbo.h"
#include "r_glsl.h"
#include "r_video.h"
#include "r_bsp.h"
#include "r_common.h"
#include "r_main.h"
#include "texturemanager.h"
#include "cl_entity.h"
#include "cache_model.h"
#include "brushmodel.h"
#include "system.h"
#include "enginestate.h"
#include "cl_main.h"
#include "cl_utils.h"
#include "file.h"

#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"

// Class definition
CVideoManager gVideoManager;

//=============================================
//
//=============================================
CVideoManager::CVideoManager(void) :
	m_pCurrentVideo(nullptr),
	m_pShader(nullptr),
	m_pVBO(nullptr)
{
}

//=============================================
//
//=============================================
CVideoManager::~CVideoManager(void)
{
	ClearGL();
	ClearGame();
}

//=============================================
//
//=============================================
bool CVideoManager::InitGL(void)
{
	if (!m_pShader)
	{
		Int32 shaderFlags = CGLSLShader::FL_GLSL_BINARY_SHADER_OPS;

		m_pShader = new CGLSLShader(FL_GetInterface(), "video.bss", shaderFlags, VID_ShaderCompileCallback);
		if (m_pShader->HasError())
		{
			Sys_ErrorPopup("%s - Failed to compile shader: %s.", __FUNCTION__, m_pShader->GetError());
			return false;
		}

		m_attribs.a_vertex = m_pShader->InitAttribute("in_position", 4, GL_FLOAT, sizeof(video_vertex_t), OFFSET(video_vertex_t, origin));
		m_attribs.a_texcoord = m_pShader->InitAttribute("in_texcoord", 2, GL_FLOAT, sizeof(video_vertex_t), OFFSET(video_vertex_t, texcoord));

		if (!R_CheckShaderVertexAttribute(m_attribs.a_vertex, "in_position", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderVertexAttribute(m_attribs.a_texcoord, "in_texcoord", m_pShader, Sys_ErrorPopup))
			return false;

		m_attribs.u_fogcolor = m_pShader->InitUniform("fogcolor", CGLSLShader::UNIFORM_FLOAT3);
		m_attribs.u_fogparams = m_pShader->InitUniform("fogparams", CGLSLShader::UNIFORM_FLOAT2);
		m_attribs.u_projection = m_pShader->InitUniform("projection", CGLSLShader::UNIFORM_MATRIX4);
		m_attribs.u_modelview = m_pShader->InitUniform("modelview", CGLSLShader::UNIFORM_MATRIX4);
		m_attribs.u_texture_y = m_pShader->InitUniform("textureY", CGLSLShader::UNIFORM_SAMPLER2D);
		m_attribs.u_texture_cb = m_pShader->InitUniform("textureCb", CGLSLShader::UNIFORM_SAMPLER2D);
		m_attribs.u_texture_cr = m_pShader->InitUniform("textureCr", CGLSLShader::UNIFORM_SAMPLER2D);

		if (!R_CheckShaderUniform(m_attribs.u_fogcolor, "fogcolor", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_fogparams, "fogparams", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_projection, "projection", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_modelview, "modelview", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_texture_y, "textureY", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_texture_cb, "textureCb", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_texture_cr, "textureCr", m_pShader, Sys_ErrorPopup))
			return false;

		m_attribs.d_fog = m_pShader->GetDeterminatorIndex("fog");
		if (!R_CheckShaderDeterminator(m_attribs.d_fog, "fog", m_pShader, Sys_ErrorPopup))
			return false;
	}

	if (m_pVBO)
	{
		m_pVBO->RebindGL();
		m_pShader->SetVBO(m_pVBO);
	}

	return true;
}

//=============================================
//
//=============================================
void CVideoManager::ClearGL(void)
{
	if (m_pShader)
	{
		delete m_pShader;
		m_pShader = nullptr;
	}

	if (m_pVBO)
	{
		m_pVBO->ClearGL();
	}
}

//=============================================
//
//=============================================
bool CVideoManager::InitGame(void)
{
	return true;
}

//=============================================
//
//=============================================
void CVideoManager::ClearGame(void)
{
	if (!m_videosArray.empty())
	{
		for (Uint32 i = 0; i < m_videosArray.size(); i++)
		{
			if (m_videosArray[i]->pplm)
			{
				plm_destroy(m_videosArray[i]->pplm);
				m_videosArray[i]->pplm = nullptr;
			}

			if (m_videosArray[i]->pfile_data)
			{
				FL_FreeFile(m_videosArray[i]->pfile_data);
				m_videosArray[i]->pfile_data = nullptr;
			}

			delete m_videosArray[i];
		}

		m_videosArray.clear();
	}

	if (m_pShader)
	{
		m_pShader->SetVBO(nullptr);
		m_pShader->ResetShader();
	}

	if (m_pVBO)
	{
		delete m_pVBO;
		m_pVBO = nullptr;
	}
}

//=============================================
//
//=============================================
void CVideoManager::AllocNewVideo(cl_entity_t* pentity)
{
	const brushmodel_t* pbrushmodel = pentity->pmodel->getBrushmodel();
	if (!pbrushmodel || pbrushmodel->nummodelsurfaces == 0)
		return;

	if (!m_pVBO)
	{
		m_pVBO = new CVBO(true, false);
		m_pShader->SetVBO(m_pVBO);
	}

	cl_video_t* pvideo = new cl_video_t;
	m_videosArray.push_back(pvideo);

	pvideo->mins = NULL_MINS;
	pvideo->maxs = NULL_MAXS;

	Uint32 totalNumVerts = 0;
	for (Uint32 i = 0; i < pbrushmodel->nummodelsurfaces; i++)
	{
		msurface_t* psurf = &pbrushmodel->psurfaces[pbrushmodel->firstmodelsurface + i];
		if (psurf->numedges < 3)
			continue;

		totalNumVerts += 3 + (psurf->numedges - 3) * 3;

		for (Uint32 j = 0; j < psurf->numedges; j++)
		{
			Vector vertexpos;
			Int32 e_index = ens.pworld->psurfedges[psurf->firstedge + j];
			if (e_index > 0)
			{
				Math::VectorCopy(ens.pworld->pvertexes[ens.pworld->pedges[e_index].vertexes[0]].origin, vertexpos);
			}
			else
			{
				Math::VectorCopy(ens.pworld->pvertexes[ens.pworld->pedges[-e_index].vertexes[1]].origin, vertexpos);
			}

			for (Uint32 k = 0; k < 3; k++)
			{
				if (pvideo->mins[k] > vertexpos[k])
				{
					pvideo->mins[k] = vertexpos[k];
				}

				if (pvideo->maxs[k] < vertexpos[k])
				{
					pvideo->maxs[k] = vertexpos[k];
				}
			}
		}
	}

	if (totalNumVerts == 0)
		return;

	pvideo->pentity = pentity;
	entity_extrainfo_t* pinfo = CL_GetEntityExtraData(pentity);
	pinfo->pvideodata = pvideo;

	pvideo->origin[0] = (pvideo->mins[0] + pvideo->maxs[0]) * 0.5f;
	pvideo->origin[1] = (pvideo->mins[1] + pvideo->maxs[1]) * 0.5f;
	pvideo->origin[2] = (pvideo->mins[2] + pvideo->maxs[2]) * 0.5f;
	pvideo->psurface = &pbrushmodel->psurfaces[pbrushmodel->firstmodelsurface];

	CString videoPath;
	if (pentity->curstate.skin > 0)
	{
		videoPath << "media/video_" << pentity->curstate.skin << ".mpg";
	}

	pvideo->filename = videoPath;

	Uint32 fileSize = 0;
	pvideo->pfile_data = FL_LoadFile(pvideo->filename.c_str(), &fileSize);

	if (pvideo->pfile_data && fileSize > 0)
	{
		pvideo->pplm = plm_create_with_memory(const_cast<uint8_t*>(pvideo->pfile_data), fileSize, FALSE);
	}

	if (pvideo->pplm)
	{
		plm_set_loop(pvideo->pplm, TRUE);
		plm_set_audio_enabled(pvideo->pplm, FALSE);

		pvideo->width = plm_get_width(pvideo->pplm);
		pvideo->height = plm_get_height(pvideo->pplm);

		Uint32 lumaWidth = ((pvideo->width + 15) / 16) * 16;
		Uint32 lumaHeight = ((pvideo->height + 15) / 16) * 16;

		Uint32 chromaWidth = lumaWidth / 2;
		Uint32 chromaHeight = lumaHeight / 2;

		CTextureManager* pTextureManager = CTextureManager::GetInstance();

		pvideo->ptexture_y = pTextureManager->GenTextureIndex(RS_GAME_LEVEL);
		glBindTexture(GL_TEXTURE_2D, pvideo->ptexture_y->gl_index);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, lumaWidth, lumaHeight, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

		pvideo->ptexture_cb = pTextureManager->GenTextureIndex(RS_GAME_LEVEL);
		glBindTexture(GL_TEXTURE_2D, pvideo->ptexture_cb->gl_index);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, chromaWidth, chromaHeight, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

		pvideo->ptexture_cr = pTextureManager->GenTextureIndex(RS_GAME_LEVEL);
		glBindTexture(GL_TEXTURE_2D, pvideo->ptexture_cr->gl_index);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, chromaWidth, chromaHeight, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

		glBindTexture(GL_TEXTURE_2D, 0);
	}
	else
	{
		Con_Printf("Failed to load MPEG video file: '%s'\n", pvideo->filename.c_str());
	}

	video_vertex_t* pvertexes = new video_vertex_t[totalNumVerts];
	Uint32 dstvertindex = 0;

	for (Uint32 s = 0; s < pbrushmodel->nummodelsurfaces; s++)
	{
		msurface_t* psurf = &pbrushmodel->psurfaces[pbrushmodel->firstmodelsurface + s];
		if (psurf->numedges < 3)
			continue;

		mtexinfo_t* ptexinfo = psurf->ptexinfo;
		Vector vertexes[3];
		Uint32 srcvertindex = 0;

		for (Uint32 i = 0; i < 3; i++)
		{
			Vector vertexpos;
			Int32 e_index = ens.pworld->psurfedges[psurf->firstedge + srcvertindex];
			if (e_index > 0)
			{
				Math::VectorCopy(ens.pworld->pvertexes[ens.pworld->pedges[e_index].vertexes[0]].origin, vertexpos);
			}
			else
			{
				Math::VectorCopy(ens.pworld->pvertexes[ens.pworld->pedges[-e_index].vertexes[1]].origin, vertexpos);
			}

			Math::VectorCopy(vertexpos, vertexes[i]);

			for (Uint32 j = 0; j < 3; j++)
			{
				pvertexes[dstvertindex].origin[j] = vertexes[i][j];
			}

			pvertexes[dstvertindex].origin[3] = 1.0;

			if (ptexinfo && psurf->extents[0] > 0 && psurf->extents[1] > 0)
			{
				Float u = Math::DotProduct(vertexpos, ptexinfo->vecs[0]) + ptexinfo->vecs[0][3];
				Float v = Math::DotProduct(vertexpos, ptexinfo->vecs[1]) + ptexinfo->vecs[1][3];

				u = (u - psurf->texturemins[0]) / static_cast<Float>(psurf->extents[0]);
				v = (v - psurf->texturemins[1]) / static_cast<Float>(psurf->extents[1]);

				Uint32 lumaWidth = ((pvideo->width + 15) / 16) * 16;
				Uint32 lumaHeight = ((pvideo->height + 15) / 16) * 16;

				pvertexes[dstvertindex].texcoord[0] = u * (static_cast<Float>(pvideo->width) / static_cast<Float>(lumaWidth));
				pvertexes[dstvertindex].texcoord[1] = v * (static_cast<Float>(pvideo->height) / static_cast<Float>(lumaHeight));
			}

			dstvertindex++;
			srcvertindex++;
		}

		for (Uint32 i = 0; i < (psurf->numedges - 3); i++)
		{
			Vector vertexpos;
			Int32 e_index = ens.pworld->psurfedges[psurf->firstedge + srcvertindex];
			if (e_index > 0)
			{
				Math::VectorCopy(ens.pworld->pvertexes[ens.pworld->pedges[e_index].vertexes[0]].origin, vertexpos);
			}
			else
			{
				Math::VectorCopy(ens.pworld->pvertexes[ens.pworld->pedges[-e_index].vertexes[1]].origin, vertexpos);
			}

			Math::VectorCopy(vertexes[2], vertexes[1]);
			Math::VectorCopy(vertexpos, vertexes[2]);

			for (Uint32 j = 0; j < 3; j++)
			{
				for (Uint32 k = 0; k < 3; k++)
				{
					pvertexes[dstvertindex].origin[k] = vertexes[j][k];
				}

				pvertexes[dstvertindex].origin[3] = 1.0;

				if (ptexinfo && psurf->extents[0] > 0 && psurf->extents[1] > 0)
				{
					Float u = Math::DotProduct(vertexes[j], ptexinfo->vecs[0]) + ptexinfo->vecs[0][3];
					Float v = Math::DotProduct(vertexes[j], ptexinfo->vecs[1]) + ptexinfo->vecs[1][3];

					u = (u - psurf->texturemins[0]) / static_cast<Float>(psurf->extents[0]);
					v = (v - psurf->texturemins[1]) / static_cast<Float>(psurf->extents[1]);

					Uint32 lumaWidth = ((pvideo->width + 15) / 16) * 16;
					Uint32 lumaHeight = ((pvideo->height + 15) / 16) * 16;

					pvertexes[dstvertindex].texcoord[0] = u * (static_cast<Float>(pvideo->width) / static_cast<Float>(lumaWidth));
					pvertexes[dstvertindex].texcoord[1] = v * (static_cast<Float>(pvideo->height) / static_cast<Float>(lumaHeight));
				}

				dstvertindex++;
			}

			srcvertindex++;
		}
	}

	pvideo->start_vertex = m_pVBO->GetVBOSize() / sizeof(video_vertex_t);
	pvideo->num_vertexes = totalNumVerts;

	m_pVBO->Append(pvertexes, sizeof(video_vertex_t) * totalNumVerts, nullptr, 0);
	delete[] pvertexes;
}

//=============================================
//
//=============================================
bool CVideoManager::DrawVideos(void)
{
	if (m_videosArray.empty())
		return true;

	if (!m_pShader->EnableShader())
	{
		Sys_ErrorPopup("Shader error: %s.", m_pShader->GetError());
		return false;
	}

	m_pShader->EnableAttribute(m_attribs.a_vertex);
	m_pShader->EnableAttribute(m_attribs.a_texcoord);

	bool result = true;
	if (rns.fog.settings.active)
	{
		result = m_pShader->SetDeterminator(m_attribs.d_fog, 1);
		m_pShader->SetUniform3f(m_attribs.u_fogcolor, rns.fog.settings.color[0], rns.fog.settings.color[1], rns.fog.settings.color[2]);
		m_pShader->SetUniform2f(m_attribs.u_fogparams, rns.fog.settings.end, 1.0f / (static_cast<Float>(rns.fog.settings.end) - static_cast<Float>(rns.fog.settings.start)));
	}
	else
	{
		result = m_pShader->SetDeterminator(m_attribs.d_fog, 0);
	}

	if (!result)
	{
		Sys_ErrorPopup("Shader error: %s.", m_pShader->GetError());
		m_pShader->DisableShader();
		return false;
	}

	m_pShader->SetUniform1i(m_attribs.u_texture_y, 0);
	m_pShader->SetUniform1i(m_attribs.u_texture_cb, 1);
	m_pShader->SetUniform1i(m_attribs.u_texture_cr, 2);

	m_pShader->SetUniformMatrix4fv(m_attribs.u_modelview, rns.view.modelview.GetMatrix());
	m_pShader->SetUniformMatrix4fv(m_attribs.u_projection, rns.view.projection.GetMatrix());

	Double curtime = rns.time;

	for (Uint32 i = 0; i < rns.objects.numvisents; i++)
	{
		cl_entity_t* pentity = rns.objects.pvisents_unsorted[i];
		if (pentity->curstate.rendertype != RT_VIDEO)
			continue;

		entity_extrainfo_t* pinfo = CL_GetEntityExtraData(pentity);
		if (!pinfo || !pinfo->pvideodata)
			continue;

		m_pCurrentVideo = pinfo->pvideodata;

		if (rns.view.frustum.CullBBox(m_pCurrentVideo->mins, m_pCurrentVideo->maxs))
			continue;

		if (m_pCurrentVideo->trigger_count != pentity->curstate.iuser1)
		{
			m_pCurrentVideo->trigger_count = pentity->curstate.iuser1;
			if (m_pCurrentVideo->pplm)
			{
				plm_rewind(m_pCurrentVideo->pplm);
				m_pCurrentVideo->time = 0.0;
				m_pCurrentVideo->next_frame_time = 0.0;
				m_pCurrentVideo->has_finished = false;
			}
		}

		bool isLooping = (pentity->curstate.body & (1 << 0)) != 0;

		if (m_pCurrentVideo->pplm && m_pCurrentVideo->ptexture_y && !m_pCurrentVideo->has_finished)
		{
			Double frametime = curtime - m_pCurrentVideo->last_time;

			m_pCurrentVideo->last_time = curtime;
			m_pCurrentVideo->time += frametime;

			Double fps = plm_get_framerate(m_pCurrentVideo->pplm);

			if (m_pCurrentVideo->time >= m_pCurrentVideo->next_frame_time)
			{
				plm_frame_t* pframe = plm_decode_video(m_pCurrentVideo->pplm);
				if (!pframe)
				{
					if (isLooping)
					{
						plm_rewind(m_pCurrentVideo->pplm);
						m_pCurrentVideo->time = 0.0;
						m_pCurrentVideo->next_frame_time = 0.0;
						pframe = plm_decode_video(m_pCurrentVideo->pplm);
					}
					else
					{
						m_pCurrentVideo->has_finished = true;
					}
				}

				if (pframe)
				{
					m_pCurrentVideo->next_frame_time += 1.0 / fps;

					R_Bind2DTexture(GL_TEXTURE0, m_pCurrentVideo->ptexture_y->gl_index);
					glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, pframe->y.width, pframe->y.height, GL_RED, GL_UNSIGNED_BYTE, pframe->y.data);

					R_Bind2DTexture(GL_TEXTURE1, m_pCurrentVideo->ptexture_cb->gl_index);
					glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, pframe->cb.width, pframe->cb.height, GL_RED, GL_UNSIGNED_BYTE, pframe->cb.data);

					R_Bind2DTexture(GL_TEXTURE2, m_pCurrentVideo->ptexture_cr->gl_index);
					glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, pframe->cr.width, pframe->cr.height, GL_RED, GL_UNSIGNED_BYTE, pframe->cr.data);
				}
			}
			else
			{
				R_Bind2DTexture(GL_TEXTURE0, m_pCurrentVideo->ptexture_y->gl_index);
				R_Bind2DTexture(GL_TEXTURE1, m_pCurrentVideo->ptexture_cb->gl_index);
				R_Bind2DTexture(GL_TEXTURE2, m_pCurrentVideo->ptexture_cr->gl_index);
			}
		}
		else if (m_pCurrentVideo->ptexture_y)
		{
			R_Bind2DTexture(GL_TEXTURE0, m_pCurrentVideo->ptexture_y->gl_index);
			R_Bind2DTexture(GL_TEXTURE1, m_pCurrentVideo->ptexture_cb->gl_index);
			R_Bind2DTexture(GL_TEXTURE2, m_pCurrentVideo->ptexture_cr->gl_index);
		}

		R_ValidateShader(m_pShader);

		m_pShader->DrawArrays(GL_TRIANGLES, m_pCurrentVideo->start_vertex, m_pCurrentVideo->num_vertexes);

		const brushmodel_t* pbrushmodel = pentity->pmodel->getBrushmodel();
		for (Uint32 j = 0; j < pbrushmodel->nummodelsurfaces; j++)
		{
			msurface_t* psurf = &pbrushmodel->psurfaces[pbrushmodel->firstmodelsurface + j];
			psurf->visframe = cls.framecount;
		}
	}

	m_pShader->DisableShader();

	// Clear any binds
	R_ClearBinds();

	return true;
}