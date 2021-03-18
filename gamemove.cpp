#include "gamemove.h"

void GameMove::InitalState(CBaseEntity* ply)
{
	m_vecVelocity = ply->EstimateAbsVelocity();
	m_vecBaseVelocity = ply->GetBaseVelocity();
	m_vecAbsOrigin = ply->GetAbsOrigin();
	m_surfaceFriction = ply->GetSurfaceFriction();
	m_flStepSize = ply->GetStepSize();

	sv_gravity = cvar->FindVar("sv_gravity").GetValue<float>();
	sv_friction = cvar->FindVar("sv_friction").GetValue<float>();
	sv_stopspeed = cvar->FindVar("sv_stopspeed").GetValue<float>();
	sv_accelerate = cvar->FindVar("sv_accelerate").GetValue<float>();
	sv_airaccelerate = cvar->FindVar("sv_airaccelerate").GetValue<float>();
	sv_maxvelocity = cvar->FindVar("sv_maxvelocity").GetValue<float>();
	sv_bounce = cvar->FindVar("sv_bounce").GetValue<float>();

	frametime = globals->interval_per_tick;
	groundentity = entlist->GetClientEntityFromHandle(ply->GetGroundEntity());

	debug = true;
}

void GameMove::Spew(std::string text, bool fordebug)
{
	if ((fordebug && debug) || !fordebug)
		util::Print(text);
}

trace_t GameMove::UTIL_TraceRay(const Ray_t& ray, unsigned int mask, CBaseEntity* ignore, int collisionGroup)
{
	trace_t trace;
	CTraceFilter filter(ignore, collisionGroup);

	traces->TraceRay(ray, mask, &filter, &trace);

	if (debug)
	{
		Vector start, end;
		if (draw::WorldToScreen(trace.start, start) && draw::WorldToScreen(trace.end, end))
		{
			surface->SetDrawColor(Color(255, 255, 255));
			surface->DrawLine(start.x, start.y, end.x, end.y);
		}
	}

	return trace;
}

trace_t GameMove::TracePlayerBBox(CBaseEntity* ply, Vector start, Vector end, unsigned int fmask, int collisiongroup)
{
	Ray_t ray;
	ray.Init(start, end, ply->OBBMins() , ply->OBBMaxs());

	return UTIL_TraceRay(ray, fmask, ply, collisiongroup);
}

trace_t GameMove::TryTouchGround(CBaseEntity* ply, Vector start, Vector end, Vector mins, Vector maxs, unsigned int fmask, int collisiongroup)
{
	Ray_t ray;
	ray.Init(start, end, mins, maxs);

	return UTIL_TraceRay(ray, fmask, ply, collisiongroup);
}

void GameMove::TryTouchGroundInQuadrants(CBaseEntity* ply, Vector start, Vector end, unsigned int fmask, int collisiongroup, trace_t& pm)
{
	Vector mins, maxs;
	Vector minsSrc = ply->OBBMins();
	Vector maxsSrc = ply->OBBMaxs();

	float fraction = pm.fraction;
	Vector endpos = pm.end;

	// Check the -x, -y quadrant
	mins = minsSrc;
	maxs.Init(min(0, maxsSrc.x), min(0, maxsSrc.y), maxsSrc.z);
	pm = TryTouchGround(ply, start, end, mins, maxs, fmask, collisiongroup);
	if (pm.m_pEnt && pm.plane.normal[2] >= 0.7)
	{
		pm.fraction = fraction;
		pm.end = endpos;
		return;
	}

	// Check the +x, +y quadrant
	mins.Init(max(0, minsSrc.x), max(0, minsSrc.y), minsSrc.z);
	maxs = maxsSrc;
	pm = TryTouchGround(ply, start, end, mins, maxs, fmask, collisiongroup);
	if (pm.m_pEnt && pm.plane.normal[2] >= 0.7)
	{
		pm.fraction = fraction;
		pm.end = endpos;
		return;
	}

	// Check the -x, +y quadrant
	mins.Init(minsSrc.x, max(0, minsSrc.y), minsSrc.z);
	maxs.Init(min(0, maxsSrc.x), maxsSrc.y, maxsSrc.z);
	pm = TryTouchGround(ply, start, end, mins, maxs, fmask, collisiongroup);
	if (pm.m_pEnt && pm.plane.normal[2] >= 0.7)
	{
		pm.fraction = fraction;
		pm.end = endpos;
		return;
	}

	// Check the +x, -y quadrant
	mins.Init(max(0, minsSrc.x), minsSrc.y, minsSrc.z);
	maxs.Init(maxsSrc.x, min(0, maxsSrc.y), maxsSrc.z);
	pm = TryTouchGround(ply, start, end, mins, maxs, fmask, collisiongroup);
	if (pm.m_pEnt && pm.plane.normal[2] >= 0.7)
	{
		pm.fraction = fraction;
		pm.end = endpos;
		return;
	}

	pm.fraction = fraction;
	pm.end = endpos;
}

