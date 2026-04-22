#include "SDL3TextureManager.h"
#include "Graphics.h"
#include "SexyAppBase.h"
#include "DDInterface.h"
#include "SexyVector.h"
#include "SexyMatrix.h"
#include <SDL3/SDL.h>
#include <vector>

using namespace Sexy;

// ============================================================
//  Internal Helpers
// ============================================================

static inline void SetupBlendMode(Image* srcImage, SDL_Texture* tex, int drawMode, const Color& theColor)
{
    if (drawMode == Graphics::DRAWMODE_ADDITIVE)
    {
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_ADD);
        return;
    }

    MemoryImage* aMemImage = dynamic_cast<MemoryImage*>(srcImage);
    bool hasAlpha = aMemImage && (aMemImage->mHasAlpha || aMemImage->mHasTrans);

    if (hasAlpha || theColor.mAlpha < 255)
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    else
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_NONE);
}

static inline void SetupColorMod(SDL_Texture* tex, const Color& theColor)
{
    SDL_SetTextureColorMod(tex, (Uint8)theColor.mRed, (Uint8)theColor.mGreen, (Uint8)theColor.mBlue);
    SDL_SetTextureAlphaMod(tex, (Uint8)theColor.mAlpha);
}

static inline void ResetColorMod(SDL_Texture* tex)
{
    SDL_SetTextureColorMod(tex, 255, 255, 255);
    SDL_SetTextureAlphaMod(tex, 255);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
}

// ============================================================
//  Lifecycle
// ============================================================

void SDL3TextureManager::SetRenderer(SDL_Renderer* theRenderer)
{
    if (mRenderer == theRenderer) return;
    Cleanup();
    mRenderer = theRenderer;
}

void SDL3TextureManager::Cleanup()
{
    for (auto& pair : mTextureMap)
    {
        if (pair.second) SDL_DestroyTexture(pair.second);
    }
    mTextureMap.clear();
    mSyncCount.clear();

    if (mStreamTex) {
        SDL_DestroyTexture(mStreamTex);
        mStreamTex = nullptr;
    }
    mRenderTarget = nullptr;
    mRenderer = nullptr;
}

// ============================================================
//  Frame Management
// ============================================================

void SDL3TextureManager::EndFrame()
{
    if (!mRenderer) return;

    DDImage* screenImage = nullptr;
    extern SexyAppBase* gSexyAppBase;
    if (gSexyAppBase && gSexyAppBase->mDDInterface)
        screenImage = (DDImage*)gSexyAppBase->mDDInterface->mScreenImage;

    if (!screenImage) return;

    SDL_Texture* screenTex = nullptr;
    auto it = mTextureMap.find(screenImage);
    if (it != mTextureMap.end()) screenTex = it->second;

    // Perform final upscale from screen texture to the backbuffer
    SDL_SetRenderTarget(mRenderer, nullptr);
    SDL_SetRenderDrawColor(mRenderer, 0, 0, 0, 255);
    SDL_RenderClear(mRenderer);

    if (screenTex)
    {
        SDL_SetTextureBlendMode(screenTex, SDL_BLENDMODE_NONE);
        SDL_SetTextureScaleMode(screenTex, SDL_SCALEMODE_LINEAR);
        SDL_RenderTexture(mRenderer, screenTex, nullptr, nullptr);
    }

    SDL_RenderPresent(mRenderer);
    mRenderTarget = nullptr;
}

void SDL3TextureManager::FlushFrame(ulong* theBits, int theWidth, int theHeight)
{
    if (!mRenderer || !theBits || theWidth <= 0 || theHeight <= 0) return;

    float tw = 0, th = 0;
    if (mStreamTex) {
        SDL_GetTextureSize(mStreamTex, &tw, &th);
        if ((int)tw != theWidth || (int)th != theHeight) {
            SDL_DestroyTexture(mStreamTex);
            mStreamTex = nullptr;
        }
    }

    if (!mStreamTex) {
        mStreamTex = SDL_CreateTexture(mRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, theWidth, theHeight);
        SDL_SetTextureBlendMode(mStreamTex, SDL_BLENDMODE_NONE);
    }

    SDL_UpdateTexture(mStreamTex, nullptr, theBits, theWidth * sizeof(ulong));

    SDL_SetRenderTarget(mRenderer, nullptr);
    SDL_SetRenderDrawColor(mRenderer, 0, 0, 0, 255);
    SDL_RenderClear(mRenderer);
    SDL_RenderTexture(mRenderer, mStreamTex, nullptr, nullptr);
    SDL_RenderPresent(mRenderer);
}

