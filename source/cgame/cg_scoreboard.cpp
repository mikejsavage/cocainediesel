#include "cgame/cg_local.h"

void CG_ScoresOn_f() {
	if( cgs.demoPlaying || cg.frame.multipov ) {
		cg.showScoreboard = true;
	}
	else {
		trap_Cmd_ExecuteText( EXEC_NOW, "svscore 1" );
	}
}

void CG_ScoresOff_f() {
	if( cgs.demoPlaying || cg.frame.multipov ) {
		cg.showScoreboard = false;
	}
	else {
		trap_Cmd_ExecuteText( EXEC_NOW, "svscore 0" );
	}
}