void GameMove::CategorizeGroundSurface(CBaseEntity* ply, trace_t& pm)
{
	int m_surfaceProps = pm.surface.surfaceProps;
	physics->GetPhysicsProperties(m_surfaceProps, NULL, NULL, &m_surfaceFriction, NULL);

	// HACKHACK: Scale this to fudge the relationship between vphysics friction values and player friction values.
	// A value of 0.8f feels pretty normal for vphysics, whereas 1.0f is normal for players.
	// This scaling trivially makes them equivalent.  REVISIT if this affects low friction surfaces too much.
	m_surfaceFriction *= 1.25f;
	if (m_surfaceFriction > 1.0f)
		m_surfaceFriction = 1.0f;

	//player->m_chTextureType = player->m_pSurfaceData->game.material;
}

void GameMove::SetGroundEntity(CBaseEntity* ply, trace_t* pm)
{
	CBaseEntity* newGround = pm ? pm->m_pEnt : nullptr;

	CBaseEntity* oldGround = groundentity;
	Vector vecBaseVelocity = m_vecBaseVelocity;

	if (!oldGround && newGround)
	{
		// Subtract ground velocity at instant we hit ground jumping
		vecBaseVelocity -= newGround->EstimateAbsVelocity();
		vecBaseVelocity.z = newGround->EstimateAbsVelocity().z;
	}
	else if (oldGround && !newGround)
	{
		// Add in ground velocity at instant we started jumping
		vecBaseVelocity += oldGround->EstimateAbsVelocity();
		vecBaseVelocity.z = oldGround->EstimateAbsVelocity().z;
	}

	m_vecBaseVelocity = vecBaseVelocity;
	groundentity = newGround;

	// If we are on something...

	if (newGround)
	{
		CategorizeGroundSurface(ply, *pm);

		// Then we are not in water jump sequence
		//player->m_flWaterJumpTime = 0;

		// Standing on an entity other than the world, so signal that we are touching something.
		/*if (!pm->DidHitWorld())
		{
			MoveHelper()->AddToTouched(*pm, mv->m_vecVelocity);
		}*/

		m_vecVelocity.z = 0.0f;
	}
}

void GameMove::CategorizePosition(CBaseEntity* ply)
{
	Vector point;
	trace_t pm;

	m_surfaceFriction = 1.0f;

	float flOffset = 2.0f;

	point.x = m_vecAbsOrigin.x;
	point.y = m_vecAbsOrigin.y;
	point.z = m_vecAbsOrigin.z - flOffset;

	Vector bumpOrigin = m_vecAbsOrigin;

#define NON_JUMP_VELOCITY 140.0f

	float zvel = m_vecVelocity.z;
	bool bMovingUp = zvel > 0.0f;
	bool bMovingUpRapidly = zvel > NON_JUMP_VELOCITY;
	float flGroundEntityVelZ = 0.0f;

	if (bMovingUpRapidly)
	{
		if (IsOnGround(ply))
		{
			flGroundEntityVelZ = groundentity->EstimateAbsVelocity().z;
			bMovingUpRapidly = (zvel - flGroundEntityVelZ) > NON_JUMP_VELOCITY;
		}
	}

	if (bMovingUpRapidly)
	{
		//Spew("bMovingUpRapidly");
		SetGroundEntity(ply, nullptr);
	}
	else
	{
		pm = TryTouchGround(ply, bumpOrigin, point, ply->OBBMins(), ply->OBBMaxs(), MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT);
		if (!pm.m_pEnt || pm.plane.normal.z < 0.7)
		{
			TryTouchGroundInQuadrants(ply, bumpOrigin, point, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT, pm);
			if (!pm.m_pEnt || pm.plane.normal.z < 0.7)
			{
				SetGroundEntity(ply, nullptr);

				if (m_vecVelocity.z > 0.0f)
					m_surfaceFriction = 0.25f;
			}
			else
				SetGroundEntity(ply, &pm);
		}
		else
			SetGroundEntity(ply, &pm);
	}
}

