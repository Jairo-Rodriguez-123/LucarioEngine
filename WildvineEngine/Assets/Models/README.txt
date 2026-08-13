Wildvine Engine - OBJ assets

Place .obj and .mtl files here, or in subfolders.
Recommended layout:

Assets/Models/MyModel/MyModel.obj
Assets/Models/MyModel/MyModel.mtl
Assets/Textures/MyModel/MyModel_Albedo.png
Assets/Textures/MyModel/MyModel_Normal.png
Assets/Textures/MyModel/MyModel_Metallic.png
Assets/Textures/MyModel/MyModel_Roughness.png
Assets/Textures/MyModel/MyModel_AO.png
Assets/Textures/MyModel/MyModel_Emissive.png

The OBJ must reference its material library with:
mtllib MyModel.mtl

The faces can select materials with:
usemtl MaterialName

Wildvine V10 supports multiple usemtl sections/submeshes and falls back safely when a texture is missing.
