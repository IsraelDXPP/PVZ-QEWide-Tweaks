#include "SDL3TextureManager.h"
#include "Graphics.h"
#include "SexyAppBase.h"
#include "DDInterface.h"
#include "SexyVector.h"
#include "SexyMatrix.h"
#include <SDL3/SDL.h>
#include <vector>

using namespace Sexy;

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
    mRenderer = nullptr; 
}

void SDL3TextureManager::EndFrame()
{
    if (!mRenderer) return;

    // Grab the Screen Image representation
    DDImage* screenImage = nullptr;
    extern SexyAppBase* gSexyAppBase;
    if (gSexyAppBase && gSexyAppBase->mDDInterface)
        screenImage = (DDImage*)gSexyAppBase->mDDInterface->mScreenImage;

    if (!screenImage) return;

    SDL_Texture* screenTex = GetTextureForImage(screenImage);
    if (!screenTex) return;

    // Final Presentation from Screen Texture to Backbuffer
    SDL_SetRenderTarget(mRenderer, nullptr);
    SDL_SetRenderDrawColor(mRenderer, 0, 0, 0, 255);
    SDL_RenderClear(mRenderer);
    
    // Present the fully composited screen texture
    SDL_SetTextureBlendMode(screenTex, SDL_BLENDMODE_NONE);
    SDL_RenderTexture(mRenderer, screenTex, nullptr, nullptr);
    
    SDL_RenderPresent(mRenderer);

    // CRITICAL: Reset the target pointer so the next frame's first draw call 
    // triggers VerifyTarget -> SetRenderTarget -> Hybrid Sync.
    mRenderTarget = nullptr;
}

void SDL3TextureManager::VerifyTarget(DDImage* destImage)
{
    if (destImage)
    {
        if (mRenderTarget != destImage)
            SetRenderTarget(destImage);
        else if (destImage->mIsScreenBuffer && mSyncCount[destImage] != destImage->mBitsChangedCount)
            SetRenderTarget(destImage); // Re-trigger sync if screen bits changed
    }
}

void SDL3TextureManager::SetRenderTarget(DDImage* theTargetImage)
{
    if (!mRenderer || !theTargetImage) return;
    mRenderTarget = theTargetImage;
    
    SDL_Texture* aTex = GetTextureForImage(theTargetImage);
    
    // HYBRID SYNC: If this is the screen and bits changed in software, upload BEFORE drawing GPU sprites.
    // This prevents the software background from overwriting hardware sprites later.
    if (theTargetImage->mIsScreenBuffer && theTargetImage->mBits && mSyncCount[theTargetImage] != theTargetImage->mBitsChangedCount)
    {
        if (!mStreamTex) {
            mStreamTex = SDL_CreateTexture(mRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, theTargetImage->mWidth, theTargetImage->mHeight);
        }
        SDL_UpdateTexture(mStreamTex, nullptr, theTargetImage->mBits, theTargetImage->mWidth * sizeof(ulong));
        mSyncCount[theTargetImage] = theTargetImage->mBitsChangedCount;

        // Base the texture on the current software bits
        SDL_SetRenderTarget(mRenderer, aTex);
        SDL_SetTextureBlendMode(mStreamTex, SDL_BLENDMODE_NONE);
        SDL_RenderTexture(mRenderer, mStreamTex, nullptr, nullptr);
    }

    SDL_SetRenderTarget(mRenderer, aTex);
}

