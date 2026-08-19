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
#include "sv_physics.h"
#include "sv_bulletphysics.h"
#include "sv_main.h"
#include "edictmanager.h"
#include "com_math.h"
#include "brushmodel.h"
#include "modelcache.h"
#include "disptrace.h"
#include "mcdformat.h"
#include "enginestate.h"
#include "system.h"
#include "cvar.h"
#include "console.h"
#include "r_main.h"
#include "r_basicdraw.h"

CCVar* g_pCvarPhysicsDebug = nullptr;

CBulletPhysics gBulletPhysics;

//=============================================
//
//=============================================
CBulletPhysics::CBulletPhysics( void )
{
	m_pCollisionConfiguration = nullptr;
	m_pDispatcher = nullptr;
	m_pOverlappingPairCache = nullptr;
	m_pSolver = nullptr;
	m_pDynamicsWorld = nullptr;

	m_pWorldMesh = nullptr;
	m_pStaticWorldBody = nullptr;
}

//=============================================
//
//=============================================
CBulletPhysics::~CBulletPhysics( void )
{
	Shutdown();
}

//=============================================
//
//=============================================
void CBulletPhysics::Init( void )
{
	g_pCvarPhysicsDebug = gConsole.CreateCVar(CVAR_FLOAT, FL_CV_CLIENT, "physics_debug", "0", "Show wireframe of Bullet physics");

	m_pOverlappingPairCache = new btDbvtBroadphase();

	m_pCollisionConfiguration = new btDefaultCollisionConfiguration();
	m_pDispatcher = new btCollisionDispatcher(m_pCollisionConfiguration);

	m_pSolver = new btSequentialImpulseConstraintSolver();

	m_pDynamicsWorld = new btDiscreteDynamicsWorld(m_pDispatcher, m_pOverlappingPairCache, m_pSolver, m_pCollisionConfiguration);

	m_pDynamicsWorld->setGravity(btVector3(0.0f, 0.0f, -g_psv_gravity->GetValue()));

	m_pDynamicsWorld->setDebugDrawer(&m_debugDrawer);
	m_debugDrawer.setDebugMode(btIDebugDraw::DBG_DrawWireframe);

	m_bodyIds.resize(MAX_SERVER_ENTITIES);
	for (Uint32 i = 0; i < MAX_SERVER_ENTITIES; i++)
		m_bodyIds[i] = nullptr;
}

//=============================================
//
//=============================================
void CBulletPhysics::Shutdown( void )
{
	ClearWorld();

	if (m_pDynamicsWorld)
	{
		delete m_pDynamicsWorld;
		m_pDynamicsWorld = nullptr;

		delete m_pSolver; 
		m_pSolver = nullptr;

		delete m_pOverlappingPairCache;
		m_pOverlappingPairCache = nullptr;

		delete m_pDispatcher; 
		m_pDispatcher = nullptr;

		delete m_pCollisionConfiguration;
		m_pCollisionConfiguration = nullptr;
	}
}

//=============================================
//
//=============================================
void CBulletPhysics::RemoveBody( Uint32 entindex )
{
	btRigidBody* body = m_bodyIds[entindex];
	if (!body) 
		return;

	if (m_pDynamicsWorld)
		m_pDynamicsWorld->removeRigidBody(body);

	if (body->getMotionState())
		delete body->getMotionState();

	if (body->getCollisionShape())
		delete body->getCollisionShape();

	delete body;
	m_bodyIds[entindex] = nullptr;
}

//=============================================
//
//=============================================
void CBulletPhysics::ClearWorld( void )
{
	for (Uint32 i = 0; i < MAX_SERVER_ENTITIES; i++)
	{
		RemoveBody(i);
	}

	if (m_pStaticWorldBody)
	{
		if (m_pDynamicsWorld)
			m_pDynamicsWorld->removeRigidBody(m_pStaticWorldBody);

		if (m_pStaticWorldBody->getMotionState())
			delete m_pStaticWorldBody->getMotionState();

		if (m_pStaticWorldBody->getCollisionShape())
			delete m_pStaticWorldBody->getCollisionShape();

		delete m_pStaticWorldBody;
		m_pStaticWorldBody = nullptr;
	}

	if (m_pWorldMesh)
	{
		delete m_pWorldMesh;
		m_pWorldMesh = nullptr;
	}
}

