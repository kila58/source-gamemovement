#pragma once

#include "sdk.h"

#define	COORD_INTEGER_BITS			14
#define COORD_FRACTIONAL_BITS		5
#define COORD_DENOMINATOR			(1<<(COORD_FRACTIONAL_BITS))
#define COORD_RESOLUTION	(1.0/(COORD_DENOMINATOR))

#define DIST_EPSILON (0.03125)

#define	MAX_CLIP_PLANES		5

#define TICK_INTERVAL	(globals->interval_per_tick)
#define TIME_TO_TICKS( dt )		( (int)( 0.5f + (float)(dt) / TICK_INTERVAL ) )
#define TICKS_TO_TIME( t ) ( TICK_INTERVAL *( t ) )

class GameMove
{
private:
	//player
	Vector m_vecVelocity;
	Vector m_vecBaseVelocity;
	Vector m_vecAbsOrigin;
	float m_surfaceFriction;
	float m_flStepSize;

	//cvars
	float sv_gravity;
	float sv_friction;
	float sv_stopspeed;
	float sv_accelerate;
	float sv_airaccelerate;
	float sv_maxvelocity;
	float sv_bounce;

	//meta
	float frametime;
	CBaseEntity* groundentity;

	//settings
	bool debug = false;

public:
	void InitalState(CBaseEntity* ply);
	void Spew(std::string text, bool fordebug = true);
	trace_t UTIL_TraceRay(const Ray_t &ray, unsigned int mask, CBaseEntity* ignore, int collisionGroup);
	trace_t TracePlayerBBox(CBaseEntity* ply, Vector start, Vector end, unsigned int fmask, int collisiongroup);
	trace_t TryTouchGround(CBaseEntity* ply, Vector start, Vector end, Vector mins, Vector maxs, unsigned int fmask, int collisiongroup);
	void TryTouchGroundInQuadrants(CBaseEntity* ply, Vector start, Vector end, unsigned int fmask, int collisiongroup, trace_t& pm);
	void CategorizeGroundSurface(CBaseEntity* ply, trace_t& pm);
	void SetGroundEntity(CBaseEntity* ply, trace_t* pm);
	void CategorizePosition(CBaseEntity* ply);
	bool IsOnGround(CBaseEntity* ply);
	void CheckVelocity();
	void StartGravity(CBaseEntity* ply);
	void FinishGravity(CBaseEntity* ply);
	void Friction(CBaseEntity* ply);
	void Accelerate(CBaseEntity* ply, Vector wishdir, float wishspeed, float accel);
	void AirAccelerate(CBaseEntity* ply, Vector wishdir, float wishspeed, float accel);
	void StayOnGround(CBaseEntity* ply);
	int TryPlayerMove(CBaseEntity* ply, Vector* firstdest = nullptr, trace_t* firsttrace = nullptr);
	void StepMove(CBaseEntity* ply, Vector destination, trace_t &trace);
	void WalkMove(CBaseEntity* ply);
	void AirMove(CBaseEntity* ply);
	void FullWalkMove(CBaseEntity* ply);
	void PlayerMove(CBaseEntity* ply);
	void RenderMove(CBaseEntity* ply, int ticks);
};