bool GameMove::IsOnGround(CBaseEntity* ply)
{
	return (groundentity != nullptr);
	//return (ply->GetFlags() & FL_ONGROUND);
}

void GameMove::CheckVelocity()
{
	for (int i = 0; i < 3; i++)
	{
		if (m_vecVelocity[i] > sv_maxvelocity)
			m_vecVelocity[i] = sv_maxvelocity;
		else if (m_vecVelocity[i] < -sv_maxvelocity)
			m_vecVelocity[i] = -sv_maxvelocity;
	}
}

void GameMove::StartGravity(CBaseEntity* ply)
{
	float ent_gravity;
	if (ply->GetGravity())
		ent_gravity = ply->GetGravity();
	else
		ent_gravity = 1;

	m_vecVelocity.z -= (ent_gravity * sv_gravity * 0.5f * frametime);
	m_vecVelocity.z += (m_vecBaseVelocity.z * frametime);

	m_vecBaseVelocity.z = 0;

	CheckVelocity();
}

void GameMove::FinishGravity(CBaseEntity* ply)
{
	float ent_gravity;
	if (ply->GetGravity())
		ent_gravity = ply->GetGravity();
	else
		ent_gravity = 1;

	m_vecVelocity.z -= (ent_gravity * sv_gravity * frametime * 0.5f);

	CheckVelocity();
}

void GameMove::Friction(CBaseEntity* ply)
{
	float speed = m_vecVelocity.Length();
	if (speed < 0.1f)
		return;

	float friction, control, drop;
	drop = 0;
	if (IsOnGround(ply))
	{
		friction = sv_friction * m_surfaceFriction;
		control = (speed < sv_stopspeed) ? sv_stopspeed : speed;
		drop += control * friction * frametime;
	}

	float newspeed = speed - drop;
	if (newspeed < 0)
		newspeed = 0;

	if (newspeed != speed)
	{
		newspeed /= speed;
		m_vecVelocity = m_vecVelocity * newspeed;
	}
}

void GameMove::Accelerate(CBaseEntity* ply, Vector wishdir, float wishspeed, float accel)
{
	float currentspeed = m_vecVelocity.Dot(wishdir);

	float addspeed = wishspeed - currentspeed;
	if (addspeed <= 0)
		return;

	float accelspeed = accel * frametime * wishspeed * m_surfaceFriction;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (int i = 0; i < 3; i++)
		m_vecVelocity[i] += accelspeed * wishdir[i];
}

void GameMove::AirAccelerate(CBaseEntity* ply, Vector wishdir, float wishspeed, float accel)
{
	float wishspd = wishspeed;
	if (wishspd > 30)
		wishspd = 30;

	float currentspeed = m_vecVelocity.Dot(wishdir);

	float addspeed = wishspd - currentspeed;
	if (addspeed <= 0)
		return;

	float accelspeed = accel * wishspeed * frametime * m_surfaceFriction;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (int i = 0; i < 3; i++)
		m_vecVelocity[i] += accelspeed * wishdir[i];
}

int ClipVelocity(Vector& in, Vector& normal, Vector& out, float overbounce)
{
	float	backoff;
	float	change;
	float angle;
	int		i, blocked;

	angle = normal.z;

	blocked = 0x00;         // Assume unblocked.
	if (angle > 0)			// If the plane that is blocking us has a positive z component, then assume it's a floor.
		blocked |= 0x01;	// 
	if (!angle)				// If the plane has no Z, it is vertical (wall/step)
		blocked |= 0x02;	// 

	// Determine how far along plane to slide based on incoming direction.
	backoff = in.Dot(normal) * overbounce;

	for (i = 0; i<3; i++)
	{
		change = normal[i] * backoff;
		out[i] = in[i] - change;
	}

	// iterate once to make sure we aren't still moving through the plane
	float adjust = out.Dot(normal);
	if (adjust < 0.0f)
		out -= (normal * adjust);

	// Return blocking flags.
	return blocked;
}