//=============================================
//
//=============================================
static mvertex_t* GetBSPVertex(brushmodel_t* pmodel, Int32 edgeIndex)
{
	if (edgeIndex >= 0)
		return &pmodel->pvertexes[pmodel->pedges[edgeIndex].vertexes[0]];
	else
		return &pmodel->pvertexes[pmodel->pedges[-edgeIndex].vertexes[1]];
}

//=============================================
//
//=============================================
void CBulletPhysics::SetupStaticWorld( void )
{
	if (!ens.pworld) 
		return;

	if (m_pDynamicsWorld)
		m_pDynamicsWorld->setGravity(btVector3(0.0f, 0.0f, -g_psv_gravity->GetValue()));

	m_pWorldMesh = new btTriangleMesh();

	// World BSP
	mmodel_t* pWorldSubModel = &ens.pworld->psubmodels[0];
	for (Uint32 i = 0; i < pWorldSubModel->numfaces; i++)
	{
		msurface_t* psurf = &ens.pworld->psurfaces[pWorldSubModel->firstface + i];

		if (psurf->flags & SURF_DRAWTURB)
			continue;

		mvertex_t* v0 = GetBSPVertex(ens.pworld, ens.pworld->psurfedges[psurf->firstedge]);

		for (Int32 j = 1; j < psurf->numedges - 1; j++)
		{
			mvertex_t* v1 = GetBSPVertex(ens.pworld, ens.pworld->psurfedges[psurf->firstedge + j]);
			mvertex_t* v2 = GetBSPVertex(ens.pworld, ens.pworld->psurfedges[psurf->firstedge + j + 1]);

			btVector3 pt0(v0->origin.x, v0->origin.y, v0->origin.z);
			btVector3 pt1(v1->origin.x, v1->origin.y, v1->origin.z);
			btVector3 pt2(v2->origin.x, v2->origin.y, v2->origin.z);

			m_pWorldMesh->addTriangle(pt0, pt1, pt2, true);
		}
	}

	// World displacements
	if (g_pWorldDisplacementMCD)
	{
		const mcdbodypart_t* pPart = g_pWorldDisplacementMCD->getBodyPart(0);
		const mcdsubmodel_t* pSub = pPart->getSubmodel(g_pWorldDisplacementMCD, 0);
		const mcdtrimeshtype_t* pMesh = (mcdtrimeshtype_t*)pSub->getTypeData(g_pWorldDisplacementMCD, MCD_COLLISION_TRIANGLES);

		if (pMesh)
		{
			const mcdvertex_t* pVerts = pMesh->getVertexes(g_pWorldDisplacementMCD);
			const mcdtrimeshtriangle_t* pTris = pMesh->getTriangles(g_pWorldDisplacementMCD);

			for (Uint32 i = 0; i < pMesh->numtriangles; i++)
			{
				const mcdtrimeshtriangle_t& t = pTris[i];
				const Vector& v0 = pVerts[t.trivertexes[0]].origin;
				const Vector& v1 = pVerts[t.trivertexes[1]].origin;
				const Vector& v2 = pVerts[t.trivertexes[2]].origin;

				m_pWorldMesh->addTriangle(btVector3(v0.x, v0.y, v0.z), btVector3(v1.x, v1.y, v1.z), btVector3(v2.x, v2.y, v2.z), true);
			}
		}
	}

	// Model collision
	for (Uint32 e = 1; e < gEdicts.GetNbEdicts(); e++)
	{
		edict_t* pedict = gEdicts.GetEdict(e);
		if (pedict->free || pedict->state.modelindex <= 0)
			continue;

		if (pedict->state.movetype == MOVETYPE_PHYSICS)
			continue;

		cache_model_t* pmodel = gModelCache.GetModelByIndex(pedict->state.modelindex);
		if (!pmodel || pmodel->type != MOD_VBM)
			continue;

		const vbmcache_t* pvbmcache = pmodel->getVBMCache();
		if (!pvbmcache || !pvbmcache->pmcdheader)
			continue;

		const mcdheader_t* pMCD = pvbmcache->pmcdheader;
		const mcdbodypart_t* pPart = pMCD->getBodyPart(0);
		if (!pPart)
			continue;

		Uint32 subIdx = (pedict->state.body / pPart->base) % pPart->numsubmodels;
		const mcdsubmodel_t* pSub = pPart->getSubmodel(pMCD, subIdx);
		if (!pSub)
			continue;

		const mcdtrimeshtype_t* pMesh = reinterpret_cast<const mcdtrimeshtype_t*>(pSub->getTypeData(pMCD, MCD_COLLISION_TRIANGLES));
		if (!pMesh || pMesh->numtriangles == 0)
			continue;

		Vector angles = pedict->state.angles;
		angles[PITCH] = -angles[PITCH];

		Float mat[3][4];
		Math::AngleMatrix(angles, mat);
		for (Uint32 j = 0; j < 3; j++)
			mat[j][3] = pedict->state.origin[j];

		const mcdvertex_t* pVerts = pMesh->getVertexes(pMCD);
		const mcdtrimeshtriangle_t* pTris = pMesh->getTriangles(pMCD);

		for (Uint32 i = 0; i < pMesh->numtriangles; i++)
		{
			const mcdtrimeshtriangle_t& t = pTris[i];

			Vector pt0, pt1, pt2;
			Math::VectorTransform(pVerts[t.trivertexes[0]].origin, mat, pt0);
			Math::VectorTransform(pVerts[t.trivertexes[1]].origin, mat, pt1);
			Math::VectorTransform(pVerts[t.trivertexes[2]].origin, mat, pt2);

			m_pWorldMesh->addTriangle(btVector3(pt0.x, pt0.y, pt0.z), btVector3(pt1.x, pt1.y, pt1.z), btVector3(pt2.x, pt2.y, pt2.z), true);
		}
	}

	btBvhTriangleMeshShape* worldShape = new btBvhTriangleMeshShape(m_pWorldMesh, true);
	
	btTransform startTransform;
	startTransform.setIdentity();

	btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
	btRigidBody::btRigidBodyConstructionInfo rbInfo(0.0f, myMotionState, worldShape, btVector3(0, 0, 0));

	rbInfo.m_friction = 0.8f;
	rbInfo.m_restitution = 0.1f;

	m_pStaticWorldBody = new btRigidBody(rbInfo);
	m_pDynamicsWorld->addRigidBody(m_pStaticWorldBody);
}