// ============================================================
//  Target Management
// ============================================================

void SDL3TextureManager::VerifyTarget(DDImage* destImage)
{
    if (!destImage) return;
    if (mRenderTarget != destImage)
        SetRenderTarget(destImage);
}

void SDL3TextureManager::SetRenderTarget(DDImage* theTargetImage)
{
    if (!mRenderer || !theTargetImage) return;
    mRenderTarget = theTargetImage;

    // Ensure texture exists for the target
    auto it = mTextureMap.find(theTargetImage);
    if (it == mTextureMap.end())
    {
        if (!GenerateTextureForImage(theTargetImage)) return;
        it = mTextureMap.find(theTargetImage);
    }

    // HYBRID SYNC: If this is the screen buffer and software bits changed,
    // upload them to the GPU texture before drawing hardware sprites on top.
    // Optimization: Only do this if software rendering is actually active (mDrawToBits).
    if (theTargetImage->mIsScreenBuffer && theTargetImage->mDrawToBits && theTargetImage->mBits &&
        mSyncCount[theTargetImage] != theTargetImage->mBitsChangedCount)
    {
        // Ensure streaming tex matches target dimensions
        float tw = 0, th = 0;
        if (mStreamTex) {
            SDL_GetTextureSize(mStreamTex, &tw, &th);
            if ((int)tw != theTargetImage->mWidth || (int)th != theTargetImage->mHeight) {
                SDL_DestroyTexture(mStreamTex);
                mStreamTex = nullptr;
            }
        }
        if (!mStreamTex)
            mStreamTex = SDL_CreateTexture(mRenderer, SDL_PIXELFORMAT_ARGB8888, 
                SDL_TEXTUREACCESS_STREAMING, theTargetImage->mWidth, theTargetImage->mHeight);

        SDL_UpdateTexture(mStreamTex, nullptr, theTargetImage->mBits,
            theTargetImage->mWidth * sizeof(ulong));
        mSyncCount[theTargetImage] = theTargetImage->mBitsChangedCount;

        SDL_SetRenderTarget(mRenderer, it->second);
        SDL_SetTextureBlendMode(mStreamTex, SDL_BLENDMODE_NONE);
        SDL_SetTextureScaleMode(mStreamTex, SDL_SCALEMODE_LINEAR);
        SDL_FRect r = { 0, 0, (float)theTargetImage->mWidth, (float)theTargetImage->mHeight };
        SDL_RenderTexture(mRenderer, mStreamTex, &r, &r);
    }

    SDL_SetRenderTarget(mRenderer, it->second);
}

// ============================================================
//  Texture Lifecycle
// ============================================================

SDL_Texture* SDL3TextureManager::GetTextureForImage(Image* theImage)
{
    if (!mRenderer || !theImage) return nullptr;

    auto it = mTextureMap.find(theImage);
    if (it == mTextureMap.end())
    {
        if (!GenerateTextureForImage(theImage)) return nullptr;
        it = mTextureMap.find(theImage);
    }

    // Sync if bits have changed since last upload
    MemoryImage* aMemImage = dynamic_cast<MemoryImage*>(theImage);
    if (aMemImage && !aMemImage->mIsScreenBuffer)
    {
        ulong* bits = aMemImage->mBits ? aMemImage->mBits : aMemImage->GetBits();
        if (bits)
        {
            bool needsUpdate = aMemImage->mBitsChanged ||
                               mSyncCount.find(theImage) == mSyncCount.end() ||
                               mSyncCount[theImage] != aMemImage->mBitsChangedCount;
            if (needsUpdate)
                UpdateTextureForImage(theImage);
        }
    }

    return it->second;
}