void GameMove::StayOnGround(CBaseEntity* ply)
{
	Vector start(m_vecAbsOrigin);
	Vector end(m_vecAbsOrigin);
	start.z += 2;
	end.z -= m_flStepSize;

	// See how far up we can go without getting stuck

	trace_t trace = TracePlayerBBox(ply, m_vecAbsOrigin, start, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT);
	start = trace.end;

	// using trace.startsolid is unreliable here, it doesn't get set when
	// tracing bounding box vs. terrain

	// Now trace down from a known safe position
	trace = TracePlayerBBox(ply, start, end, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT);
	if (trace.fraction > 0.0f &&			// must go somewhere
		trace.fraction < 1.0f &&			// must hit something
		!trace.startSolid &&				// can't be embedded in a solid
		trace.plane.normal.z >= 0.7)		// can't hit a steep slope that we can't stand on anyway
	{
		float flDelta = fabs(m_vecAbsOrigin.z - trace.end.z);

		//This is incredibly hacky. The real problem is that trace returning that strange value we can't network over.
		if (flDelta > 0.5f * COORD_RESOLUTION)
			m_vecAbsOrigin = trace.end;
	}
}

int GameMove::TryPlayerMove(CBaseEntity* ply, Vector* firstdest, trace_t* firsttrace)
{
	int numbumps = 4;           // Bump up to four times

	int blocked = 0;           // Assume not blocked
	int numplanes = 0;           //  and not sliding along any planes
	Vector planes[MAX_CLIP_PLANES];

	Vector original_velocity = m_vecVelocity; // Store original velocity
	Vector primal_velocity = m_vecVelocity;

	float allFraction = 0;
	float time_left = frametime;   // Total time for this movement operation.

	Vector new_velocity;

	trace_t pm;

	int bumpcount;
	for (bumpcount = 0; bumpcount < numbumps; bumpcount++)
	{
		if (m_vecVelocity.Length() == 0.0)
			break;

		Vector end = m_vecAbsOrigin + m_vecVelocity * time_left;

		if (firstdest && end == *firstdest)
			pm = *firsttrace;
		else
			pm = TracePlayerBBox(ply, m_vecAbsOrigin, end, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT);

		allFraction += pm.fraction;

		if (pm.allsolid)
		{
			m_vecVelocity = Vector();
			return 4;
		}

		if (pm.fraction > 0)
		{
			if (numbumps > 0 && pm.fraction == 1)
			{
				// There's a precision issue with terrain tracing that can cause a swept box to successfully trace
				// when the end position is stuck in the triangle.  Re-run the test with an uswept box to catch that
				// case until the bug is fixed.
				// If we detect getting stuck, don't allow the movement
				trace_t stuck = TracePlayerBBox(ply, pm.end, pm.end, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT);
				if (stuck.startSolid || stuck.fraction != 1.0f)
				{
					//Msg( "Player will become stuck!!!\n" );
					m_vecVelocity = Vector();
					break;
				}
			}

			// actually covered some distance
			m_vecAbsOrigin = pm.end;
			original_velocity = m_vecVelocity;
			numplanes = 0;
		}

		// If we covered the entire distance, we are done
		//  and can return.
		if (pm.fraction == 1)
		{
			break;		// moved the entire distance
		}

		//MoveHelper()->AddToTouched(pm, mv->m_vecVelocity);

		// If the plane we hit has a high z component in the normal, then
		//  it's probably a floor
		if (pm.plane.normal.z > 0.7)
		{
			blocked |= 1;		// floor
		}
		// If the plane has a zero z component in the normal, then it's a 
		//  step or wall
		if (!pm.plane.normal.z)
		{
			blocked |= 2;		// step / wall
		}

		time_left -= time_left * pm.fraction;

		if (numplanes >= MAX_CLIP_PLANES)
		{
			m_vecVelocity = Vector();

			break;
		}

		// Set up next clipping plane
		planes[numplanes] = pm.plane.normal;
		numplanes++;

		int x;
		int j;
		if (numplanes == 1 && !IsOnGround(ply))
		{
			for (x = 0; x < numplanes; x++)
			{
				if (planes[x].z > 0.7)
				{
					// floor or slope
					ClipVelocity(original_velocity, planes[x], new_velocity, 1);
					original_velocity = new_velocity;
				}
				else
				{
					ClipVelocity(original_velocity, planes[x], new_velocity, 1.0f + (sv_bounce * (1.0f  - m_surfaceFriction)));
				}
			}

			m_vecVelocity = new_velocity;
			original_velocity = new_velocity;
		}
		else
		{
			for (x = 0; x < numplanes; x++)
			{
				ClipVelocity(original_velocity, planes[x], m_vecVelocity, 1);

				for (j = 0; j < numplanes; j++)
					if (j != x)
					{
						// Are we now moving against this plane?
						if (m_vecVelocity.Dot(planes[j]) < 0)
							break;	// not ok
					}

				if (j == numplanes)  // Didn't have to clip, so we're ok
					break;
			}

			float d;
			// Did we go all the way through plane set
			if (x != numplanes)
			{	// go along this plane
				// pmove.velocity is set in clipping call, no need to set again.
				;
			}
			else
			{	// go along the crease
				if (numplanes != 2)
				{
					m_vecVelocity = Vector();
					break;
				}

				Vector dir = planes[0].Cross(planes[1]);
				dir.NormalizeInPlace();
				d = dir.Dot(m_vecVelocity);
				m_vecVelocity = dir * d;
			}

			//
			// if original velocity is against the original velocity, stop dead
			// to avoid tiny occilations in sloping corners
			//
			d = m_vecVelocity.Dot(primal_velocity);
			if (d <= 0)
			{
				m_vecVelocity = Vector();
				break;
			}
		}
	}

	if (allFraction == 0)
		m_vecVelocity = Vector();

	return blocked;
}

