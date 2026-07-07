#ifndef GLTFLOADER_H
#define GLTFLOADER_H

#include "RenderModel.h"
#include <string>

//======================================================================================================================
// glTF 2.0 model loader.
//
// Parses a .gltf or .glb file with cgltf and produces a RenderModel::GltfModelData
// (a neutral, cgltf-free intermediate) that RenderModel::Init(GltfModelData,...)
// turns into the exact same RenderModel::Component structures the AC3D loader
// produces. This lets community content ship modern glTF assets that feed the same
// PBR / CSM / component-animation path as the legacy .ac models.
//
// Mapping summary (see GltfLoader.cpp for detail):
//   - Geometry:  POSITION, NORMAL, TEXCOORD_0 (+ COLOR_0 if present) + indices.
//   - Material:  metallic-roughness -> metallicFactor->mMetallic,
//                roughnessFactor->mRoughness, baseColorFactor->vertex colour,
//                baseColorTexture->mTexture (external-URI images only).
//   - Nodes:     each node's WORLD transform is baked into its vertices; the node
//                name becomes the component name, so a glTF node named to match a
//                <ControlSurface name="..."> animates exactly like the AC3D object.
//   - Axis:      glTF is Y-up / -Z-forward (right handed); PicaSim is Z-up / +X
//                forward / +Y left. Conversion: (px,py,pz) = (-gz, -gx, gy).
//
// modelScale and offset are applied after the axis conversion, matching the AC3D
// path (offset + vertex * modelScale). colourOffset applies an HSV shift to the
// untextured vertex colours (textures are shifted later in RenderModel::Init).
//
// Returns true on success. On failure it logs and leaves out empty so the caller
// can fall back / report gracefully without crashing.

bool LoadGltfModel(
    RenderModel::GltfModelData& out,
    const std::string&          filename,
    const Vector3&              offset,
    float                       modelScale,
    float                       colourOffset);

#endif // GLTFLOADER_H
