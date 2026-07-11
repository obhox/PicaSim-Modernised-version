#include "GltfLoader.h"

#include "Helpers.h"
#include "Trace.h"

#include <string>
#include <vector>
#include <cstring>
#include <cmath>

// cgltf is a single-header library; define the implementation in exactly this
// one translation unit (declarations only elsewhere). Vendored under
// source/thirdparty/cgltf (MIT, by Johannes Kuhlmann) so there is no vcpkg
// dependency, matching how the Hosek sky model is bundled.
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

//======================================================================================================================
// glTF (Y-up, -Z forward, right handed) -> PicaSim (Z-up, +X forward, +Y left).
//   px = -gz   (glTF forward -Z  -> PicaSim forward +X)
//   py = -gx   (glTF right  +X   -> PicaSim right   -Y, i.e. left is +Y)
//   pz =  gy   (glTF up     +Y   -> PicaSim up      +Z)
// This is a pure rotation (det +1) so it preserves handedness and winding.
static inline Vector3 GltfAxisToPicaSim(float gx, float gy, float gz)
{
    return Vector3(-gz, -gx, gy);
}

//======================================================================================================================
// Transform a position by a column-major 4x4 world matrix (as produced by
// cgltf_node_transform_world), then convert to PicaSim axes.
static Vector3 TransformPosition(const float m[16], float x, float y, float z)
{
    float wx = m[0] * x + m[4] * y + m[8]  * z + m[12];
    float wy = m[1] * x + m[5] * y + m[9]  * z + m[13];
    float wz = m[2] * x + m[6] * y + m[10] * z + m[14];
    return GltfAxisToPicaSim(wx, wy, wz);
}

//======================================================================================================================
// Transform a normal/direction by the upper-left 3x3 of the world matrix, then
// convert to PicaSim axes and normalise. NOTE: this uses the plain rotation part,
// which is correct for rigid transforms and uniform scale (the common aircraft
// case). Non-uniform node scaling would require the inverse-transpose; that is
// not handled and is flagged in the loader documentation.
static Vector3 TransformNormal(const float m[16], float x, float y, float z)
{
    float wx = m[0] * x + m[4] * y + m[8]  * z;
    float wy = m[1] * x + m[5] * y + m[9]  * z;
    float wz = m[2] * x + m[6] * y + m[10] * z;
    Vector3 n = GltfAxisToPicaSim(wx, wy, wz);
    float len = n.GetLength();
    if (len > 1e-8f)
        n *= (1.0f / len);
    return n;
}

//======================================================================================================================
// Per-triangle tangent from positions + UVs (Lengyel), matching the AC3D path in
// RenderModel.cpp. Used only when a glTF mesh has a normal map but no TANGENT
// attribute. The shader re-orthogonalises against the interpolated normal, so a
// flat per-triangle tangent is sufficient.
static Vector3 ComputeGltfTangent(const Vector3& p0, const Vector3& p1, const Vector3& p2,
                                  const float uv0[2], const float uv1[2], const float uv2[2])
{
    Vector3 e1 = p1 - p0;
    Vector3 e2 = p2 - p0;
    float du1 = uv1[0] - uv0[0], dv1 = uv1[1] - uv0[1];
    float du2 = uv2[0] - uv0[0], dv2 = uv2[1] - uv0[1];
    float det = du1 * dv2 - du2 * dv1;
    if (det > -1e-12f && det < 1e-12f)
        return Vector3(1.0f, 0.0f, 0.0f);
    Vector3 t = (e1 * dv2 - e2 * dv1) * (1.0f / det);
    if (t.GetLengthSquared() < 1e-16f)
        return Vector3(1.0f, 0.0f, 0.0f);
    return t.GetNormalised();
}

