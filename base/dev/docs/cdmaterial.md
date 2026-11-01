# .cdmaterial

`.cdmaterial` files look like:

```
// a comment
[material 1 name] {
    [key] [values...]
    [key] [values...]
}

[material 2 name] {
    [key] [values...]
    [key] [values...]
}
...
```

Comments must be on their own line. All keys are optional, valid keys are:

- `alphagen`/`rgbgen`: Sets the material alpha and color, see below for specifics.
- `blendfunc`: Sets the transparency blending mode, valid values are `blend` and `add`.
- `cull`: Flips or disables backface culling, valid values are `front` and `none` for two-sided materials.
- `shaded`: Makes a material "shaded", mutually exclusive with `world` TODO
- `world`: Makes a material "world", mutually exclusive with `shaded` TODO
- `shininess`: TODO, figure out what the scale is
- `specular`: TODO, figure out what the scale is
- `texture`: A texture name. Defaults to `white`.

## Colors

`alphagen` and `rgbgen` support several modes for coloring materials. They are as follows:

- `alphagen const [alpha]`: Sets a constant alpha, linear in [0, 1].
- `rgbgen const [r] [g] [b]`: Sets a constant color. If all values are in [0, 1], this is a linear
  color. Otherwise, sRGB with values in [0, 255]. TODO support hex colors
- `*gen entity`: Sets the material alpha to the entity's alpha.
- `*gen wave`: See below.

For example: `rgbgen wave sin 0 1 0 1`

### Waves

`alphagen` and `rgbgen` both support "waves", which vary the color over time. The supported "waves"
are:

- `wave sin [base] [amplitude] [phase] [freq]`: Sets alpha to `base + amplitude * sin( freq Hz + phase )`. TODO
- `wave triangle [base] [amplitude] [phase] [freq]`: Sets alpha to `base + amplitude * triangle( freq Hz + phase )`. TODO
- `wave sawtooth [base] [amplitude] [phase] [freq]`: Sets alpha to `base + amplitude * ( freq Hz + phase )`. TODO
- `wave inversesawtooth [base] [amplitude] [phase] [freq]`: Sets alpha to `base + amplitude * ( 1 - freq Hz + phase )`. TODO

`rgbgen` additionally supports `colorwave`, which does nothing, and `entitycolorwave`, which adds
the wave to the entity color.


