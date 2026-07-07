#include "Skybox.h"

#include <filesystem>

#include "RenderManager.h"
#include "LoadingScreenHelper.h"
#include "Graphics.h"
#include "Helpers.h"
#include "ShaderManager.h"
#include "Shaders.h"
#include "Viewport.h"
#include "Trace.h"

//======================================================================================================================
Skybox::Skybox() :
    mOffset(0.0f),
    mExtendBelowHorizon(1.0f),
    mInitialised(false)
{}

//======================================================================================================================
Skybox::~Skybox()
{
}

//======================================================================================================================
bool Skybox::Init(const char* skyboxPath, bool use16BitTextures, int maxDetail, LoadingScreenHelper* loadingScreen)
{
    TRACE_METHOD_ONLY(1);
    if (loadingScreen) loadingScreen->Update("Skybox");

    if (mInitialised)
        Terminate();

    char filename[256];
    char loadingTxt[256];

    const char* sideNames[] = {"up", "front", "left", "back", "right", "down"};

    int detail = maxDetail;
    while (detail >= 1)
    {
        sprintf(filename, "%s/%d/front1.jpg", skyboxPath, detail);
        if (std::filesystem::exists(filename))
        {
            break;
        }
        --detail;
    }

    if (detail < 1)
    {
        TRACE_FILE_IF(1) TRACE("Failed to find Skybox files for %s", skyboxPath);
        return false;
    }

    for (int iSide = 0 ; iSide != NUM_SIDES ; ++iSide)
    {
        for (int iImage = 1 ; ; ++iImage)
        {
            sprintf(filename, "%s/%d/%s%d.jpg", skyboxPath, detail, sideNames[iSide], iImage);
            if (!std::filesystem::exists(filename))
            {
                break;
            }

            sprintf(loadingTxt, "Image %s%d", sideNames[iSide], iImage);
            if (loadingScreen) loadingScreen->Update(loadingTxt);

            Texture* texture = new Texture;
            mTextures[iSide].push_back(texture);

            LoadTextureFromFile(*texture, filename);
            if (loadingScreen) loadingScreen->Update();
            texture->SetMipMapping(false);
            texture->SetModifiable(false);
            if (use16BitTextures && texture->GetWidth() > 512)
                texture->SetFormatHW(CIwImage::RGB_565);
            texture->Upload();
            TRACE_FILE_IF(1) TRACE("Uploaded texture %s id %d", filename, texture->mHWID);

            if (texture->GetFlags() & Texture::UPLOADED_F)
            {
                // Letting Marmalade handle the filtering/clamping doesn't seem to work... in particular clamping doesn't, so 
                // force it manually and re-upload
                glBindTexture(GL_TEXTURE_2D, texture->mHWID);

                // filtering
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

                if (loadingScreen) loadingScreen->Update();
                texture->Upload();
                TRACE_FILE_IF(1) TRACE("Uploaded texture %s id %d", filename, texture->mHWID);
            }

        }

    }
    if (loadingScreen) loadingScreen->Update();

    RenderManager::GetInstance().RegisterRenderObject(this, RENDER_LEVEL_SKYBOX);

    mInitialised = true;
    return true;
}

//======================================================================================================================
void Skybox::Terminate()
{
    TRACE_METHOD_ONLY(1);
    if (mInitialised)
        RenderManager::GetInstance().UnregisterRenderObject(this, RENDER_LEVEL_SKYBOX);

    for (int iSide = 0 ; iSide != NUM_SIDES ; ++iSide)
    {
        while (!mTextures[iSide].empty())
        {
            delete mTextures[iSide].back();
            mTextures[iSide].pop_back();
        }
    }
    mInitialised = false;
}

//======================================================================================================================
// Avoids clipping with the near plane
float scale = 100.0f;
static GLfloat pts[] = {
    scale,  scale,  scale,
    scale, -scale,  scale, 
    scale, -scale, -scale, 
    scale,  scale, -scale, 
};

static GLfloat uvs[] = {
    0.0f, 0.0f,
    1.0f, 0.0f,
    1.0f, 1.0f,
    0.0f, 1.0f,
};

