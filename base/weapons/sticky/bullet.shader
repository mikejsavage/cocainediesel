weapons/sticky/spikes
{
	shaded
	roughness 0.3
	metallic 1
	anisotropic 0.5
	{
		map $whiteimage
		rgbGen const 0.2 0.2 0.2
	}
}

weapons/sticky/ball
{
	{
		map $whiteimage
		rgbgen entitycolorwave 0.7 0.7 0.7 sin 0.25 0.25 0.25 10
	}
}