bool SDL3TextureManager::GenerateTextureForImage(Image* theImage)
{
    if (!mRenderer || !theImage) return false;
    if (mTextureMap.count(theImage)) return true;

    MemoryImage* aMemImage = dynamic_cast<MemoryImage*>(theImage);
    if (!aMemImage) return false;

    // Screen/render targets use TARGET access. Everything else uses STATIC.
    SDL_TextureAccess access = aMemImage->mIsScreenBuffer
        ? SDL_TEXTUREACCESS_TARGET
        : SDL_TEXTUREACCESS_STATIC;

    SDL_Texture* aTex = SDL_CreateTexture(mRenderer, SDL_PIXELFORMAT_ARGB8888, access,
                                          theImage->mWidth, theImage->mHeight);
    if (!aTex) return false;

    mTextureMap[theImage] = aTex;
    mSyncCount[theImage] = -1;

    SDL_SetTextureScaleMode(aTex, SDL_SCALEMODE_LINEAR);

    if (aMemImage->mIsScreenBuffer)
    {
        // Clear the render target to opaque black immediately
        SDL_Texture* prevTarget = SDL_GetRenderTarget(mRenderer);
        SDL_SetRenderTarget(mRenderer, aTex);
        SDL_SetRenderDrawColor(mRenderer, 0, 0, 0, 255);
        SDL_RenderClear(mRenderer);
        SDL_SetRenderTarget(mRenderer, prevTarget);
        SDL_SetTextureBlendMode(aTex, SDL_BLENDMODE_NONE);
    }
    else
    {
        // Try to get bits (forces lazy loading for PAK-backed images)
        ulong* bits = aMemImage->mBits ? aMemImage->mBits : aMemImage->GetBits();
        if (bits)
        {
            // Initial analysis to detect transparency
            aMemImage->mBitsChanged = true;
            aMemImage->CommitBits();

            // Alpha repair for "lying" JPEGs: if a static asset is 100% transparent, it's actually opaque.
            // We only do this for fresh loads (mBitsChangedCount == 0) to avoid breaking dynamic images.
            if (aMemImage->mBitsChangedCount == 0)
            {
                int total = aMemImage->mWidth * aMemImage->mHeight;
                bool allZero = true;
                for (int i = 0; i < total; i++) {
                    if ((bits[i] & 0xFF000000) != 0) { allZero = false; break; }
                }
                if (allZero) {
                    for (int i = 0; i < total; i++) bits[i] |= 0xFF000000;
                    aMemImage->mHasTrans = false;
                    aMemImage->mHasAlpha = false;
                }
            }
            
            SDL_UpdateTexture(aTex, nullptr, bits, theImage->mWidth * sizeof(ulong));
            mSyncCount[theImage] = aMemImage->mBitsChangedCount;
            aMemImage->mBitsChanged = false;
        }
        else
        {
            // No bits available - default to BLEND so we don't paint solid black
            SDL_SetTextureBlendMode(aTex, SDL_BLENDMODE_BLEND);
        }
    }

    return true;
}

void SDL3TextureManager::UpdateTextureForImage(Image* theImage)
{
    auto it = mTextureMap.find(theImage);
    if (it == mTextureMap.end()) return;

    MemoryImage* aMemImage = dynamic_cast<MemoryImage*>(theImage);
    if (!aMemImage) return;

    // Force lazy loading for PAK-backed images
    ulong* bits = aMemImage->mBits ? aMemImage->mBits : aMemImage->GetBits();
    if (!bits) return;

    // Scan for transparency changes - must happen BEFORE clearing mBitsChanged
    if (aMemImage->mBitsChanged)
    {
        aMemImage->CommitBits();
        aMemImage->mBitsChanged = false;
    }

    // No repair here, only on initial load
    SDL_UpdateTexture(it->second, nullptr, bits, theImage->mWidth * sizeof(ulong));
    mSyncCount[theImage] = aMemImage->mBitsChangedCount;
}

void SDL3TextureManager::DeleteTextureForImage(Image* theImage)
{
    auto it = mTextureMap.find(theImage);
    if (it != mTextureMap.end())
    {
        if (it->second)
        {
            if (mRenderer) SDL_FlushRenderer(mRenderer);
            SDL_DestroyTexture(it->second);
        }
        mTextureMap.erase(it);
        mSyncCount.erase(theImage);
    }
}

void SDL3TextureManager::Remove(Image* theImage)
{
    DeleteTextureForImage(theImage);
}

// ============================================================
//  Draw Operations
// ============================================================

void SDL3TextureManager::Blt(DDImage* theDestImage, Image* theImage, int theX, int theY, const Rect& theSrcRect, const Color& theColor, int theDrawMode)
{
    VerifyTarget(theDestImage);
    SDL_Texture* aTex = GetTextureForImage(theImage);
    if (!aTex) return;

    SDL_FRect srcR = { (float)theSrcRect.mX, (float)theSrcRect.mY, (float)theSrcRect.mWidth, (float)theSrcRect.mHeight };
    SDL_FRect dstR = { (float)theX, (float)theY, (float)theSrcRect.mWidth, (float)theSrcRect.mHeight };

    SetupBlendMode(theImage, aTex, theDrawMode, theColor);
    SetupColorMod(aTex, theColor);
    SDL_RenderTexture(mRenderer, aTex, &srcR, &dstR);
    ResetColorMod(aTex);
}