//======================================================================================================================
void Skybox::DrawSide(Side side, int mvpLoc) const
{
    float fNumPerSide = sqrtf((float) mTextures[side].size());
    int numPerSide = (int) (fNumPerSide + 0.5f);

    float imageScale = 1.0f / numPerSide;
    esScalef(1.0f, imageScale, imageScale);

    size_t index = 0;
    for (int j = 0 ; j != numPerSide ; ++j)
    {
        for (int i = 0 ; i != numPerSide ; ++i)
        {
            if (index < mTextures[side].size())
            {
                Texture* texture = mTextures[side][index];
                if (texture && texture->GetFlags() & Texture::UPLOADED_F)
                {
                    glBindTexture(GL_TEXTURE_2D, texture->mHWID);
                    float y = scale * (numPerSide - (i * 2 + 1.0f));
                    float z = scale * (numPerSide - (j * 2 + 1.0f));
                    esPushMatrix();
                    esTranslatef(0.0f, y, z);
                    esSetModelViewProjectionMatrix(mvpLoc);
                    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
                    esPopMatrix();
                }
            }
            ++index;
        }
    }

}

//======================================================================================================================
void Skybox::RenderUpdate(class Viewport* viewport, int renderLevel)
{
    TRACE_METHOD_ONLY(2);
    esPushMatrix();

    glFrontFace(GL_CW);

    DisableDepthMask disableDepthMask;
    DisableDepthTest disableDepthTest;

    DisableFog disableFog;

    const SkyboxShader* skyboxShader = (SkyboxShader*) ShaderManager::GetInstance().GetShader(SHADER_SKYBOX);
    {
        skyboxShader->Use();
        glUniform1i(skyboxShader->u_texture, 0);
    }

    Vector3 pos = viewport->GetCamera()->GetPosition();
    esTranslatef(pos.x, pos.y, pos.z);

    esRotatef(-mOffset, 0, 0, 1);
      
    glActiveTexture(GL_TEXTURE0);

    {
        // Upload the quad's positions + texcoords into the shared streaming VBO
        // and point the attributes at byte offsets (core profile has no
        // client-side arrays). The 6 DrawSide() calls below reuse these.
        gStreamVBO.Bind();
        gStreamVBO.Reserve(sizeof(pts) + sizeof(uvs));
        size_t posOffset = gStreamVBO.Upload(pts, sizeof(pts));
        size_t uvOffset  = gStreamVBO.Upload(uvs, sizeof(uvs));

        glVertexAttribPointer(skyboxShader->a_position, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid*)posOffset);
        glEnableVertexAttribArray(skyboxShader->a_position);

        glVertexAttribPointer(skyboxShader->a_texCoord, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid*)uvOffset);
        glEnableVertexAttribArray(skyboxShader->a_texCoord);
    }

#if 1
    // front
    {
        esPushMatrix();
        DrawSide(FRONT, skyboxShader->u_mvpMatrix);
        esPopMatrix();
    }
#endif

#if 1
    // right
    {
        esPushMatrix();
        ROTATE_270_Z;
        DrawSide(RIGHT, skyboxShader->u_mvpMatrix);
        esPopMatrix();
    }
#endif

#if 1
    // back
    {
        esPushMatrix();
        ROTATE_180_Z;
        DrawSide(BACK, skyboxShader->u_mvpMatrix);
        esPopMatrix();
    }
#endif

#if 1
    // left
    {
        esPushMatrix();
        ROTATE_90_Z;
        DrawSide(LEFT, skyboxShader->u_mvpMatrix);
        esPopMatrix();
    }
#endif

#if 1
    // up
    {
        esPushMatrix();
        ROTATE_270_Y;
        DrawSide(UP, skyboxShader->u_mvpMatrix);
        esPopMatrix();
    }
#endif

#if 1
    // down
    {
        esPushMatrix();
        ROTATE_90_Y;
        DrawSide(DOWN, skyboxShader->u_mvpMatrix);
        esPopMatrix();
    }
#endif

    {
        glDisableVertexAttribArray(skyboxShader->a_position);
        glDisableVertexAttribArray(skyboxShader->a_texCoord);
        // Unbind so draw sites still using client-side arrays (not yet converted)
        // continue to interpret their pointers as CPU addresses.
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    glFrontFace(GL_CCW);

    esPopMatrix();
}