//======================================================================================================================
static const cgltf_accessor* FindAttribute(const cgltf_primitive* prim, cgltf_attribute_type type, int setIndex)
{
    for (cgltf_size a = 0; a < prim->attributes_count; ++a)
    {
        const cgltf_attribute& attr = prim->attributes[a];
        if (attr.type == type && attr.index == setIndex)
            return attr.data;
    }
    return 0;
}

//======================================================================================================================
// Minimal base64 decode for image data URIs. Appends decoded bytes to 'out'.
static void Base64Decode(const char* b64, size_t len, std::vector<unsigned char>& out)
{
    // Build the reverse lookup once (loading is single-threaded).
    static bool init = false;
    static int8_t rev[256];
    if (!init)
    {
        for (int i = 0; i < 256; ++i) rev[i] = -1;
        const char* alpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (int i = 0; i < 64; ++i) rev[(unsigned char) alpha[i]] = (int8_t) i;
        init = true;
    }

    int val = 0, bits = -8;
    for (size_t i = 0; i < len; ++i)
    {
        int8_t c = rev[(unsigned char) b64[i]];
        if (c < 0) continue;                 // skip '=', whitespace, newlines
        val = (val << 6) | c;
        bits += 6;
        if (bits >= 0)
        {
            out.push_back((unsigned char) ((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
}

//======================================================================================================================
// Resolve a glTF image to a loadable source: an external file path, or in-memory
// encoded bytes for GLB buffer-view images and base64 data-URI images. Leaves
// 'out' empty() only if the image is missing/unreadable.
static void ExtractImage(const cgltf_data* data, const cgltf_image* image,
                         const std::string& gltfDir, const std::string& modelFile,
                         RenderModel::GltfImageSource& out)
{
    if (!image)
        return;

    // Stable cache key: image name if present, else model file + image index.
    size_t imgIndex = (data && data->images) ? (size_t) (image - data->images) : 0;
    std::string key = image->name ? std::string(image->name)
                                   : (modelFile + "#img" + std::to_string(imgIndex));

    // 1) GLB / buffer-view embedded: encoded bytes live in a loaded buffer.
    if (image->buffer_view)
    {
        const uint8_t* p = cgltf_buffer_view_data(image->buffer_view);
        cgltf_size sz = image->buffer_view->size;
        if (p && sz)
        {
            out.mBytes.assign(p, p + sz);
            out.mName = key;
        }
        return;
    }

    if (!image->uri)
        return;

    // 2) base64 data URI: data:[<mime>];base64,<payload>
    if (std::strncmp(image->uri, "data:", 5) == 0)
    {
        const char* comma = std::strchr(image->uri, ',');
        if (comma)
        {
            const char* b64 = comma + 1;
            Base64Decode(b64, std::strlen(b64), out.mBytes);
            if (!out.mBytes.empty())
                out.mName = key;
        }
        return;
    }

    // 3) external file: percent-decode the URI relative to the model directory.
    std::string uri = image->uri;
    std::vector<char> tmp(uri.begin(), uri.end());
    tmp.push_back('\0');
    cgltf_decode_uri(&tmp[0]);
    out.mPath = gltfDir + std::string(&tmp[0]);
    out.mName = out.mPath;
}

//======================================================================================================================
bool LoadGltfModel(
    RenderModel::GltfModelData& out,
    const std::string&          filename,
    const Vector3&              offset,
    float                       modelScale,
    float                       colourOffset)
{
    out.clear();

    cgltf_options options;
    std::memset(&options, 0, sizeof(options));

    cgltf_data* data = 0;
    cgltf_result result = cgltf_parse_file(&options, filename.c_str(), &data);
    if (result != cgltf_result_success)
    {
        TRACE("GltfLoader: failed to parse %s (cgltf error %d)", filename.c_str(), (int) result);
        return false;
    }

    // Load .bin buffers / GLB binary chunk / data URIs referenced by accessors.
    result = cgltf_load_buffers(&options, data, filename.c_str());
    if (result != cgltf_result_success)
    {
        TRACE("GltfLoader: failed to load buffers for %s (cgltf error %d)", filename.c_str(), (int) result);
        cgltf_free(data);
        return false;
    }

    // Directory of the model file, used to resolve external texture URIs.
    std::string gltfDir;
    std::string::size_type lastSlash = filename.find_last_of("/\\");
    if (lastSlash != std::string::npos)
        gltfDir = filename.substr(0, lastSlash + 1);

    int synthName = 0;

    // Walk every node that carries a mesh. cgltf_node_transform_world folds the
    // whole parent chain in, so the node hierarchy is baked into the vertices
    // (RenderModel components are otherwise flat, exactly like the AC3D path).
    for (cgltf_size n = 0; n < data->nodes_count; ++n)
    {
        const cgltf_node* node = &data->nodes[n];
        if (!node->mesh)
            continue;

        float world[16];
        cgltf_node_transform_world(node, world);

        std::string nodeName = node->name ? node->name : "";

        for (cgltf_size p = 0; p < node->mesh->primitives_count; ++p)
        {
            const cgltf_primitive* prim = &node->mesh->primitives[p];
            if (prim->type != cgltf_primitive_type_triangles)
                continue; // only triangle lists are supported

            const cgltf_accessor* posAcc   = FindAttribute(prim, cgltf_attribute_type_position, 0);
            const cgltf_accessor* normAcc  = FindAttribute(prim, cgltf_attribute_type_normal,   0);
            const cgltf_accessor* uvAcc    = FindAttribute(prim, cgltf_attribute_type_texcoord, 0);
            const cgltf_accessor* colAcc   = FindAttribute(prim, cgltf_attribute_type_color,    0);
            const cgltf_accessor* tanAcc   = FindAttribute(prim, cgltf_attribute_type_tangent,  0);
            if (!posAcc)
                continue;

            RenderModel::GltfComponentData component;
            component.mName = !nodeName.empty() ? nodeName
                                                : (std::string("Gltf") + std::to_string(synthName++));

            // --- Material -> PBR-lite params + base colour + base-colour/normal maps ---
            float baseColour[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
            float roughness = 0.6f;
            float metallic  = 0.0f;
            if (prim->material)
            {
                if (prim->material->has_pbr_metallic_roughness)
                {
                    const cgltf_pbr_metallic_roughness& mr = prim->material->pbr_metallic_roughness;
                    baseColour[0] = mr.base_color_factor[0];
                    baseColour[1] = mr.base_color_factor[1];
                    baseColour[2] = mr.base_color_factor[2];
                    baseColour[3] = mr.base_color_factor[3];
                    roughness = mr.roughness_factor;
                    metallic  = mr.metallic_factor;
                    if (mr.base_color_texture.texture && mr.base_color_texture.texture->image)
                        ExtractImage(data, mr.base_color_texture.texture->image, gltfDir, filename, component.mBaseColor);
                }
                if (prim->material->normal_texture.texture && prim->material->normal_texture.texture->image)
                    ExtractImage(data, prim->material->normal_texture.texture->image, gltfDir, filename, component.mNormal);
            }

            bool textured = !component.mBaseColor.empty();
            bool hasNormalMap = !component.mNormal.empty();
            // Tangents are only meaningful with a normal map. Compute per-triangle
            // when the mesh lacks a TANGENT attribute.
            bool computeTangents = hasNormalMap && !tanAcc;

            component.mRoughness = ClampToRange(roughness, 0.04f, 1.0f);
            component.mMetallic  = ClampToRange(metallic, 0.0f, 1.0f);

            // Number of triangles (indexed or sequential).
            cgltf_size indexCount = prim->indices ? prim->indices->count : posAcc->count;
            for (cgltf_size i = 0; i + 3 <= indexCount; i += 3)
            {
                RenderModel::TexturedVertex tri[3];   // buffered so a per-triangle tangent can be derived
                for (int c = 0; c < 3; ++c)
                {
                    cgltf_size vi = prim->indices ? cgltf_accessor_read_index(prim->indices, i + c)
                                                  : (i + c);

                    float pos[3] = { 0, 0, 0 };
                    cgltf_accessor_read_float(posAcc, vi, pos, 3);
                    Vector3 point = TransformPosition(world, pos[0], pos[1], pos[2]);
                    point = ComponentMultiply(point, Vector3(modelScale, modelScale, modelScale)) + offset;

                    Vector3 normal(0, 0, 1);
                    if (normAcc)
                    {
                        float nrm[3] = { 0, 0, 1 };
                        cgltf_accessor_read_float(normAcc, vi, nrm, 3);
                        normal = TransformNormal(world, nrm[0], nrm[1], nrm[2]);
                    }

                    float vcol[4] = { baseColour[0], baseColour[1], baseColour[2], baseColour[3] };
                    if (colAcc)
                    {
                        float rc[4] = { 1, 1, 1, 1 };
                        cgltf_accessor_read_float(colAcc, vi, rc, 4);
                        vcol[0] = baseColour[0] * rc[0];
                        vcol[1] = baseColour[1] * rc[1];
                        vcol[2] = baseColour[2] * rc[2];
                        vcol[3] = baseColour[3] * rc[3];
                    }
                    OffsetColour(vcol, colourOffset);

                    // Always populate the untextured (colour) vertices - they are
                    // the graceful fallback if the texture fails to load.
                    RenderModel::UntexturedVertex uv;
                    uv.mPoint = point;
                    uv.mNormal = normal;
                    uv.mColour[0] = vcol[0];
                    uv.mColour[1] = vcol[1];
                    uv.mColour[2] = vcol[2];
                    uv.mColour[3] = vcol[3];
                    component.mUntexturedVertices.push_back(uv);

                    if (textured)
                    {
                        RenderModel::TexturedVertex& tv = tri[c];
                        tv.mPoint = point;
                        tv.mNormal = normal;
                        float uvw[2] = { 0, 0 };
                        if (uvAcc)
                            cgltf_accessor_read_float(uvAcc, vi, uvw, 2);
                        // glTF texcoords use a top-left origin; flip V to match
                        // the AC3D path's (1 - v) convention through the shared
                        // (unflipped stb) Texture loader.
                        tv.mTexCoord[0] = uvw[0];
                        tv.mTexCoord[1] = 1.0f - uvw[1];
                        // Tangent: use the mesh's TANGENT attribute if present
                        // (ignore the .w handedness, matching the AC3D path),
                        // otherwise leave zero and derive per-triangle below.
                        Vector3 tangent(0.0f, 0.0f, 0.0f);
                        if (tanAcc)
                        {
                            float tg[4] = { 1, 0, 0, 1 };
                            cgltf_accessor_read_float(tanAcc, vi, tg, 4);
                            tangent = TransformNormal(world, tg[0], tg[1], tg[2]);
                        }
                        tv.mTangent = tangent;
                    }
                }

                if (textured)
                {
                    if (computeTangents)
                    {
                        Vector3 t = ComputeGltfTangent(
                            tri[0].mPoint, tri[1].mPoint, tri[2].mPoint,
                            tri[0].mTexCoord, tri[1].mTexCoord, tri[2].mTexCoord);
                        tri[0].mTangent = tri[1].mTangent = tri[2].mTangent = t;
                    }
                    component.mTexturedVertices.push_back(tri[0]);
                    component.mTexturedVertices.push_back(tri[1]);
                    component.mTexturedVertices.push_back(tri[2]);
                }
            }

            if (!component.mUntexturedVertices.empty())
                out.push_back(component);
        }
    }

    cgltf_free(data);

    if (out.empty())
    {
        TRACE("GltfLoader: %s produced no drawable geometry", filename.c_str());
        return false;
    }
    return true;
}
