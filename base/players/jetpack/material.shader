players/jetpack/body
{
    shaded
    specular 10
    shininess 8
    {
        map $whiteimage
        rgbgen entity
    }
}

players/jetpack/helmet
{
  {
    blendfunc add
    map $whiteimage
    rgbGen entity
    alphaGen const 0.25
  }
}