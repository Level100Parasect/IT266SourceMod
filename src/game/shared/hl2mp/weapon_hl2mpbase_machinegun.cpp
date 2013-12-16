//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"

#if defined( CLIENT_DLL )
	#include "c_hl2mp_player.h"
#else
	#include "hl2mp_player.h"
#endif

#include "weapon_hl2mpbase_machinegun.h"
#include "in_buttons.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( HL2MPMachineGun, DT_HL2MPMachineGun )

BEGIN_NETWORK_TABLE( CHL2MPMachineGun, DT_HL2MPMachineGun )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CHL2MPMachineGun )
END_PREDICTION_DATA()

//=========================================================
//	>> CHLSelectFireMachineGun
//=========================================================
BEGIN_DATADESC( CHL2MPMachineGun )
//louis
	DEFINE_FIELD( m_nShotsFired,	FIELD_INTEGER ),

	DEFINE_FIELD( m_nBurstRate, FIELD_INTEGER ),    // Burst fires this many bullets
	DEFINE_FIELD( m_bFMReady, FIELD_BOOLEAN ),      // firing mode change readiness
	DEFINE_FIELD( m_bFMAutomatic, FIELD_BOOLEAN ),  // firing mode change readiness
	DEFINE_FIELD( m_nShotsLeft, FIELD_INTEGER ),    // keeps track of shots left in this firing mode
	DEFINE_FIELD( m_nFireMode, FIELD_INTEGER ),     // keeps track of firing mode

	DEFINE_FIELD( m_flNextSoundTime, FIELD_TIME ),

END_DATADESC()

//louis
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHL2MPMachineGun::CHL2MPMachineGun( void )
{
	m_nFireMode = 0;
	m_nShotsLeft = m_nFireMode;
	m_nBurstRate = 3;
	m_bFMAutomatic = true;
}

const Vector &CHL2MPMachineGun::GetBulletSpread( void )
{
	static Vector cone = VECTOR_CONE_3DEGREES;
	return cone;
}

//louis
//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CHL2MPMachineGun::PrimaryAttack( void )
{
   // If there is less than 1 shot left in this firing mode... return
   if (m_nShotsLeft < 1)
      return;
 
   // If the firing mode is less than four, remove one from the shots left counter...
   if (m_nFireMode < 3)
      m_nShotsLeft--;
 
   // Only the player fires this way so we can cast
   CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
   if (!pPlayer)
      return;
 
   // Abort here to handle burst and auto fire modes
   if ( (UsesClipsForAmmo1() && m_iClip1 == 0) || ( !UsesClipsForAmmo1() && !pPlayer->GetAmmoCount(m_iPrimaryAmmoType) ) )
      return;   
 
   m_nShotsFired++;
 
   if (pPlayer->m_nButtons & IN_ATTACK)
      DevMsg ("There are %d shots left...\\n", m_nShotsLeft);
 
   // MUST call sound before removing a round from the clip of a CHLMachineGun
   // FIXME: only called once, will miss multiple sound events per frame if needed
   // FIXME: m_flNextPrimaryAttack is always in the past, it's not clear what'll happen with sounds
   WeaponSound(SINGLE, m_flNextPrimaryAttack);
   // Msg("%.3f\\n", m_flNextPrimaryAttack.Get() );
 
   pPlayer->DoMuzzleFlash();
 
   // To make the firing framerate independent, we may have to fire more than one bullet here on low-framerate systems,
   // especially if the weapon we're firing has a really fast rate of fire.
   int iBulletsToFire = 0;
   float fireRate = GetFireRate();
 
   while ( m_flNextPrimaryAttack <= gpGlobals->curtime )
   {
      m_flNextPrimaryAttack = m_flNextPrimaryAttack + fireRate;
      iBulletsToFire++;
   }
 
   // Make sure we don't fire more than the amount in the clip, if this weapon uses clips
   if ( UsesClipsForAmmo1() )
   {
      if ( iBulletsToFire > m_iClip1 )
         iBulletsToFire = m_iClip1;
      m_iClip1 -= iBulletsToFire;
   }
 
   CHL2MP_Player *pHL2MPPlayer = ToHL2MPPlayer( pPlayer );
 
   // Fire the bullets
   FireBulletsInfo_t info;
   info.m_iShots = iBulletsToFire;
   info.m_vecSrc = pHL2MPPlayer->Weapon_ShootPosition( );
   info.m_vecDirShooting = pHL2MPPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );
   info.m_vecSpread = pHL2MPPlayer->GetAttackSpread( this );
   info.m_flDistance = MAX_TRACE_LENGTH;
   info.m_iAmmoType = m_iPrimaryAmmoType;
   info.m_iTracerFreq = 2;
   FireBullets( info );
 
   // Factor in the view kick
   AddViewKick();
 
   if (!m_iClip1 && pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
   {
      // HEV suit - indicate out of ammo condition
      pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
   }
 
   SendWeaponAnim( GetPrimaryAttackActivity() );
   pPlayer->SetAnimation( PLAYER_ATTACK1 );

}

