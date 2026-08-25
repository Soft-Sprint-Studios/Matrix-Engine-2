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
#include "r_glass.h"
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
#include "r_rttcache.h"

CGlassManager gGlassManager;

//=============================================
//
//=============================================
CGlassManager::CGlassManager(void) :
	m_pCurrentGlass(nullptr),
	m_pShader(nullptr),
	m_pVBO(nullptr)
{
}

//=============================================
//
//=============================================
CGlassManager::~CGlassManager(void)
{
	ClearGL();
	ClearGame();
}

//=============================================
//
//=============================================
bool CGlassManager::Init(void)
{
	return true;
}

//=============================================
//
//=============================================
void CGlassManager::Shutdown(void)
{
	ClearGL();
	ClearGame();
}

//=============================================
//
//=============================================
bool CGlassManager::InitGL(void)
{
	if (!m_pShader)
	{
		Int32 shaderFlags = CGLSLShader::FL_GLSL_BINARY_SHADER_OPS;
		m_pShader = new CGLSLShader(FL_GetInterface(), "glass.bss", shaderFlags, VID_ShaderCompileCallback);
		if (m_pShader->HasError())
		{
			Sys_ErrorPopup("%s - Failed to compile shader: %s.", __FUNCTION__, m_pShader->GetError());
			return false;
		}

		m_attribs.a_vertex = m_pShader->InitAttribute("in_position", 4, GL_FLOAT, sizeof(glass_vertex_t), OFFSET(glass_vertex_t, origin));
		m_attribs.a_tangent = m_pShader->InitAttribute("in_tangent", 3, GL_FLOAT, sizeof(glass_vertex_t), OFFSET(glass_vertex_t, tangent));
		m_attribs.a_binormal = m_pShader->InitAttribute("in_binormal", 3, GL_FLOAT, sizeof(glass_vertex_t), OFFSET(glass_vertex_t, binormal));
		m_attribs.a_texcoord = m_pShader->InitAttribute("in_texcoord", 2, GL_FLOAT, sizeof(glass_vertex_t), OFFSET(glass_vertex_t, texcoord));

		if (!R_CheckShaderVertexAttribute(m_attribs.a_vertex, "in_position", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderVertexAttribute(m_attribs.a_tangent, "in_tangent", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderVertexAttribute(m_attribs.a_binormal, "in_binormal", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderVertexAttribute(m_attribs.a_texcoord, "in_texcoord", m_pShader, Sys_ErrorPopup))
			return false;

		m_attribs.u_fogcolor = m_pShader->InitUniform("fogcolor", CGLSLShader::UNIFORM_FLOAT3);
		m_attribs.u_fogparams = m_pShader->InitUniform("fogparams", CGLSLShader::UNIFORM_FLOAT2);
		m_attribs.u_projection = m_pShader->InitUniform("projection", CGLSLShader::UNIFORM_MATRIX4);
		m_attribs.u_modelview = m_pShader->InitUniform("modelview", CGLSLShader::UNIFORM_MATRIX4);
		m_attribs.u_normalmatrix = m_pShader->InitUniform("normalmatrix", CGLSLShader::UNIFORM_MATRIX4);
		m_attribs.u_refract_texture = m_pShader->InitUniform("s_refractTex", CGLSLShader::UNIFORM_SAMPLER2D);
		m_attribs.u_normal_texture = m_pShader->InitUniform("s_normalMap", CGLSLShader::UNIFORM_SAMPLER2D);
		m_attribs.u_glass_params = m_pShader->InitUniform("u_glassParams", CGLSLShader::UNIFORM_FLOAT4);

		if (!R_CheckShaderUniform(m_attribs.u_fogcolor, "fogcolor", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_fogparams, "fogparams", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_projection, "projection", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_modelview, "modelview", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_normalmatrix, "normalmatrix", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_refract_texture, "s_refractTex", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_normal_texture, "s_normalMap", m_pShader, Sys_ErrorPopup)
			|| !R_CheckShaderUniform(m_attribs.u_glass_params, "u_glassParams", m_pShader, Sys_ErrorPopup))
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
void CGlassManager::ClearGL(void)
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
bool CGlassManager::InitGame(void)
{
	return true;
}

//=============================================
//
//=============================================
void CGlassManager::ClearGame(void)
{
	if (!m_glassArray.empty())
	{
		for (Uint32 i = 0; i < m_glassArray.size(); i++)
		{
			delete m_glassArray[i];
		}
		m_glassArray.clear();
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
void CGlassManager::AllocNewGlass(cl_entity_t* pentity)
{
	const brushmodel_t* pbrushmodel = pentity->pmodel->getBrushmodel();
	if (!pbrushmodel || pbrushmodel->nummodelsurfaces == 0)
		return;

	if (!m_pVBO)
	{
		m_pVBO = new CVBO(true, false);
		m_pShader->SetVBO(m_pVBO);
	}

	cl_glass_t* pglass = new cl_glass_t;
	m_glassArray.push_back(pglass);

	pglass->mins = NULL_MINS;
	pglass->maxs = NULL_MAXS;

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
				Math::VectorCopy(ens.pworld->pvertexes[ens.pworld->pedges[e_index].vertexes[0]].origin, vertexpos);
			else
				Math::VectorCopy(ens.pworld->pvertexes[ens.pworld->pedges[-e_index].vertexes[1]].origin, vertexpos);

			for (Uint32 k = 0; k < 3; k++)
			{
				if (pglass->mins[k] > vertexpos[k])
					pglass->mins[k] = vertexpos[k];
				if (pglass->maxs[k] < vertexpos[k])
					pglass->maxs[k] = vertexpos[k];
			}
		}
	}

	if (totalNumVerts == 0)
		return;

	pglass->pentity = pentity;
	entity_extrainfo_t* pinfo = CL_GetEntityExtraData(pentity);
	pinfo->pglassdata = pglass;

	pglass->origin[0] = (pglass->mins[0] + pglass->maxs[0]) * 0.5f;
	pglass->origin[1] = (pglass->mins[1] + pglass->maxs[1]) * 0.5f;
	pglass->origin[2] = (pglass->mins[2] + pglass->maxs[2]) * 0.5f;
	pglass->psurface = &pbrushmodel->psurfaces[pbrushmodel->firstmodelsurface];
	pglass->amount = pentity->curstate.scale > 0.0f ? pentity->curstate.scale : 0.05f;

	CString normalPath;
	if (pentity->curstate.iuser1 > 0)
		normalPath << "general/glass_" << pentity->curstate.iuser1 << ".dds";

	CTextureManager* pTextureManager = CTextureManager::GetInstance();
	en_texture_t* pTex = pTextureManager->LoadTexture(normalPath.c_str(), RS_GAME_LEVEL);
	if (!pTex)
		pTex = pTextureManager->GetDummyTexture();

	pglass->pnormalmap = pTex->palloc;

	glass_vertex_t* pvertexes = new glass_vertex_t[totalNumVerts];
	Uint32 dstvertindex = 0;

	for (Uint32 s = 0; s < pbrushmodel->nummodelsurfaces; s++)
	{
		msurface_t* psurf = &pbrushmodel->psurfaces[pbrushmodel->firstmodelsurface + s];
		if (psurf->numedges < 3)
			continue;

		mtexinfo_t* ptexinfo = psurf->ptexinfo;
		Vector tangent, binormal;
		Math::VectorCopy(ptexinfo->vecs[0], tangent);
		Math::VectorNormalize(tangent);
		Math::VectorCopy(ptexinfo->vecs[1], binormal);
		Math::VectorNormalize(binormal);

		Vector vertexes[3];
		Uint32 srcvertindex = 0;

		for (Uint32 i = 0; i < 3; i++)
		{
			Vector vertexpos;
			Int32 e_index = ens.pworld->psurfedges[psurf->firstedge + srcvertindex];
			if (e_index > 0)
				Math::VectorCopy(ens.pworld->pvertexes[ens.pworld->pedges[e_index].vertexes[0]].origin, vertexpos);
			else
				Math::VectorCopy(ens.pworld->pvertexes[ens.pworld->pedges[-e_index].vertexes[1]].origin, vertexpos);

			Math::VectorCopy(vertexpos, vertexes[i]);

			for (Uint32 j = 0; j < 3; j++)
				pvertexes[dstvertindex].origin[j] = vertexes[i][j];

			pvertexes[dstvertindex].origin[3] = 1.0f;
			pvertexes[dstvertindex].tangent = tangent;
			pvertexes[dstvertindex].binormal = binormal;

			if (ptexinfo && psurf->extents[0] > 0 && psurf->extents[1] > 0)
			{
				Float u = Math::DotProduct(vertexpos, ptexinfo->vecs[0]) + ptexinfo->vecs[0][3];
				Float v = Math::DotProduct(vertexpos, ptexinfo->vecs[1]) + ptexinfo->vecs[1][3];

				pvertexes[dstvertindex].texcoord[0] = u / static_cast<Float>(ptexinfo->ptexture->width);
				pvertexes[dstvertindex].texcoord[1] = v / static_cast<Float>(ptexinfo->ptexture->height);
			}

			dstvertindex++;
			srcvertindex++;
		}

		for (Uint32 i = 0; i < (psurf->numedges - 3); i++)
		{
			Vector vertexpos;
			Int32 e_index = ens.pworld->psurfedges[psurf->firstedge + srcvertindex];
			if (e_index > 0)
				Math::VectorCopy(ens.pworld->pvertexes[ens.pworld->pedges[e_index].vertexes[0]].origin, vertexpos);
			else
				Math::VectorCopy(ens.pworld->pvertexes[ens.pworld->pedges[-e_index].vertexes[1]].origin, vertexpos);

			Math::VectorCopy(vertexes[2], vertexes[1]);
			Math::VectorCopy(vertexpos, vertexes[2]);

			for (Uint32 j = 0; j < 3; j++)
			{
				for (Uint32 k = 0; k < 3; k++)
					pvertexes[dstvertindex].origin[k] = vertexes[j][k];

				pvertexes[dstvertindex].origin[3] = 1.0f;
				pvertexes[dstvertindex].tangent = tangent;
				pvertexes[dstvertindex].binormal = binormal;

				if (ptexinfo && psurf->extents[0] > 0 && psurf->extents[1] > 0)
				{
					Float u = Math::DotProduct(vertexes[j], ptexinfo->vecs[0]) + ptexinfo->vecs[0][3];
					Float v = Math::DotProduct(vertexes[j], ptexinfo->vecs[1]) + ptexinfo->vecs[1][3];

					pvertexes[dstvertindex].texcoord[0] = u / static_cast<Float>(ptexinfo->ptexture->width);
					pvertexes[dstvertindex].texcoord[1] = v / static_cast<Float>(ptexinfo->ptexture->height);
				}

				dstvertindex++;
			}

			srcvertindex++;
		}
	}

	pglass->start_vertex = m_pVBO->GetVBOSize() / sizeof(glass_vertex_t);
	pglass->num_vertexes = totalNumVerts;

	m_pVBO->Append(pvertexes, sizeof(glass_vertex_t) * totalNumVerts, nullptr, 0);
	delete[] pvertexes;
}

//=============================================
//
//=============================================
bool CGlassManager::DrawGlass(void)
{
	if (m_glassArray.empty())
		return true;

	if (!m_pShader->EnableShader())
	{
		Sys_ErrorPopup("Shader error: %s.", m_pShader->GetError());
		return false;
	}

	m_pShader->EnableAttribute(m_attribs.a_vertex);
	m_pShader->EnableAttribute(m_attribs.a_tangent);
	m_pShader->EnableAttribute(m_attribs.a_binormal);
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

	m_pShader->SetUniform1i(m_attribs.u_refract_texture, 0);
	m_pShader->SetUniform1i(m_attribs.u_normal_texture, 1);

	m_pShader->SetUniformMatrix4fv(m_attribs.u_modelview, rns.view.modelview.GetMatrix());
	m_pShader->SetUniformMatrix4fv(m_attribs.u_projection, rns.view.projection.GetMatrix());
	m_pShader->SetUniformMatrix4fv(m_attribs.u_normalmatrix, rns.view.modelview.GetInverse());

	rtt_texture_t* pScreenTexture = gRTTCache.Alloc(rns.screenwidth, rns.screenheight, false);
	R_GrabScreenToTexture(pScreenTexture->palloc, rns.screenwidth, rns.screenheight, false);
	R_Bind2DTexture(GL_TEXTURE0, pScreenTexture->palloc->gl_index);

	for (Uint32 i = 0; i < rns.objects.numvisents; i++)
	{
		cl_entity_t* pentity = rns.objects.pvisents_unsorted[i];
		if (pentity->curstate.rendertype != RT_GLASS)
			continue;

		entity_extrainfo_t* pinfo = CL_GetEntityExtraData(pentity);
		if (!pinfo || !pinfo->pglassdata)
			continue;

		m_pCurrentGlass = pinfo->pglassdata;

		if (rns.view.frustum.CullBBox(m_pCurrentGlass->mins, m_pCurrentGlass->maxs))
			continue;

		if (m_pCurrentGlass->pnormalmap)
			R_Bind2DTexture(GL_TEXTURE1, m_pCurrentGlass->pnormalmap->gl_index);

		m_pShader->SetUniform4f(m_attribs.u_glass_params, m_pCurrentGlass->amount, 0.0f, 0.0f, 0.0f);

		R_ValidateShader(m_pShader);

		m_pShader->DrawArrays(GL_TRIANGLES, m_pCurrentGlass->start_vertex, m_pCurrentGlass->num_vertexes);

		const brushmodel_t* pbrushmodel = pentity->pmodel->getBrushmodel();
		for (Uint32 j = 0; j < pbrushmodel->nummodelsurfaces; j++)
		{
			msurface_t* psurf = &pbrushmodel->psurfaces[pbrushmodel->firstmodelsurface + j];
			psurf->visframe = cls.framecount;
		}
	}

	gRTTCache.Free(pScreenTexture);

	m_pShader->DisableShader();

	R_ClearBinds();
	return true;
}