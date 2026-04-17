#pragma once
#include <SDL3/SDL.h>
#include <unordered_map>
#include "Common.h"
#include "DDImage.h"

namespace Sexy
{

class DDImage;

// ============================================================
//  SDL3TextureManager — Native Hardware Acceleration 3D API
//
//  Architecture:
//    Operates identically to the legacy Direct3D/DirectDraw interface.
//    Manages GPU memory operations explicitly for mVideoMemory images.
//    Supports additive blending, clipping, and matrix geometries.
// ============================================================

class SDL3TextureManager
{
public:
    SDL_Renderer* mRenderer  = nullptr;
    SDL_Texture*  mStreamTex = nullptr; // Fallback streaming texture for software mode
    DDImage*      mRenderTarget = nullptr; // Track current render target explicitly

    static SDL3TextureManager& Instance()
    {
        static SDL3TextureManager sInstance;
        return sInstance;
    }

    std::unordered_map<Image*, SDL_Texture*> mTextureMap;
    std::unordered_map<Image*, int> mSyncCount;

    // Initialize with Window Renderer
    void SetRenderer(SDL_Renderer* theRenderer);

    // Generic Surface creation and teardown management
    SDL_Texture* GetTextureForImage(Image* theImage);
    bool GenerateTextureForImage(Image* theImage);
    void DeleteTextureForImage(Image* theImage);
    void UpdateTextureForImage(Image* theImage);
    void Remove(class Image* theImage);

    // Hardware Render target routines
    void SetRenderTarget(class DDImage* theTargetImage);
    void VerifyTarget(class DDImage* destImage);

    // -------------------------------------------------------------
    // HARDWARE ACCELERATED DRAWING OPERATIONS
    // -------------------------------------------------------------
    void Blt(DDImage* theDestImage, Image* theImage, int theX, int theY, const Rect& theSrcRect, const Color& theColor, int theDrawMode);
    void BltMirror(DDImage* theDestImage, Image* theImage, int theX, int theY, const Rect& theSrcRect, const Color& theColor, int theDrawMode);
    void BltF(DDImage* theDestImage, Image* theImage, float theX, float theY, const Rect& theSrcRect, const Rect &theClipRect, const Color& theColor, int theDrawMode);
    void BltRotated(DDImage* theDestImage, Image* theImage, float theX, float theY, const Rect &theSrcRect, const Rect& theClipRect, const Color& theColor, int theDrawMode, double theRot, float theRotCenterX, float theRotCenterY);
    void StretchBlt(DDImage* theDestImage, Image* theImage, const Rect& theDestRect, const Rect& theSrcRect, const Rect& theClipRect, const Color& theColor, int theDrawMode, bool fastStretch);
    void StretchBltMirror(DDImage* theDestImage, Image* theImage, const Rect& theDestRect, const Rect& theSrcRect, const Rect& theClipRect, const Color& theColor, int theDrawMode, bool fastStretch);
    void BltMatrix(DDImage* theDestImage, Image* theImage, float x, float y, const SexyMatrix3 &theMatrix, const Rect& theClipRect, const Color& theColor, int theDrawMode, const Rect &theSrcRect, bool blend);
    void BltTrianglesTex(DDImage* theDestImage, Image* theTexture, const TriVertex theVertices[][3], int theNumTriangles, const Rect& theClipRect, const Color& theColor, int theDrawMode, float tx, float ty, bool blend);
    void FillRect(DDImage* theDestImage, const Rect& theRect, const Color& theColor, int theDrawMode);
    void DrawLine(DDImage* theDestImage, double theStartX, double theStartY, double theEndX, double theEndY, const Color& theColor, int theDrawMode);

    // Global GPU state
    void Cleanup();
    void EndFrame();
    void FlushFrame(ulong* theBits, int theWidth, int theHeight); // Software mode fallback
};

} // namespace Sexy