//louis
//----------------------------------------------------------
// Purpose: Implementation of firemode in all machine guns
//----------------------------------------------------------
void   CHL2MPMachineGun::FireMode( void ){
 
   // Grab the current player
   CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
   if (!pPlayer) // if there isn't one, return...
      return;
 
   // If player is pushing the firemode button and is okay to change mode,
   if ( ( ( pPlayer->m_nButtons & IN_FIREMODE ) && ( m_bFMReady == true ) ) && !( pPlayer->m_nButtons & IN_ATTACK ) ){
      if ( m_nFireMode == 1 )         // if mode was 0
         m_nFireMode = 2;         // set it to 1
      else if ( m_nFireMode == 2 && m_bFMAutomatic)   // if mode was 1
         m_nFireMode = 3;         // set it to 3
      else							// otherwise
         m_nFireMode = 1;         // set it to automatic
 
      // play weapon sound so you know the button has been pressed
      WeaponSound(EMPTY);
 
      if (m_nFireMode == 2)// sets the counter to the correct amount (depending on firing mode
         m_nShotsLeft = m_nBurstRate;
      else
         m_nShotsLeft = m_nFireMode;
 
      // and set ready to false to keep from runnign through the list over and over
      m_bFMReady = false;         
      DevMsg ("There are %d bullets being fired at a time...\\n", m_nFireMode);
   }
 
   return;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
//-----------------------------------------------------------------------------
void CHL2MPMachineGun::FireBullets( const FireBulletsInfo_t &info )
{
	if(CBasePlayer *pPlayer = ToBasePlayer ( GetOwner() ) )
	{
		pPlayer->FireBullets(info);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2MPMachineGun::DoMachineGunKick( CBasePlayer *pPlayer, float dampEasy, float maxVerticleKickAngle, float fireDurationTime, float slideLimitTime )
{
	#define	KICK_MIN_X			0.2f	//Degrees
	#define	KICK_MIN_Y			0.2f	//Degrees
	#define	KICK_MIN_Z			0.1f	//Degrees

	QAngle vecScratch;
	int iSeed = CBaseEntity::GetPredictionRandomSeed() & 255;
	
	//Find how far into our accuracy degradation we are
	float duration	= ( fireDurationTime > slideLimitTime ) ? slideLimitTime : fireDurationTime;
	float kickPerc = duration / slideLimitTime;

	// do this to get a hard discontinuity, clear out anything under 10 degrees punch
	pPlayer->ViewPunchReset( 10 );

	//Apply this to the view angles as well
	vecScratch.x = -( KICK_MIN_X + ( maxVerticleKickAngle * kickPerc ) );
	vecScratch.y = -( KICK_MIN_Y + ( maxVerticleKickAngle * kickPerc ) ) / 3;
	vecScratch.z = KICK_MIN_Z + ( maxVerticleKickAngle * kickPerc ) / 8;

	RandomSeed( iSeed );

	//Wibble left and right
	if ( RandomInt( -1, 1 ) >= 0 )
		vecScratch.y *= -1;

	iSeed++;

	//Wobble up and down
	if ( RandomInt( -1, 1 ) >= 0 )
		vecScratch.z *= -1;

	//Clip this to our desired min/max
	UTIL_ClipPunchAngleOffset( vecScratch, pPlayer->m_Local.m_vecPunchAngle, QAngle( 24.0f, 3.0f, 1.0f ) );

	//Add it to the view punch
	// NOTE: 0.5 is just tuned to match the old effect before the punch became simulated
	pPlayer->ViewPunch( vecScratch * 0.5 );
}

//louis
//-----------------------------------------------------------------------------
// Purpose: Reset our shots fired
//-----------------------------------------------------------------------------
bool CHL2MPMachineGun::Deploy( void )
{
	m_nShotsFired = 0;
	m_bFMReady = true;
	return BaseClass::Deploy();
}

bool CHL2MPMachineGun::Holster( CBaseCombatWeapon *pSwitchingTo )
{
   m_nShotsLeft = 0;
   m_nFireMode = 0;
   return BaseClass::Holster(pSwitchingTo);
}

//-----------------------------------------------------------------------------
// Purpose: Make enough sound events to fill the estimated think interval
// returns: number of shots needed
//-----------------------------------------------------------------------------
int CHL2MPMachineGun::WeaponSoundRealtime( WeaponSound_t shoot_type )
{
	int numBullets = 0;

	// ran out of time, clamp to current
	if (m_flNextSoundTime < gpGlobals->curtime)
	{
		m_flNextSoundTime = gpGlobals->curtime;
	}

	// make enough sound events to fill up the next estimated think interval
	float dt = clamp( m_flAnimTime - m_flPrevAnimTime, 0, 0.2 );
	if (m_flNextSoundTime < gpGlobals->curtime + dt)
	{
		WeaponSound( SINGLE_NPC, m_flNextSoundTime );
		m_flNextSoundTime += GetFireRate();
		numBullets++;
	}
	if (m_flNextSoundTime < gpGlobals->curtime + dt)
	{
		WeaponSound( SINGLE_NPC, m_flNextSoundTime );
		m_flNextSoundTime += GetFireRate();
		numBullets++;
	}

	return numBullets;
}




//louis
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2MPMachineGun::ItemPostFrame( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
 
   if ( pOwner == NULL )
      return;
 
   // Debounce the recoiling counter
   if (!( pOwner->m_nButtons & IN_ATTACK ))
   { //when not using primary fire
      if (m_nFireMode == 2)// sets the counter to the correct amount (depending on firing mode
         m_nShotsLeft = m_nBurstRate;
      else
         m_nShotsLeft = m_nFireMode;
      m_nShotsFired = 0;
   }
 
   if (!(pOwner->m_nButtons & IN_FIREMODE)) // when not pressing firemode button
      m_bFMReady = true; // set ready to true
 
   BaseClass::ItemPostFrame();
}