//=============================================
//
//=============================================
void CBulletPhysics::Frame( double frametime )
{
	if (!m_pDynamicsWorld || frametime <= 0.0)
		return;

	for (Uint32 i = 1; i < gEdicts.GetNbEdicts(); i++)
	{
		edict_t* pedict = gEdicts.GetEdict(i);

		if (pedict->free || pedict->state.solid == SOLID_NOT)
		{
			RemoveBody(i);
			continue;
		}

		if (pedict->state.movetype == MOVETYPE_PHYSICS)
		{
			if (!m_bodyIds[i])
				SyncEntityToPhysics(pedict, i);

			if (m_bodyIds[i])
				ApplyBuoyancy(pedict, m_bodyIds[i]);
		}
		else if (pedict->state.movetype == MOVETYPE_PUSH)
		{
			if (!m_bodyIds[i])
				SyncEntityToPhysics(pedict, i);

			if (m_bodyIds[i])
				SyncEntityToKinematic(pedict, i);
		}
		else if (m_bodyIds[i])
		{
			RemoveBody(i);
		}
	}

	m_pDynamicsWorld->stepSimulation((btScalar)frametime, 10, 1.0f / 60.0f);

	for (Uint32 i = 1; i < gEdicts.GetNbEdicts(); i++)
	{
		edict_t* pedict = gEdicts.GetEdict(i);
		if (!pedict->free && pedict->state.movetype == MOVETYPE_PHYSICS && m_bodyIds[i])
		{
			m_bodyIds[i]->activate(true);
			SyncPhysicsToEntity(pedict, i);
		}
	}

	ProcessContactTouches();
}

