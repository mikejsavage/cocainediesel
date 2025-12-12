#include "qcommon/time.h"

#include "cgame/cg_local.h"

template <typename T>
struct Tween {
	T start, end;
	Time start_time, end_time;
};


template <typename T>
static Tween<T> StartTween( T start, T end, Time time ) {
	return Tween<T>{ start, end, cls.game_time, Time( cls.game_time.flicks + time.flicks ) };
}


template <typename T>
static T TweenLerp( const Tween<T>& tween, float x ) {
	return Lerp( tween.start, x, tween.end );
}

static float Ramp( Time start, Time now, Time end ) {
	return Min2( 1.f, float( now.flicks - start.flicks ) / float( end.flicks - start.flicks ) );
}



//
// Interpolation functions
//

template <typename T>
static T TweenLinear( const Tween<T>& tween, Time now ) {
	return TweenLerp( tween, Ramp( tween.start_time, now, tween.end_time ) );
}

template <typename T>
static T TweenSquareIn( const Tween<T>& tween, Time now ) {
	float x = Ramp( tween.start_time, now, tween.end_time );
	return TweenLerp( tween, x * x );
}

template <typename T>
static T TweenSquareOut( const Tween<T>& tween, Time now ) {
	float x = Ramp( tween.start_time, now, tween.end_time ) - 1.f;
	return TweenLerp( tween, 1.f - x * x );
}





static bool IsChasing( const SyncPlayerState * ps ) {
	return cls.cgameActive && !CL_DemoPlaying() && ps->team != Team_None && ps->POVnum != cgs.playerNum + 1;
}


void CG_HandlePlayerTweens() {
	const SyncPlayerState * ps = &cg.frame.playerState;
	const SyncPlayerState * ops = &cg.oldFrame.playerState;
	PlayerTweenState * state = &cg.animationState;

	Time now = cls.game_time;

	// chase tween
	{
		static Tween<float> tween_chase;
		
		constexpr Time CHASING_TIME = Milliseconds( 250 );
		bool ps_chasing = IsChasing( ps );
		bool ops_chasing = IsChasing( ops );

		if( ps_chasing != ops_chasing ) {
			float start = ps_chasing ? 0.f : 1.f;
			tween_chase = StartTween<float>( start, 1.f - start, CHASING_TIME );
		}

		state->chasing = TweenSquareOut( tween_chase, now );
	}
}