void SDL3TextureManager::BltF(DDImage* theDestImage, Image* theImage, float theX, float theY, const Rect& theSrcRect, const Rect& theClipRect, const Color& theColor, int theDrawMode)
{
    VerifyTarget(theDestImage);
    SDL_Texture* aTex = GetTextureForImage(theImage);
    if (!aTex) return;

    SDL_FRect srcR = { (float)theSrcRect.mX, (float)theSrcRect.mY, (float)theSrcRect.mWidth, (float)theSrcRect.mHeight };
    SDL_FRect dstR = { theX, theY, (float)theSrcRect.mWidth, (float)theSrcRect.mHeight };

    SetupBlendMode(theImage, aTex, theDrawMode, theColor);
    SetupColorMod(aTex, theColor);

    SDL_Rect clipRect = { theClipRect.mX, theClipRect.mY, theClipRect.mWidth, theClipRect.mHeight };
    SDL_SetRenderClipRect(mRenderer, &clipRect);
    SDL_RenderTexture(mRenderer, aTex, &srcR, &dstR);
    SDL_SetRenderClipRect(mRenderer, nullptr);
    ResetColorMod(aTex);
}

void SDL3TextureManager::StretchBlt(DDImage* theDestImage, Image* theImage, const Rect& theDestRect, const Rect& theSrcRect, const Rect& theClipRect, const Color& theColor, int theDrawMode, bool fastStretch)
{
    VerifyTarget(theDestImage);
    SDL_Texture* aTex = GetTextureForImage(theImage);
    if (!aTex) return;

    SDL_FRect srcR = { (float)theSrcRect.mX, (float)theSrcRect.mY, (float)theSrcRect.mWidth, (float)theSrcRect.mHeight };
    SDL_FRect dstR = { (float)theDestRect.mX, (float)theDestRect.mY, (float)theDestRect.mWidth, (float)theDestRect.mHeight };

    SDL_SetTextureScaleMode(aTex, fastStretch ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR);
    SetupBlendMode(theImage, aTex, theDrawMode, theColor);
    SetupColorMod(aTex, theColor);

    SDL_Rect clipRect = { theClipRect.mX, theClipRect.mY, theClipRect.mWidth, theClipRect.mHeight };
    SDL_SetRenderClipRect(mRenderer, &clipRect);
    SDL_RenderTexture(mRenderer, aTex, &srcR, &dstR);
    SDL_SetRenderClipRect(mRenderer, nullptr);
    ResetColorMod(aTex);
}

void SDL3TextureManager::BltRotated(DDImage* theDestImage, Image* theImage, float theX, float theY, const Rect& theSrcRect, const Rect& theClipRect, const Color& theColor, int theDrawMode, double theRot, float theRotCenterX, float theRotCenterY)
{
    VerifyTarget(theDestImage);
    SDL_Texture* aTex = GetTextureForImage(theImage);
    if (!aTex) return;

    SDL_FRect srcR = { (float)theSrcRect.mX, (float)theSrcRect.mY, (float)theSrcRect.mWidth, (float)theSrcRect.mHeight };
    SDL_FRect dstR = { theX, theY, (float)theSrcRect.mWidth, (float)theSrcRect.mHeight };
    SDL_FPoint center = { theRotCenterX, theRotCenterY };

    SetupBlendMode(theImage, aTex, theDrawMode, theColor);
    SetupColorMod(aTex, theColor);

    SDL_Rect clipRect = { theClipRect.mX, theClipRect.mY, theClipRect.mWidth, theClipRect.mHeight };
    SDL_SetRenderClipRect(mRenderer, &clipRect);
    SDL_RenderTextureRotated(mRenderer, aTex, &srcR, &dstR, theRot * (180.0 / SDL_PI_D), &center, SDL_FLIP_NONE);
    SDL_SetRenderClipRect(mRenderer, nullptr);
    ResetColorMod(aTex);
}

