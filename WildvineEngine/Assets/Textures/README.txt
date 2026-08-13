Wildvine Engine - textures

Supported through stb_image: PNG, JPG/JPEG, TGA, BMP and other stb_image formats.
DDS uses the legacy D3DX11 loader when available.

MTL mappings recognized by Wildvine V10:
map_Kd            -> Albedo/Base Color
map_Bump / bump   -> Normal
norm / map_Kn     -> Normal
map_Pm            -> Metallic
map_Pr            -> Roughness
map_Ka / map_AO   -> Ambient Occlusion
map_Ke            -> Emissive

Scalar MTL values recognized:
Kd, Ke, d, Tr, Ns, Pm and Pr.

Missing files never abort the engine: a generated fallback texture is used instead.
