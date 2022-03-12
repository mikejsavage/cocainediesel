#pragma once

enum ClientCommandType : u8 {
	ClientCommand_New,
	ClientCommand_ConfigStrings,
	ClientCommand_Baselines,
	ClientCommand_Begin,
	ClientCommand_Disconnect,
	ClientCommand_NoDelta,
	ClientCommand_UserInfo,

	ClientCommand_Position,
	ClientCommand_Say,
	ClientCommand_SayTeam,
	ClientCommand_Noclip,
	ClientCommand_Suicide,
	ClientCommand_Spectate,
	ClientCommand_ChaseNext,
	ClientCommand_ChasePrev,
	ClientCommand_ToggleFreeFly,
	ClientCommand_Timeout,
	ClientCommand_Timein,
	ClientCommand_DemoList,
	ClientCommand_DemoGetURL,
	ClientCommand_Callvote,
	ClientCommand_Vote,
	ClientCommand_Operator,
	ClientCommand_OpCall,
	ClientCommand_Ready,
	ClientCommand_Unready,
	ClientCommand_ToggleReady,
	ClientCommand_Join,
	ClientCommand_TypewriterClack,
	ClientCommand_TypewriterSpace,
	ClientCommand_Spray,
	ClientCommand_Vsay,

	ClientCommand_DropBomb,
	ClientCommand_LoadoutMenu,
	ClientCommand_SetLoadout,

	ClientCommand_Count
};