void SDL3TextureManager::BltMirror(DDImage* theDestImage, Image* theImage, int theX, int theY, const Rect& theSrcRect, const Color& theColor, int theDrawMode)
{
    VerifyTarget(theDestImage);
    SDL_Texture* aTex = GetTextureForImage(theImage);
    if (!aTex) return;

    SDL_FRect srcR = { (float)theSrcRect.mX, (float)theSrcRect.mY, (float)theSrcRect.mWidth, (float)theSrcRect.mHeight };
    SDL_FRect dstR = { (float)theX, (float)theY, (float)theSrcRect.mWidth, (float)theSrcRect.mHeight };

    SetupBlendMode(theImage, aTex, theDrawMode, theColor);
    SetupColorMod(aTex, theColor);
    SDL_RenderTextureRotated(mRenderer, aTex, &srcR, &dstR, 0, nullptr, SDL_FLIP_HORIZONTAL);
    ResetColorMod(aTex);
}

void SDL3TextureManager::StretchBltMirror(DDImage* theDestImage, Image* theImage, const Rect& theDestRect, const Rect& theSrcRect, const Rect& theClipRect, const Color& theColor, int theDrawMode, bool fastStretch)
{
    VerifyTarget(theDestImage);
    SDL_Texture* aTex = GetTextureForImage(theImage);
    if (!aTex) return;

    SDL_FRect srcR = { (float)theSrcRect.mX, (float)theSrcRect.mY, (float)theSrcRect.mWidth, (float)theSrcRect.mHeight };
    SDL_FRect dstR = { (float)theDestRect.mX, (float)theDestRect.mY, (float)theDestRect.mWidth, (float)theDestRect.mHeight };

    SDL_SetTextureScaleMode(aTex, fastStretch ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR);
    SetupBlendMode(theImage, aTex, theDrawMode, theColor);
    SetupColorMod(aTex, theColor);

    SDL_Rect clipRect = { theClipRect.mX, theClipRect.mY, theClipRect.mWidth, theClipRect.mHeight };
    SDL_SetRenderClipRect(mRenderer, &clipRect);
    SDL_RenderTextureRotated(mRenderer, aTex, &srcR, &dstR, 0, nullptr, SDL_FLIP_HORIZONTAL);
    SDL_SetRenderClipRect(mRenderer, nullptr);
    ResetColorMod(aTex);
}

void SDL3TextureManager::BltMatrix(DDImage* theDestImage, Image* theImage, float x, float y, const SexyMatrix3& theMatrix, const Rect& theClipRect, const Color& theColor, int theDrawMode, const Rect& theSrcRect, bool blend)
{
    VerifyTarget(theDestImage);
    SDL_Texture* aTex = GetTextureForImage(theImage);
    if (!aTex) return;

    float w2 = theSrcRect.mWidth  / 2.0f;
    float h2 = theSrcRect.mHeight / 2.0f;

    float u0 = (float)theSrcRect.mX / theImage->mWidth;
    float u1 = (float)(theSrcRect.mX + theSrcRect.mWidth)  / theImage->mWidth;
    float v0 = (float)theSrcRect.mY / theImage->mHeight;
    float v1 = (float)(theSrcRect.mY + theSrcRect.mHeight) / theImage->mHeight;

    SDL_Vertex verts[4];
    float us[] = { u0, u1, u0, u1 };
    float vs[] = { v0, v0, v1, v1 };
    float px[] = { -w2,  w2, -w2, w2 };
    float py[] = { -h2, -h2,  h2, h2 };

    // Modulate vertex colors by the global color tint
    SDL_FColor tint = { theColor.mRed / 255.0f, theColor.mGreen / 255.0f, theColor.mBlue / 255.0f, theColor.mAlpha / 255.0f };

    for (int i = 0; i < 4; i++)
    {
        SexyVector3 vec(px[i], py[i], 1);
        vec = theMatrix * vec;
        verts[i].position.x  = vec.x + x;
        verts[i].position.y  = vec.y + y;
        verts[i].tex_coord.x = us[i];
        verts[i].tex_coord.y = vs[i];
        verts[i].color       = tint;
    }

    int indices[6] = { 0, 1, 2, 1, 3, 2 };

    SetupBlendMode(theImage, aTex, theDrawMode, theColor);
    // SDL_SetTextureColorMod is often ignored in RenderGeometry, so vertex color is the primary tint
    SDL_SetTextureColorMod(aTex, 255, 255, 255); 
    SDL_SetTextureAlphaMod(aTex, 255);

    SDL_Rect clipRect = { theClipRect.mX, theClipRect.mY, theClipRect.mWidth, theClipRect.mHeight };
    SDL_SetRenderClipRect(mRenderer, &clipRect);
    SDL_RenderGeometry(mRenderer, aTex, verts, 4, indices, 6);
    SDL_SetRenderClipRect(mRenderer, nullptr);
    ResetColorMod(aTex);
}

