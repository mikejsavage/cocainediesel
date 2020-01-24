// use a base shader to assign settings to all mapmodels
// this allows me to change their lightmap quality to
// speed up WIP compile times

terrainmodel_baseshader
{
	q3map_forceMeta
	q3map_nonPlanar
	q3map_clipmodel
	q3map_lightmapMergable

	// lightmap quality settings
	q3map_lightmapSampleOffset 32
	q3map_lightmapsamplesize 16
}

lambert1
{
	qer_editorimage gfx/colors/white
	q3map_baseShader terrainmodel_baseshader
	q3map_shadeangle 60

	{
		map $lightmap
	}

	{
		map $whiteimage
		rgbgen const 0.75 0.75 0.75
		blendFunc filter
	}
}

