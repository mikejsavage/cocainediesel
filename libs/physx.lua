prebuilt_lib( "physx", {
	"LowLevelDynamics_static_64",
	"LowLevel_static_64",
	"PhysXCharacterKinematic_static_64",
	"PhysXCommon_64",
	"PhysXExtensions_static_64",
	"PhysXFoundation_64",
	config == "debug" and "PhysXPvdSDK_static_64" or { },
	"PhysXTask_static_64",
	"PhysXVehicle_static_64",
	"PhysX_64",
	"SceneQuery_static_64"
} )

prebuilt_lib( "physx_cooking", {
	"PhysXCooking_64",
}, "physx" )
