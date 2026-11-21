# .cdtexture

`.cdtexture` files are used to define texture properties. They apply to textures with the same name,
minus the file extension. So `path/a.cdtexture` affects both `path/a.png` and `path/a.dds`.

`.cdtexture` files can either be empty, or contain the word `sprite`. The `sprite` keyword is
required for a texture to be used as a particle or decal, including sprays (the spray converter
automatically generates cdtextures).
