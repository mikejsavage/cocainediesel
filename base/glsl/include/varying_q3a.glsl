qf_varying vec3 v_Position;
qf_varying vec2 v_TexCoord;

#if defined(APPLY_DRAWFLAT)
qf_varying vec3 v_Normal;
#endif

#if defined(APPLY_SOFT_PARTICLE)
qf_varying float v_Depth;
#endif