//=============================================
//
//=============================================
void CBulletDebugDraw::drawLine(const btVector3& from, const btVector3& to, const btVector3& color)
{
	CBasicDraw* pDraw = CBasicDraw::GetInstance();
	pDraw->Color4f(color.getX(), color.getY(), color.getZ(), 1.0f);
	pDraw->Begin(CBasicDraw::DRAW_LINES);
	pDraw->Vertex3f(from.getX(), from.getY(), from.getZ());
	pDraw->Vertex3f(to.getX(), to.getY(), to.getZ());
	pDraw->End();
}

//=============================================
//
//=============================================
void CBulletPhysics::DrawDebug(void)
{
	if (!m_pDynamicsWorld || !g_pCvarPhysicsDebug || g_pCvarPhysicsDebug->GetValue() < 1.0f)
		return;

	CBasicDraw* pDraw = CBasicDraw::GetInstance();
	if (pDraw->Enable() && pDraw->DisableTexture())
	{
		pDraw->SetProjection(rns.view.projection.GetMatrix());
		pDraw->SetModelview(rns.view.modelview.GetMatrix());

		m_pDynamicsWorld->debugDrawWorld();
		pDraw->Disable();
	}
}

//=============================================
//
//=============================================
bool CBulletPhysics::ConvexSweepTest(const Vector& start, const Vector& end, const Vector& mins, const Vector& maxs, Int32 ignore_ent, trace_t& trace)
{
	if (!m_pDynamicsWorld)
		return false;

	btVector3 halfExtents((maxs.x - mins.x) * 0.5f, (maxs.y - mins.y) * 0.5f, (maxs.z - mins.z) * 0.5f);
	btVector3 localCenter((mins.x + maxs.x) * 0.5f, (mins.y + maxs.y) * 0.5f, (mins.z + maxs.z) * 0.5f);

	btBoxShape boxShape(halfExtents);

	btTransform startTrans, endTrans;
	startTrans.setIdentity();
	endTrans.setIdentity();

	startTrans.setOrigin(btVector3(start.x + localCenter.x(), start.y + localCenter.y(), start.z + localCenter.z()));
	endTrans.setOrigin(btVector3(end.x + localCenter.x(), end.y + localCenter.y(), end.z + localCenter.z()));

	struct SweepCallback : public btCollisionWorld::ClosestConvexResultCallback
	{
		Int32 ignoreEnt;
		SweepCallback(const btVector3& from, const btVector3& to, Int32 ignore)
			: btCollisionWorld::ClosestConvexResultCallback(from, to), ignoreEnt(ignore) {}

		virtual bool needsCollision(btBroadphaseProxy* proxy0) const override
		{
			btCollisionObject* colObj = (btCollisionObject*)proxy0->m_clientObject;
			if (colObj && colObj->getUserPointer())
			{
				edict_t* pedict = (edict_t*)colObj->getUserPointer();
				if (pedict->entindex == ignoreEnt || pedict->free)
					return false;
			}
			return btCollisionWorld::ClosestConvexResultCallback::needsCollision(proxy0);
		}
	};

	SweepCallback callback(startTrans.getOrigin(), endTrans.getOrigin(), ignore_ent);
	m_pDynamicsWorld->convexSweepTest(&boxShape, startTrans, endTrans, callback);

	if (callback.hasHit() && callback.m_closestHitFraction < trace.fraction)
	{
		trace.fraction = callback.m_closestHitFraction;
		trace.plane.normal = Vector(callback.m_hitNormalWorld.x(), callback.m_hitNormalWorld.y(), callback.m_hitNormalWorld.z());
		trace.plane.dist = Math::DotProduct(trace.plane.normal, Vector(callback.m_hitPointWorld.x(), callback.m_hitPointWorld.y(), callback.m_hitPointWorld.z()));

		Vector dir = end - start;
		trace.endpos = start + dir * trace.fraction;

		if (callback.m_hitCollisionObject && callback.m_hitCollisionObject->getUserPointer())
		{
			edict_t* pedict = (edict_t*)callback.m_hitCollisionObject->getUserPointer();
			trace.hitentity = pedict->entindex;
		}
		return true;
	}

	return false;
}

//=============================================
//
//=============================================
void CBulletPhysics::ApplyImpulse( Uint32 entindex, const Vector& impulse, const Vector& relPos )
{
	if (entindex >= m_bodyIds.size())
		return;

	btRigidBody* body = m_bodyIds[entindex];
	if (!body)
		return;

	body->activate(true);

	if (relPos.IsZero())
	{
		body->applyCentralImpulse(btVector3(impulse.x, impulse.y, impulse.z));
	}
	else
	{
		body->applyImpulse(btVector3(impulse.x, impulse.y, impulse.z), btVector3(relPos.x, relPos.y, relPos.z));
	}
}

