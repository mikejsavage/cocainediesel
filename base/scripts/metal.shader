textures/metal/treadplate0015
{
	qer_editorimage textures/metal/treadplate0015.tga

	{
		material textures/metal/treadplate0015.tga
	}
}

textures/metal/treadplate0015b
{
	qer_editorimage textures/metal/treadplate0015b.tga

	{
		material textures/metal/treadplate0015b.tga textures/metal/treadplate0015_norm.tga textures/metal/treadplate0015_gloss.tga
	}
}

textures/metal/treadplate0015e
{
	qer_editorimage textures/metal/treadplate0015e.tga

	{
		material textures/metal/treadplate0015e.tga textures/metal/treadplate0015_norm.tga textures/metal/treadplate0015_gloss.tga
	}
}

textures/metal/treadplate0015f
{
	qer_editorimage textures/metal/treadplate0015f.tga

	{
		material textures/metal/treadplate0015f.tga textures/metal/treadplate0015_norm.tga textures/metal/treadplate0015f_gloss.tga
	}
}

textures/metal/treadplate0015g
{
	qer_editorimage textures/metal/treadplate0015g.tga

	{
		material textures/metal/treadplate0015g.tga textures/metal/treadplate0015_norm.tga textures/metal/treadplate0015f_gloss.tga
	}
}

textures/metal/metalbaseyellow0033
{
	qer_editorimage textures/metal/metalbaseyellow0033.tga
	glossExponent 256

	{
		material textures/metal/metalbaseyellow0033.tga $blankbumpimage textures/metal/metalbaseyellow0033_gloss.tga
	}
}

textures/metal/corrugated0023green
{
	qer_editorimage textures/metal/corrugated0023green.tga
	glossExponent 256

	{
		material textures/metal/corrugated0023green.tga textures/metal/corrugated0023_norm.tga textures/metal/corrugated0023green_gloss.tga
	}
}

textures/metal/corrugated0023oxid
{
	qer_editorimage textures/metal/corrugated0023oxid.tga
	glossExponent 256

	{
		material textures/metal/corrugated0023oxid.tga textures/metal/corrugated0023_norm.tga textures/metal/corrugated0023_gloss.tga
	}
}

textures/metal/aluminium
{
	qer_editorimage textures/metal/aluminium.tga

	glossExponent 24
	glossIntensity 0.9

	{
		material textures/metal/aluminium.tga $blankbumpimage textures/metal/aluminium_gloss.tga
	}
}

textures/metal/chrome
{
	qer_editorimage textures/metal/chrome
	surfaceparm nolightmap
	surfaceparm nomarks
	{
		map env/2d/chrome1.tga
		tcGen environment
		rgbgen vertex
	}

	{
		map $whiteImage
		rgbgen vertex
		alphagen const 0.25
		blendFunc blend
	}

	{
		map env/2d/chrome1.tga
		tcGen environment
		rgbgen vertex
		alphagen const 0.6
		blendFunc GL_SRC_ALPHA GL_ONE // blendfunc add the alphamasked part only
	}
}

textures/metal/chrome_rusty
{
	qer_editorimage textures/metal/chrome
	surfaceparm nolightmap
	surfaceparm nomarks
	{
		map env/2d/chrome1.tga
		tcGen environment
		rgbgen vertex
	}

	{
		map $whiteImage
		rgbgen vertex
		alphagen const 0.25
		blendFunc blend
	}

	{
		map env/2d/chrome1.tga
		tcGen environment
		rgbgen vertex
		alphagen const 0.6
		blendFunc GL_SRC_ALPHA GL_ONE // blendfunc add the alphamasked part only
	}

	{
		map textures/metal/chrome.blend.tga
		rgbgen vertex
		blendFunc blend
	}
}