void GameMove::StepMove(CBaseEntity* ply, Vector destination, trace_t &trace)
{
	Vector vecEndPos = destination;

	// Try sliding forward both on ground and up 16 pixels
	//  take the move that goes farthest
	Vector vecPos = m_vecAbsOrigin, vecVel = m_vecVelocity;

	// Slide move down.
	TryPlayerMove(ply, &vecEndPos, &trace);

	// Down results.
	Vector vecDownPos = m_vecAbsOrigin, vecDownVel = m_vecVelocity;

	// Reset original values.
	m_vecAbsOrigin = vecPos;
	m_vecVelocity = vecVel;

	// Move up a stair height.
	vecEndPos = m_vecAbsOrigin;
	//if (player->m_Local.m_bAllowAutoMovement)
	{
		vecEndPos.z += m_flStepSize + DIST_EPSILON;
	}

	trace = TracePlayerBBox(ply, m_vecAbsOrigin, vecEndPos, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT);
	if (!trace.startSolid && !trace.allsolid)
		m_vecAbsOrigin = trace.end;

	// Slide move up.
	TryPlayerMove(ply);

	// Move down a stair (attempt to).
	vecEndPos = m_vecAbsOrigin;
	//if (player->m_Local.m_bAllowAutoMovement)
	{
		vecEndPos.z -= m_flStepSize + DIST_EPSILON;
	}

	trace = TracePlayerBBox(ply, m_vecAbsOrigin, vecEndPos, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT);

	// If we are not on the ground any more then use the original movement attempt.
	if (trace.plane.normal[2] < 0.7)
	{
		m_vecAbsOrigin = vecDownPos;
		m_vecVelocity = vecDownVel;

		return;
	}

	// If the trace ended up in empty space, copy the end over to the origin.
	if (!trace.startSolid && !trace.allsolid)
		m_vecAbsOrigin = trace.end;

	// Copy this origin to up.
	Vector vecUpPos = m_vecAbsOrigin;

	// decide which one went farther
	float flDownDist = (vecDownPos.x - vecPos.x) * (vecDownPos.x - vecPos.x) + (vecDownPos.y - vecPos.y) * (vecDownPos.y - vecPos.y);
	float flUpDist = (vecUpPos.x - vecPos.x) * (vecUpPos.x - vecPos.x) + (vecUpPos.y - vecPos.y) * (vecUpPos.y - vecPos.y);
	if (flDownDist > flUpDist)
	{
		m_vecAbsOrigin = vecDownPos;
		m_vecVelocity = vecDownVel;
	}
	else
		// copy z value from slide move
		m_vecVelocity.z = vecDownVel.z;
}