//=============================================
//
//=============================================
btCollisionShape* CreateEntityCollisionShape(edict_t* pedict, btVector3& outLocalCenter)
{
	if (!pedict)
	{
		Con_EPrintf("%s - Entity was nullptr.\n", __FUNCTION__);
		return nullptr;
	}

	if (pedict->state.modelindex <= 0)
	{
		Con_EPrintf("%s - Entity %d (%s) has no model set.\n", __FUNCTION__, pedict->entindex, SV_GetString(pedict->fields.classname));
		return nullptr;
	}

	cache_model_t* pmodel = gModelCache.GetModelByIndex(pedict->state.modelindex);
	if (!pmodel)
	{
		Con_EPrintf("[flags=onlyonce_game]%s - Entity %d (%s) model invalid.\n", __FUNCTION__, pedict->entindex, SV_GetString(pedict->fields.classname));
		return nullptr;
	}

	Vector mins = pedict->state.mins;
	Vector maxs = pedict->state.maxs;
	Vector center = (mins + maxs) * 0.5f;
	outLocalCenter.setValue(center.x, center.y, center.z);

	if (pmodel->type == MOD_BRUSH)
	{
		brushmodel_t* pbrushmodel = pmodel->getBrushmodel();
		if (!pbrushmodel || pbrushmodel->nummodelsurfaces == 0)
		{
			Con_EPrintf("[flags=onlyonce_game]%s - Entity %d (%s) has no model surfaces.\n", __FUNCTION__, pedict->entindex, SV_GetString(pedict->fields.classname));
			return nullptr;
		}

		btConvexHullShape* convexShape = new btConvexHullShape();
		for (Uint32 i = 0; i < pbrushmodel->nummodelsurfaces; i++)
		{
			msurface_t* psurf = &pbrushmodel->psurfaces[pbrushmodel->firstmodelsurface + i];
			for (Int32 j = 0; j < psurf->numedges; j++)
			{
				Int32 edgeIndex = pbrushmodel->psurfedges[psurf->firstedge + j];
				mvertex_t* pvert = GetBSPVertex(pbrushmodel, edgeIndex);

				Vector localVert = pvert->origin - center;
				convexShape->addPoint(btVector3(localVert.x, localVert.y, localVert.z), false);
			}
		}

		if (convexShape->getNumPoints() == 0)
		{
			Con_EPrintf("[flags=onlyonce_game]%s - Entity %d (%s) generated 0 convex hull points.\n", __FUNCTION__, pedict->entindex, SV_GetString(pedict->fields.classname));
			delete convexShape;
			return nullptr;
		}

		convexShape->recalcLocalAabb();
		return convexShape;
	}
	else if (pmodel->type == MOD_VBM)
	{
		const vbmcache_t* pvbmcache = pmodel->getVBMCache();
		if (!pvbmcache || !pvbmcache->pmcdheader)
		{
			Con_EPrintf("[flags=onlyonce_game]%s - VBM Entity %d (%s) has no MCD collision data.\n", __FUNCTION__, pedict->entindex, SV_GetString(pedict->fields.classname));
			return nullptr;
		}

		const mcdheader_t* pMCD = pvbmcache->pmcdheader;
		const mcdbodypart_t* pPart = pMCD->getBodyPart(0);
		if (!pPart)
			return nullptr;

		Uint32 subIdx = (pedict->state.body / pPart->base) % pPart->numsubmodels;
		const mcdsubmodel_t* pSub = pPart->getSubmodel(pMCD, subIdx);
		if (!pSub)
			return nullptr;

		const mcdtrimeshtype_t* pMesh = reinterpret_cast<const mcdtrimeshtype_t*>(pSub->getTypeData(pMCD, MCD_COLLISION_TRIANGLES));
		if (!pMesh || pMesh->numvertexes == 0)
			return nullptr;

		const mcdvertex_t* pVerts = pMesh->getVertexes(pMCD);
		btConvexHullShape* convexShape = new btConvexHullShape();
		for (Uint32 i = 0; i < pMesh->numvertexes; i++)
		{
			Vector localVert = pVerts[i].origin - center;
			convexShape->addPoint(btVector3(localVert.x, localVert.y, localVert.z), false);
		}

		if (convexShape->getNumPoints() == 0)
		{
			delete convexShape;
			return nullptr;
		}

		convexShape->recalcLocalAabb();
		return convexShape;
	}

	return nullptr;
}