SDL_Texture* SDL3TextureManager::GetTextureForImage(Image* theImage)
{
    if (!mRenderer || !theImage) return nullptr;

    auto it = mTextureMap.find(theImage);
    if (it == mTextureMap.end())
    {
        if (!GenerateTextureForImage(theImage)) return nullptr;
        it = mTextureMap.find(theImage);
    }

    MemoryImage* aMemImage = dynamic_cast<MemoryImage*>(theImage);
    if (aMemImage)
    {
        // Don't perform standard SDL_UpdateTexture on the screen buffer here.
        // We handle screen/target synchronization exclusively in SetRenderTarget 
        // using the hybrid sync to avoid API conflicts.
        if (!aMemImage->mIsScreenBuffer && mSyncCount[theImage] != aMemImage->mBitsChangedCount)
        {
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

    // Use mIsScreenBuffer flag for robust target identification
    bool isTarget = aMemImage->mIsScreenBuffer;

    SDL_TextureAccess access = isTarget ? SDL_TEXTUREACCESS_TARGET : SDL_TEXTUREACCESS_STATIC;
    SDL_Texture* aTex = SDL_CreateTexture(mRenderer, SDL_PIXELFORMAT_ARGB8888, access, theImage->mWidth, theImage->mHeight);
    
    if (!aTex) return false;

    mTextureMap[theImage] = aTex;
    mSyncCount[theImage] = aMemImage->mBitsChangedCount;

    // Use Linear scaling for backgrounds and images to prevent artifacts
    SDL_SetTextureScaleMode(aTex, SDL_SCALEMODE_LINEAR);
    
    if (isTarget)
    {
        // For render targets (Screen), clear to Opaque Black immediately to avoid transparency bugs
        SDL_SetRenderTarget(mRenderer, aTex);
        SDL_SetRenderDrawColor(mRenderer, 0, 0, 0, 255);
        SDL_RenderClear(mRenderer);
        SDL_SetRenderTarget(mRenderer, mRenderTarget ? GetTextureForImage(mRenderTarget) : nullptr);
        SDL_SetTextureBlendMode(aTex, SDL_BLENDMODE_NONE); // The screen itself shouldn't blend with anything
    }
    else
    {
        SDL_SetTextureBlendMode(aTex, SDL_BLENDMODE_BLEND);
    }

    // Initial upload if bits exist
    if (aMemImage->mBits)
    {
        SDL_UpdateTexture(aTex, nullptr, aMemImage->mBits, theImage->mWidth * sizeof(ulong));
    }

    return true;
}

void SDL3TextureManager::UpdateTextureForImage(Image* theImage)
{
    auto it = mTextureMap.find(theImage);
    if (it == mTextureMap.end()) return;

    MemoryImage* aMemImage = dynamic_cast<MemoryImage*>(theImage);
    if (aMemImage && aMemImage->mBits)
    {
        // Only CommitBits (expensive scan) if the image isn't the screen buffer
        // Screen buffer is handled by hybrid sync and is assumed opaque
        if (!aMemImage->mIsScreenBuffer)
            aMemImage->CommitBits();

        SDL_UpdateTexture(it->second, nullptr, aMemImage->mBits, theImage->mWidth * sizeof(ulong));
        mSyncCount[theImage] = aMemImage->mBitsChangedCount;
    }
}

void SDL3TextureManager::DeleteTextureForImage(Image* theImage)
{
    auto it = mTextureMap.find(theImage);
    if (it != mTextureMap.end())
    {
        if (it->second) SDL_DestroyTexture(it->second);
        mTextureMap.erase(it);
        mSyncCount.erase(theImage);
    }
}

void SDL3TextureManager::Remove(Image* theImage)
{
    DeleteTextureForImage(theImage);
}

static inline void SetupBlendMode(Image* srcImage, SDL_Texture* tex, int drawMode, const Color& theColor)
{
    if (!srcImage || !tex) return;

    if (drawMode == Graphics::DRAWMODE_ADDITIVE)
    {
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_ADD);
    }
    else
    {
        // En lugar de intentar adivinar si es opaco, usa siempre BLEND 
        // a menos que sea el fondo de la pantalla.
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    }

    SDL_SetTextureColorMod(tex, theColor.mRed, theColor.mGreen, theColor.mBlue);
    SDL_SetTextureAlphaMod(tex, theColor.mAlpha);
}

void SDL3TextureManager::Blt(DDImage* theDestImage, Image* theImage, int theX, int theY, const Rect& theSrcRect, const Color& theColor, int theDrawMode)
{
    VerifyTarget(theDestImage);
    SDL_Texture* aTex = GetTextureForImage(theImage);
    if (!aTex) return;

    SDL_FRect srcR = { (float)theSrcRect.mX, (float)theSrcRect.mY, (float)theSrcRect.mWidth, (float)theSrcRect.mHeight };
    SDL_FRect dstR = { (float)theX, (float)theY, (float)theSrcRect.mWidth, (float)theSrcRect.mHeight };

    SetupBlendMode(theImage, aTex, theDrawMode, theColor);
    SDL_RenderTexture(mRenderer, aTex, &srcR, &dstR);
}

void SDL3TextureManager::StretchBlt(DDImage* theDestImage, Image* theImage, const Rect& theDestRect, const Rect& theSrcRect, const Rect& theClipRect, const Color& theColor, int theDrawMode, bool fastStretch)
{
    VerifyTarget(theDestImage);
    SDL_Texture* aTex = GetTextureForImage(theImage);
    if (!aTex) return;

    SDL_FRect srcR = { (float)theSrcRect.mX, (float)theSrcRect.mY, (float)theSrcRect.mWidth, (float)theSrcRect.mHeight };
    SDL_FRect dstR = { (float)theDestRect.mX, (float)theDestRect.mY, (float)theDestRect.mWidth, (float)theDestRect.mHeight };

    SDL_Rect clipRect = { theClipRect.mX, theClipRect.mY, theClipRect.mWidth, theClipRect.mHeight };
    SDL_SetRenderClipRect(mRenderer, &clipRect);

    if (fastStretch)
        SDL_SetTextureScaleMode(aTex, SDL_SCALEMODE_NEAREST);
    else
        SDL_SetTextureScaleMode(aTex, SDL_SCALEMODE_LINEAR);

    SetupBlendMode(theImage, aTex, theDrawMode, theColor);
    SDL_RenderTexture(mRenderer, aTex, &srcR, &dstR);

    SDL_SetRenderClipRect(mRenderer, nullptr);
}

void SDL3TextureManager::BltF(DDImage* theDestImage, Image* theImage, float theX, float theY, const Rect& theSrcRect, const Rect &theClipRect, const Color& theColor, int theDrawMode)
{
    VerifyTarget(theDestImage);
    SDL_Texture* aTex = GetTextureForImage(theImage);
    if (!aTex) return;

    SDL_FRect srcR = { (float)theSrcRect.mX, (float)theSrcRect.mY, (float)theSrcRect.mWidth, (float)theSrcRect.mHeight };
    SDL_FRect dstR = { theX, theY, (float)theSrcRect.mWidth, (float)theSrcRect.mHeight };

    SDL_Rect clipRect = { theClipRect.mX, theClipRect.mY, theClipRect.mWidth, theClipRect.mHeight };
    SDL_SetRenderClipRect(mRenderer, &clipRect);

    SetupBlendMode(theImage, aTex, theDrawMode, theColor);
    SDL_RenderTexture(mRenderer, aTex, &srcR, &dstR);

    SDL_SetRenderClipRect(mRenderer, nullptr);
}

void SDL3TextureManager::BltRotated(DDImage* theDestImage, Image* theImage, float theX, float theY, const Rect &theSrcRect, const Rect& theClipRect, const Color& theColor, int theDrawMode, double theRot, float theRotCenterX, float theRotCenterY)
{
    VerifyTarget(theDestImage);
    SDL_Texture* aTex = GetTextureForImage(theImage);
    if (!aTex) return;

    SDL_FRect srcR = { (float)theSrcRect.mX, (float)theSrcRect.mY, (float)theSrcRect.mWidth, (float)theSrcRect.mHeight };
    SDL_FRect dstR = { theX, theY, (float)theSrcRect.mWidth, (float)theSrcRect.mHeight };

    SDL_Rect clipRect = { theClipRect.mX, theClipRect.mY, theClipRect.mWidth, theClipRect.mHeight };
    SDL_SetRenderClipRect(mRenderer, &clipRect);

    SDL_FPoint center = { theRotCenterX, theRotCenterY };

    SetupBlendMode(theImage, aTex, theDrawMode, theColor);
    SDL_RenderTextureRotated(mRenderer, aTex, &srcR, &dstR, theRot * (180.0 / SDL_PI_D), &center, SDL_FLIP_NONE);

    SDL_SetRenderClipRect(mRenderer, nullptr);
}

void SDL3TextureManager::BltMatrix(DDImage* theDestImage, Image* theImage, float x, float y, const SexyMatrix3 &theMatrix, const Rect& theClipRect, const Color& theColor, int theDrawMode, const Rect &theSrcRect, bool blend)
{
    VerifyTarget(theDestImage);
    SDL_Texture* aTex = GetTextureForImage(theImage);
    if (!aTex) return;

    SDL_Rect clipRect = { theClipRect.mX, theClipRect.mY, theClipRect.mWidth, theClipRect.mHeight };
    SDL_SetRenderClipRect(mRenderer, &clipRect);

    float w2 = theSrcRect.mWidth / 2.0f;
    float h2 = theSrcRect.mHeight / 2.0f;

    float u0 = (float)theSrcRect.mX / theImage->mWidth;
    float u1 = (float)(theSrcRect.mX + theSrcRect.mWidth) / theImage->mWidth;
    float v0 = (float)theSrcRect.mY / theImage->mHeight;
    float v1 = (float)(theSrcRect.mY + theSrcRect.mHeight) / theImage->mHeight;

    SDL_Vertex verts[4];
    float u[] = { u0, u1, u0, u1 };
    float v[] = { v0, v0, v1, v1 };
    float px[] = { -w2, w2, -w2, w2 };
    float py[] = { -h2, -h2, h2, h2 };

    SDL_FColor rgba = { 
        theColor.mRed / 255.0f, 
        theColor.mGreen / 255.0f, 
        theColor.mBlue / 255.0f, 
        theColor.mAlpha / 255.0f 
    };

    for (int i = 0; i < 4; i++)
    {
        SexyVector3 vec(px[i], py[i], 1);
        vec = theMatrix * vec;
        verts[i].position.x = vec.x + x - 0.5f;
        verts[i].position.y = vec.y + y - 0.5f;
        verts[i].tex_coord.x = u[i];
        verts[i].tex_coord.y = v[i];
        verts[i].color = rgba;
    }

    int indices[6] = { 0, 1, 2, 1, 3, 2 };

    SetupBlendMode(theImage, aTex, theDrawMode, theColor);
    SDL_RenderGeometry(mRenderer, aTex, verts, 4, indices, 6);

    SDL_SetRenderClipRect(mRenderer, nullptr);
}

void SDL3TextureManager::FillRect(DDImage* theDestImage, const Rect& theRect, const Color& theColor, int theDrawMode)
{
    VerifyTarget(theDestImage);

    SDL_FRect dstR = { (float)theRect.mX, (float)theRect.mY, (float)theRect.mWidth, (float)theRect.mHeight };
    
    if (theDrawMode == Graphics::DRAWMODE_ADDITIVE)
        SDL_SetRenderDrawBlendMode(mRenderer, SDL_BLENDMODE_ADD);
    else
        SDL_SetRenderDrawBlendMode(mRenderer, SDL_BLENDMODE_BLEND);

    SDL_SetRenderDrawColor(mRenderer, theColor.mRed, theColor.mGreen, theColor.mBlue, theColor.mAlpha);
    SDL_RenderFillRect(mRenderer, &dstR);
    SDL_SetRenderDrawBlendMode(mRenderer, SDL_BLENDMODE_BLEND);
}

void SDL3TextureManager::BltMirror(DDImage* theDestImage, Image* theImage, int theX, int theY, const Rect& theSrcRect, const Color& theColor, int theDrawMode)
{
    VerifyTarget(theDestImage);
    SDL_Texture* aTex = GetTextureForImage(theImage);
    if (!aTex) return;

    SDL_FRect srcR = { (float)theSrcRect.mX, (float)theSrcRect.mY, (float)theSrcRect.mWidth, (float)theSrcRect.mHeight };
    SDL_FRect dstR = { (float)theX, (float)theY, (float)theSrcRect.mWidth, (float)theSrcRect.mHeight };

    SetupBlendMode(theImage, aTex, theDrawMode, theColor);
    SDL_RenderTextureRotated(mRenderer, aTex, &srcR, &dstR, 0, nullptr, SDL_FLIP_HORIZONTAL);
}

void SDL3TextureManager::StretchBltMirror(DDImage* theDestImage, Image* theImage, const Rect& theDestRect, const Rect& theSrcRect, const Rect& theClipRect, const Color& theColor, int theDrawMode, bool fastStretch)
{
    VerifyTarget(theDestImage);
    SDL_Texture* aTex = GetTextureForImage(theImage);
    if (!aTex) return;

    SDL_FRect srcR = { (float)theSrcRect.mX, (float)theSrcRect.mY, (float)theSrcRect.mWidth, (float)theSrcRect.mHeight };
    SDL_FRect dstR = { (float)theDestRect.mX, (float)theDestRect.mY, (float)theDestRect.mWidth, (float)theDestRect.mHeight };

    SDL_Rect clipRect = { theClipRect.mX, theClipRect.mY, theClipRect.mWidth, theClipRect.mHeight };
    SDL_SetRenderClipRect(mRenderer, &clipRect);

    if (fastStretch)
        SDL_SetTextureScaleMode(aTex, SDL_SCALEMODE_NEAREST);
    else
        SDL_SetTextureScaleMode(aTex, SDL_SCALEMODE_LINEAR);

    SetupBlendMode(theImage, aTex, theDrawMode, theColor);
    SDL_RenderTextureRotated(mRenderer, aTex, &srcR, &dstR, 0, nullptr, SDL_FLIP_HORIZONTAL);

    SDL_SetRenderClipRect(mRenderer, nullptr);
}

void SDL3TextureManager::DrawLine(DDImage* theDestImage, double theStartX, double theStartY, double theEndX, double theEndY, const Color& theColor, int theDrawMode)
{
    VerifyTarget(theDestImage);

    if (theDrawMode == Graphics::DRAWMODE_ADDITIVE)
        SDL_SetRenderDrawBlendMode(mRenderer, SDL_BLENDMODE_ADD);
    else
        SDL_SetRenderDrawBlendMode(mRenderer, SDL_BLENDMODE_BLEND);

    SDL_SetRenderDrawColor(mRenderer, theColor.mRed, theColor.mGreen, theColor.mBlue, theColor.mAlpha);
    SDL_RenderLine(mRenderer, (float)theStartX, (float)theStartY, (float)theEndX, (float)theEndY);
    SDL_SetRenderDrawBlendMode(mRenderer, SDL_BLENDMODE_BLEND);
}

void SDL3TextureManager::FlushFrame(ulong* theBits, int theWidth, int theHeight)
{
    if (!mRenderer || !theBits || theWidth <= 0 || theHeight <= 0) return;

    if (!mStreamTex) {
        mStreamTex = SDL_CreateTexture(mRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, theWidth, theHeight);
        SDL_SetTextureBlendMode(mStreamTex, SDL_BLENDMODE_BLEND);
    }
    
    SDL_UpdateTexture(mStreamTex, nullptr, theBits, theWidth * sizeof(ulong));

    SDL_SetRenderTarget(mRenderer, nullptr);
    SDL_SetRenderDrawColor(mRenderer, 0, 0, 0, 255);
    SDL_RenderClear(mRenderer);
    SDL_RenderTexture(mRenderer, mStreamTex, nullptr, nullptr);
    SDL_RenderPresent(mRenderer);
}

void SDL3TextureManager::BltTrianglesTex(DDImage* theDestImage, Image* theTexture, const TriVertex theVertices[][3], int theNumTriangles, const Rect& theClipRect, const Color& theColor, int theDrawMode, float tx, float ty, bool blend)
{
    VerifyTarget(theDestImage);
    SDL_Texture* aTex = GetTextureForImage(theTexture);
    if (!aTex || theNumTriangles <= 0) return;

    SDL_Rect clipRect = { theClipRect.mX, theClipRect.mY, theClipRect.mWidth, theClipRect.mHeight };
    SDL_SetRenderClipRect(mRenderer, &clipRect);

    int totalVerts = theNumTriangles * 3;
    std::vector<SDL_Vertex> sdlVerts(totalVerts);
    std::vector<int> indices(totalVerts);

    float rMod = theColor.mRed / 255.0f;
    float gMod = theColor.mGreen / 255.0f;
    float bMod = theColor.mBlue / 255.0f;
    float aMod = theColor.mAlpha / 255.0f;

    for (int i = 0; i < theNumTriangles; i++)
    {
        for (int v = 0; v < 3; v++)
        {
            const TriVertex& vert = theVertices[i][v];
            int idx = i * 3 + v;
            sdlVerts[idx].position.x = vert.x + tx;
            sdlVerts[idx].position.y = vert.y + ty;
            sdlVerts[idx].tex_coord.x = vert.u;
            sdlVerts[idx].tex_coord.y = vert.v;

            if (vert.color == 0xFFFFFFFF || vert.color == 0)
            {
                sdlVerts[idx].color.r = rMod;
                sdlVerts[idx].color.g = gMod;
                sdlVerts[idx].color.b = bMod;
                sdlVerts[idx].color.a = aMod;
            }
            else
            {
                sdlVerts[idx].color.a = (((vert.color >> 24) & 0xFF) / 255.0f) * aMod;
                sdlVerts[idx].color.r = (((vert.color >> 16) & 0xFF) / 255.0f) * rMod;
                sdlVerts[idx].color.g = (((vert.color >> 8) & 0xFF) / 255.0f) * gMod;
                sdlVerts[idx].color.b = ((vert.color & 0xFF) / 255.0f) * bMod;
            }
            indices[idx] = idx;
        }
    }

    SetupBlendMode(theTexture, aTex, theDrawMode, theColor);
    SDL_RenderGeometry(mRenderer, aTex, sdlVerts.data(), totalVerts, indices.data(), totalVerts);
    SDL_SetRenderClipRect(mRenderer, nullptr);
}
