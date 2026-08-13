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
#ifndef SV_BULLETPHYSICS_H
#define SV_BULLETPHYSICS_H

#include <btBulletDynamicsCommon.h>
#include "carray.h"
#include "edict.h"

class CBulletDebugDraw : public btIDebugDraw
{
public:
	virtual void drawLine(const btVector3& from, const btVector3& to, const btVector3& color) override;
	virtual void drawContactPoint(const btVector3& PointOnB, const btVector3& normalOnB, btScalar distance, int lifeTime, const btVector3& color) override {}
	virtual void reportErrorWarning(const char* warningString) override { Con_Printf("Bullet Warning: %s\n", warningString); }
	virtual void draw3dText(const btVector3& location, const char* textString) override {}
	virtual void setDebugMode(int debugMode) override { m_debugMode = debugMode; }
	virtual int getDebugMode() const override { return m_debugMode; }
private:
	int m_debugMode;
};

class CBulletPhysics
{
public:
	CBulletPhysics( void );
	~CBulletPhysics( void );

	void Init( void );
	void Shutdown( void );

	void ClearWorld( void );
	void SetupStaticWorld( void );
	void Frame( double frametime );

	void DrawDebug( void );

	bool ConvexSweepTest( const Vector& start, const Vector& end, const Vector& mins, const Vector& maxs, Int32 ignore_ent, trace_t& trace );

	void ApplyImpulse( Uint32 entindex, const Vector& impulse, const Vector& relPos = Vector(0, 0, 0) );

private:
	void ProcessContactTouches( void );
	void ApplyBuoyancy( edict_t* pedict, btRigidBody* body );
	void SyncEntityToPhysics( edict_t* pedict, Uint32 entindex );
	void SyncEntityToKinematic( edict_t* pedict, Uint32 entindex );
	void SyncPhysicsToEntity( edict_t* pedict, Uint32 entindex );
	void RemoveBody( Uint32 entindex );

private:
	btDefaultCollisionConfiguration* m_pCollisionConfiguration;
	btCollisionDispatcher* m_pDispatcher;
	btBroadphaseInterface* m_pOverlappingPairCache;
	btSequentialImpulseConstraintSolver* m_pSolver;
	btDiscreteDynamicsWorld* m_pDynamicsWorld;

	btTriangleMesh* m_pWorldMesh;
	btRigidBody* m_pStaticWorldBody;

	CArray<btRigidBody*> m_bodyIds;

	CBulletDebugDraw m_debugDrawer;
};

extern void SV_ApplyPhysicsImpulse(edict_t* pedict, const Vector& impulse, const Vector& relPos);

extern CBulletPhysics gBulletPhysics;

#endif // SV_BULLETPHYSICS_H