//=============================================
//
//=============================================
void CBulletPhysics::ProcessContactTouches( void )
{
	if (!m_pDispatcher)
		return;

	Int32 numManifolds = m_pDispatcher->getNumManifolds();
	for (Int32 i = 0; i < numManifolds; i++)
	{
		btPersistentManifold* contactManifold = m_pDispatcher->getManifoldByIndexInternal(i);
		Int32 numContacts = contactManifold->getNumContacts();
		if (numContacts <= 0)
			continue;

		const btCollisionObject* obA = contactManifold->getBody0();
		const btCollisionObject* obB = contactManifold->getBody1();

		edict_t* edictA = static_cast<edict_t*>(obA->getUserPointer());
		edict_t* edictB = static_cast<edict_t*>(obB->getUserPointer());

		if (!edictA || !edictB || edictA->free || edictB->free)
			continue;

		if (edictA->state.movetype != MOVETYPE_PHYSICS && edictB->state.movetype != MOVETYPE_PHYSICS)
			continue;

		btManifoldPoint& pt = contactManifold->getContactPoint(0);
		if (pt.getDistance() > 0.1f)
			continue;

		trace_t tr;
		tr.hitentity = edictB->entindex;
		tr.fraction = 0.0f;
		tr.endpos = edictA->state.origin;

		btVector3 norm = pt.m_normalWorldOnB;
		tr.plane.normal = Vector(norm.x(), norm.y(), norm.z());
		tr.plane.dist = Math::DotProduct(tr.plane.normal, tr.endpos);

		SV_Impact(edictA, edictB, tr);
	}
}

//=============================================
//
//=============================================
void CBulletPhysics::ApplyBuoyancy( edict_t* pedict, btRigidBody* body )
{
	SV_CheckWater(pedict);
	if (pedict->state.waterlevel <= WATERLEVEL_NONE)
	{
		body->setDamping(0.0f, 0.0f);
		return;
	}

	Float totalHeight = pedict->state.maxs.z - pedict->state.mins.z;
	Float submergedHeight = SV_Submerged(pedict);
	Float submergedRatio = (totalHeight > 0) ? clamp(submergedHeight / totalHeight, 0.0f, 1.0f) : 1.0f;

	if (submergedRatio <= 0)
		return;

	Float mass = 1.0f / body->getInvMass();
	Float gravity = g_psv_gravity->GetValue();
	Float buoyancyFactor = (pedict->state.skin > 0) ? (pedict->state.skin * 0.1f) : 1.2f;

	btVector3 buoyantForce(0, 0, mass * gravity * submergedRatio * buoyancyFactor);
	body->applyCentralForce(buoyantForce);

	body->setDamping(0.7f * submergedRatio, 0.7f * submergedRatio);
}