void SDL3TextureManager::BltTrianglesTex(DDImage* theDestImage, Image* theTexture, const TriVertex theVertices[][3], int theNumTriangles, const Rect& theClipRect, const Color& theColor, int theDrawMode, float tx, float ty, bool blend)
{
    VerifyTarget(theDestImage);
    SDL_Texture* aTex = GetTextureForImage(theTexture);
    if (!aTex || theNumTriangles <= 0) return;

    int totalVerts = theNumTriangles * 3;
    std::vector<SDL_Vertex> sdlVerts(totalVerts);
    std::vector<int> indices(totalVerts);

    for (int i = 0; i < theNumTriangles; i++)
    {
        for (int v = 0; v < 3; v++)
        {
        const TriVertex& vert = theVertices[i][v];
            int idx = i * 3 + v;
            sdlVerts[idx].position.x  = vert.x + tx;
            sdlVerts[idx].position.y  = vert.y + ty;
            sdlVerts[idx].tex_coord.x = vert.u;
            sdlVerts[idx].tex_coord.y = vert.v;

            // TriVertex.color == 0 means "use the color from the function call" (theColor).
            // When color != 0, we multiply it by theColor tint.
            float r, g, b, a;
            if (vert.color == 0) { r = 1.0f; g = 1.0f; b = 1.0f; a = 1.0f; }
            else
            {
                r = ((vert.color >> 16) & 0xFF) / 255.0f;
                g = ((vert.color >>  8) & 0xFF) / 255.0f;
                b = ( vert.color        & 0xFF) / 255.0f;
                a = ((vert.color >> 24) & 0xFF) / 255.0f;
            }

            sdlVerts[idx].color.r = r * (theColor.mRed / 255.0f);
            sdlVerts[idx].color.g = g * (theColor.mGreen / 255.0f);
            sdlVerts[idx].color.b = b * (theColor.mBlue / 255.0f);
            sdlVerts[idx].color.a = a * (theColor.mAlpha / 255.0f);
            indices[idx] = idx;
        }
    }

    SetupBlendMode(theTexture, aTex, theDrawMode, theColor);
    SetupColorMod(aTex, theColor);

    SDL_Rect clipRect = { theClipRect.mX, theClipRect.mY, theClipRect.mWidth, theClipRect.mHeight };
    SDL_SetRenderClipRect(mRenderer, &clipRect);
    SDL_RenderGeometry(mRenderer, aTex, sdlVerts.data(), totalVerts, indices.data(), totalVerts);
    SDL_SetRenderClipRect(mRenderer, nullptr);
    ResetColorMod(aTex);
}

void SDL3TextureManager::FillRect(DDImage* theDestImage, const Rect& theRect, const Color& theColor, int theDrawMode)
{
    VerifyTarget(theDestImage);

    SDL_FRect dstR = { (float)theRect.mX, (float)theRect.mY, (float)theRect.mWidth, (float)theRect.mHeight };

    SDL_SetRenderDrawBlendMode(mRenderer, theDrawMode == Graphics::DRAWMODE_ADDITIVE ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(mRenderer, (Uint8)theColor.mRed, (Uint8)theColor.mGreen, (Uint8)theColor.mBlue, (Uint8)theColor.mAlpha);
    SDL_RenderFillRect(mRenderer, &dstR);
    SDL_SetRenderDrawBlendMode(mRenderer, SDL_BLENDMODE_BLEND);
}

void SDL3TextureManager::DrawLine(DDImage* theDestImage, double theStartX, double theStartY, double theEndX, double theEndY, const Color& theColor, int theDrawMode)
{
    VerifyTarget(theDestImage);

    SDL_SetRenderDrawBlendMode(mRenderer, theDrawMode == Graphics::DRAWMODE_ADDITIVE ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(mRenderer, (Uint8)theColor.mRed, (Uint8)theColor.mGreen, (Uint8)theColor.mBlue, (Uint8)theColor.mAlpha);
    SDL_RenderLine(mRenderer, (float)theStartX, (float)theStartY, (float)theEndX, (float)theEndY);
    SDL_SetRenderDrawBlendMode(mRenderer, SDL_BLENDMODE_BLEND);
}
