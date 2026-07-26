/*
===============================================
Pathos Engine - Created by Andrew Stephen "Overfloater" Lucas

Copyright 2016
All Rights Reserved.
===============================================
*/

#ifndef FUNCPENDULUM_H
#define FUNCPENDULUM_H

//=============================================
//
//=============================================
class CFuncPendulum : public CBaseEntity
{
public:
	// Default speed for pendulum
	static const Float DEFAULT_SPEED;

public:
	enum
	{
		FL_ROTATE_Y		= 0,
		FL_START_ON		= (1<<0),
		FL_SWING		= (1<<1),
		FL_NOT_SOLID	= (1<<3),
		FL_AUTO_RETURN	= (1<<4),
		FL_ROTATE_Z		= (1<<6),
		FL_ROTATE_X		= (1<<7)
	};

public:
	explicit CFuncPendulum( edict_t* pedict );
	virtual ~CFuncPendulum( void );

public:
	virtual bool Spawn( void ) override;
	virtual void Precache( void ) override { }
	virtual bool KeyValue( const keyvalue_t& kv ) override;
	virtual void CallBlocked( CBaseEntity* pOther ) override;
	virtual void CallUse( CBaseEntity* pacticator, CBaseEntity* pCaller, usemode_t useMode, Float value ) override;
	virtual void CallTouch( CBaseEntity* pOther ) override;
	virtual void DeclareSaveFields( void ) override;
	virtual void InitEntity( void ) override;

	virtual Int32 GetEntityFlags( void ) override { return CBaseEntity::GetEntityFlags() & ~FL_ENTITY_TRANSITION; }
	virtual bool CanEntityBeParent( void ) const override { return true; }
	virtual bool CanEntityBeParented( void ) const override { return true; }

	virtual void BeginParentMovement( CBaseEntity* pParent ) override;
	virtual void RotateEntityByParent( CBaseEntity* pRotatorParent, const Float (*pPrevRotationMatrix)[4], const Float (*pCurRotationMatrix)[4], const Vector& rotatorAngularMove ) override;
	virtual void OnParentEntitySetAngles( CBaseEntity* pSetParent, const Vector& parentOrigin, const Float (*pPrevRotationMatrix)[4], const Float (*pCurRotationMatrix)[4], const Vector& angularChange ) override;
	virtual void OnEntitySetAngles( const Float (*pPrevRotationMatrix)[4], const Float (*pCurRotationMatrix)[4], const Vector& angularChange, bool realignEntity ) override;

public:
	void EXPORTFN SwingThink( void );
	void EXPORTFN StopThink( void );
	void EXPORTFN DelayInit( void );

protected:
	Float m_acceleration;
	Float m_distance;
	Double m_time;
	Float m_dampening;
	Float m_maxSpeed;
	Float m_dampSpeed;
	Float m_damage;

	Vector m_center;
	Vector m_start;
};
#endif //FUNCPENDULUM_H