//=============================================
//
//=============================================
void CBulletPhysics::SyncEntityToPhysics(edict_t* pedict, Uint32 entindex)
{
	btVector3 localCenter;
	btCollisionShape* shape = CreateEntityCollisionShape(pedict, localCenter);
	if (!shape)
		return;

	btTransform startTransform;
	startTransform.setIdentity();

	vec4_t vq;
	Math::AngleQuaternion(pedict->state.angles, vq);
	btQuaternion rot((Float)vq[0], (Float)vq[1], (Float)vq[2], (Float)vq[3]);
	startTransform.setRotation(rot);

	btVector3 worldOrigin(pedict->state.origin.x, pedict->state.origin.y, pedict->state.origin.z);
	btVector3 worldCenter = worldOrigin + quatRotate(rot, localCenter);

	startTransform.setOrigin(worldCenter);

	btScalar mass = 0.0f;
	btVector3 localInertia(0, 0, 0);

	bool isKinematic = (pedict->state.movetype == MOVETYPE_PUSH);
	if (!isKinematic)
	{
		mass = (pedict->state.fuser1 > 0) ? pedict->state.fuser1 : 100.f;
		shape->calculateLocalInertia(mass, localInertia);
	}

	btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState, shape, localInertia);

	rbInfo.m_friction = 0.8f;
	rbInfo.m_restitution = 0.2f;

	btRigidBody* body = new btRigidBody(rbInfo);
	body->setUserPointer(pedict);

	if (isKinematic)
	{
		body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
		body->setActivationState(DISABLE_DEACTIVATION);
	}
	else
	{
		body->setActivationState(DISABLE_DEACTIVATION);

		float hx = (pedict->state.maxs.x - pedict->state.mins.x) * 0.5f;
		float hy = (pedict->state.maxs.y - pedict->state.mins.y) * 0.5f;
		float hz = (pedict->state.maxs.z - pedict->state.mins.z) * 0.5f;
		float minExtent = hx < hy ? (hx < hz ? hx : hz) : (hy < hz ? hy : hz);

		body->setCcdMotionThreshold(minExtent * 0.5f);
		body->setCcdSweptSphereRadius(minExtent * 0.2f);
	}

	m_pDynamicsWorld->addRigidBody(body);
	m_bodyIds[entindex] = body;
}

//=============================================
//
//=============================================
void CBulletPhysics::SyncEntityToKinematic( edict_t* pedict, Uint32 entindex )
{
	btRigidBody* body = m_bodyIds[entindex];
	if (!body)
		return;

	btTransform trans;
	trans.setIdentity();

	vec4_t vq;
	Math::AngleQuaternion(pedict->state.angles, vq);
	btQuaternion rot((Float)vq[0], (Float)vq[1], (Float)vq[2], (Float)vq[3]);
	trans.setRotation(rot);

	btVector3 localCenter((pedict->state.mins.x + pedict->state.maxs.x) * 0.5f, (pedict->state.mins.y + pedict->state.maxs.y) * 0.5f, (pedict->state.mins.z + pedict->state.maxs.z) * 0.5f);
	btVector3 worldOrigin(pedict->state.origin.x, pedict->state.origin.y, pedict->state.origin.z);
	btVector3 worldCenter = worldOrigin + quatRotate(rot, localCenter);

	trans.setOrigin(worldCenter);

	if (body->getMotionState())
		body->getMotionState()->setWorldTransform(trans);

	body->setWorldTransform(trans);
}

//=============================================
//
//=============================================
void CBulletPhysics::SyncPhysicsToEntity(edict_t* pedict, Uint32 entindex)
{
	btRigidBody* body = m_bodyIds[entindex];
	if (!body)
		return;

	btTransform trans = body->getWorldTransform();
	btQuaternion rot = trans.getRotation();

	btVector3 localCenter((pedict->state.mins.x + pedict->state.maxs.x) * 0.5f, (pedict->state.mins.y + pedict->state.maxs.y) * 0.5f, (pedict->state.mins.z + pedict->state.maxs.z) * 0.5f);

	btVector3 worldOrigin = trans.getOrigin() - quatRotate(rot, localCenter);

	pedict->state.origin.x = worldOrigin.x();
	pedict->state.origin.y = worldOrigin.y();
	pedict->state.origin.z = worldOrigin.z();

	Float mat[3][4];
	vec4_t vq;
	vq[0] = rot.x(); 
	vq[1] = rot.y(); 
	vq[2] = rot.z(); 
	vq[3] = rot.w();

	Math::QuaternionMatrix(vq, mat);

	pedict->state.angles.x = SDL_asin(mat[2][0]) * (180.0f / M_PI);
	pedict->state.angles.y = SDL_atan2(mat[1][0], mat[0][0]) * (180.0f / M_PI);
	pedict->state.angles.z = SDL_atan2(mat[2][1], mat[2][2]) * (180.0f / M_PI);

	SV_LinkEdict(pedict, true);
}

//=============================================
//
//=============================================
void SV_ApplyPhysicsImpulse(edict_t* pedict, const Vector& impulse, const Vector& relPos)
{
	if (!pedict || pedict->free)
		return;

	gBulletPhysics.ApplyImpulse(pedict->entindex, impulse, relPos);
}