void GameMove::WalkMove(CBaseEntity* ply)
{
	auto oldground = groundentity;

	Vector wishvel = ply->EstimateAbsVelocity(); //VELOCITY CALCULATION IS FUCKED, CANT USE PREDICTED VALUE m_vecVelocity
	wishvel.z = 0;

	Vector wishdir = wishvel;
	float wishspeed = wishdir.Normalize();
	float maxspeed = ply->GetMaxSpeed();
	if ((wishspeed != 0) && (wishspeed > maxspeed))
	{
		wishvel = wishvel * (maxspeed / wishspeed);
		wishspeed = maxspeed;
	}

	m_vecVelocity.z = 0;
	Accelerate(ply, wishdir, wishspeed, sv_accelerate);
	m_vecVelocity.z = 0;

	m_vecVelocity += m_vecBaseVelocity;

	float spd = m_vecVelocity.Length();
	if (spd < 1.0F)
	{
		m_vecVelocity = Vector();
		m_vecVelocity -= m_vecBaseVelocity;

		return;
	}

	Vector dest;
	dest.x = m_vecAbsOrigin.x + m_vecVelocity.x * frametime;
	dest.y = m_vecAbsOrigin.y + m_vecVelocity.y * frametime;
	dest.z = m_vecAbsOrigin.z;

	auto pm = TracePlayerBBox(ply, m_vecAbsOrigin, dest, MASK_PLAYERSOLID, COLLISION_GROUP_PLAYER_MOVEMENT);

	if (pm.fraction == 1)
	{
		m_vecAbsOrigin = pm.end;
		m_vecVelocity -= m_vecBaseVelocity;
		StayOnGround(ply);

		return;
	}

	if (!oldground /*&& player->GetWaterLevel() == 0*/)
	{
		m_vecVelocity -= m_vecBaseVelocity;

		return;
	}

	StepMove(ply, dest, pm);

	m_vecVelocity -= m_vecBaseVelocity;

	StayOnGround(ply);
}

void GameMove::AirMove(CBaseEntity* ply)
{
	Vector wishvel = ply->EstimateAbsVelocity();
	wishvel.z = 0;

	Vector wishdir = wishvel;
	float wishspeed = wishdir.Normalize();
	float maxspeed = ply->GetMaxSpeed();
	if ((wishspeed != 0) && (wishspeed > maxspeed))
	{
		wishvel = wishvel * (maxspeed / wishspeed);
		wishspeed = maxspeed;
	}

	AirAccelerate(ply, wishdir, wishspeed, sv_airaccelerate);

	m_vecVelocity += m_vecBaseVelocity;

	TryPlayerMove(ply);

	m_vecVelocity -= m_vecBaseVelocity;
}

void GameMove::FullWalkMove(CBaseEntity* ply)
{
	StartGravity(ply);

	if (IsOnGround(ply))
	{
		m_vecVelocity.z = 0;
		Friction(ply);
	}

	CheckVelocity();

	if (IsOnGround(ply))
		WalkMove(ply);
	else
		AirMove(ply);

	CategorizePosition(ply);

	CheckVelocity();

	FinishGravity(ply);

	if (IsOnGround(ply))
		m_vecVelocity.z = 0;
}

void GameMove::PlayerMove(CBaseEntity* ply)
{
	//CategorizePosition(ply);

	if (m_vecVelocity.z > 250.0f)
		SetGroundEntity(ply, nullptr);

	FullWalkMove(ply);
}

void GameMove::RenderMove(CBaseEntity* ply, int ticks)
{
	if (!ply)
		return;

	player_info_t info;

	if (!engine->GetPlayerInfo(ply->GetIndex(), &info))
		return;

	if (ply->IsDormant())
		return;

	if (!ply->IsAlive())
		return;

	InitalState(ply);

	for (int i = 0; i < ticks; i++)
	{
		PlayerMove(ply);

		Vector screen;
		if (draw::WorldToScreen(m_vecAbsOrigin, screen))
			draw::RectOutlined(screen.x - 1, screen.y - 1, 3, 3, 1, IsOnGround(ply) ? Color(52, 152, 219) : Color(52, 219, 152), Color(0, 0, 0));
	}
}