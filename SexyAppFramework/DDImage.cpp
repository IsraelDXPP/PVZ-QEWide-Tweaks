#include "DDImage.h"
#include "DDInterface.h"
#include "SexyAppBase.h"
#include "Debug.h"
#include "SexyMatrix.h"
#include "Graphics.h"
#include "SDL3TextureManager.h"

using namespace Sexy;

DDImage::DDImage()
{
	mDDInterface = NULL;
	Init();
}

DDImage::DDImage(DDInterface* theDDInterface)
{
	mDDInterface = theDDInterface;
	Init();
}

DDImage::~DDImage()
{
	mDDInterface->RemoveDDImage(this);
	DBG_ASSERTE(mLockCount == 0);
}

void DDImage::Init()
{
	mBufferId = 0;
	if (mDDInterface)
		mDDInterface->AddDDImage(this);

	mNoLock = false;
	mVideoMemory = false;
	mFirstPixelTrans = false;
	mWantDDSurface = false;			
	mDrawToBits = true;
	mSurfaceSet = false;

	mLockCount = 0;
}

bool DDImage::Check3D(Image *theImage)
{
	return false;
}

bool DDImage::Check3D(DDImage *theImage)
{
	return false;
}

bool DDImage::LockSurface()
{
	mLockCount++;
	return true;
}

bool DDImage::UnlockSurface()
{
	if (mLockCount > 0)
		mLockCount--;

	return true;
}

void DDImage::DeleteSWBuffers()
{
	if (mBits != NULL)
	{
		delete [] mBits;
		mBits = NULL;
	}

	MemoryImage::DeleteSWBuffers();
}

void DDImage::PurgeBits()
{
	MemoryImage::PurgeBits();
}

void DDImage::ReInit()
{
	MemoryImage::ReInit();
	if (mWantDDSurface)
		GenerateDDSurface();
}

bool DDImage::GenerateDDSurface()
{
	mWantDDSurface = true;
	return SDL3TextureManager::Instance().GenerateTextureForImage(this);
}

void DDImage::DeleteDDSurface() 
{
	SDL3TextureManager::Instance().DeleteTextureForImage(this);
}

void DDImage::DeleteNativeData()
{
	MemoryImage::DeleteNativeData();
	DeleteDDSurface();
}

void DDImage::DeleteExtraBuffers()
{
	MemoryImage::DeleteExtraBuffers();
	DeleteDDSurface();
}

void DDImage::SetVideoMemory(bool wantVideoMemory)
{
	mVideoMemory = wantVideoMemory;
}

void DDImage::RehupFirstPixelTrans()
{
}

void DDImage::BitsChanged()
{
	MemoryImage::BitsChanged();
}

void DDImage::CommitBits()
{
	MemoryImage::CommitBits();
}

bool DDImage::PolyFill3D(const Point theVertices[], int theNumVertices, const Rect *theClipRect, const Color &theColor, int theDrawMode, int tx, int ty, bool convex)
{
	return false;
}

void DDImage::Create(int theWidth, int theHeight)
{
	MemoryImage::Create(theWidth, theHeight);
}

ulong* DDImage::GetBits()
{
	return MemoryImage::GetBits();
}

bool DDImage::Palletize()
{
	return MemoryImage::Palletize();
}

// ============================================================
// Render Redirects
// ============================================================

void DDImage::NormalFillRect(const Rect& theRect, const Color& theColor)
{
	FillRect(theRect, theColor, Graphics::DRAWMODE_NORMAL);
}

void DDImage::AdditiveFillRect(const Rect& theRect, const Color& theColor)
{
	FillRect(theRect, theColor, Graphics::DRAWMODE_ADDITIVE);
}

void DDImage::NormalBlt(Image* theImage, int theX, int theY, const Rect& theSrcRect, const Color& theColor)
{
	MemoryImage::NormalBlt(theImage, theX, theY, theSrcRect, theColor);
}

void DDImage::AdditiveBlt(Image* theImage, int theX, int theY, const Rect& theSrcRect, const Color& theColor)
{
	MemoryImage::AdditiveBlt(theImage, theX, theY, theSrcRect, theColor);
}

void DDImage::NormalDrawLine(double theStartX, double theStartY, double theEndX, double theEndY, const Color& theColor)
{
	MemoryImage::NormalDrawLine(theStartX, theStartY, theEndX, theEndY, theColor);
}

void DDImage::AdditiveDrawLine(double theStartX, double theStartY, double theEndX, double theEndY, const Color& theColor)
{
	MemoryImage::AdditiveDrawLine(theStartX, theStartY, theEndX, theEndY, theColor);
}

void DDImage::NormalDrawLineAA(double theStartX, double theStartY, double theEndX, double theEndY, const Color& theColor)
{
	MemoryImage::NormalDrawLineAA(theStartX, theStartY, theEndX, theEndY, theColor);
}

void DDImage::AdditiveDrawLineAA(double theStartX, double theStartY, double theEndX, double theEndY, const Color& theColor)
{
	MemoryImage::AdditiveDrawLineAA(theStartX, theStartY, theEndX, theEndY, theColor);
}

void DDImage::NormalBltMirror(Image* theImage, int theX, int theY, const Rect& theSrcRect, const Color& theColor)
{
	BltMirror(theImage, theX, theY, theSrcRect, theColor, Graphics::DRAWMODE_NORMAL);
}

void DDImage::AdditiveBltMirror(Image* theImage, int theX, int theY, const Rect& theSrcRect, const Color& theColor)
{
	BltMirror(theImage, theX, theY, theSrcRect, theColor, Graphics::DRAWMODE_ADDITIVE);
}

void DDImage::FillScanLinesWithCoverage(Span* theSpans, int theSpanCount, const Color& theColor, int theDrawMode, const BYTE* theCoverage, int theCoverX, int theCoverY, int theCoverWidth, int theCoverHeight)
{
	MemoryImage::FillScanLinesWithCoverage(theSpans, theSpanCount, theColor, theDrawMode, theCoverage, theCoverX, theCoverY, theCoverWidth, theCoverHeight);
}

void DDImage::FillRect(const Rect& theRect, const Color& theColor, int theDrawMode)
{
	if (mApp && mApp->Is3DAccelerated() && mWantDDSurface)
	{
		SDL3TextureManager::Instance().FillRect(this, theRect, theColor, theDrawMode);
		return;
	}
	MemoryImage::FillRect(theRect, theColor, theDrawMode);
}

void DDImage::DrawLine(double theStartX, double theStartY, double theEndX, double theEndY, const Color& theColor, int theDrawMode)
{
	if (mApp && mApp->Is3DAccelerated() && mWantDDSurface)
	{
		SDL3TextureManager::Instance().DrawLine(this, theStartX, theStartY, theEndX, theEndY, theColor, theDrawMode);
		return;
	}
	MemoryImage::DrawLine(theStartX, theStartY, theEndX, theEndY, theColor, theDrawMode);
}

void DDImage::DrawLineAA(double theStartX, double theStartY, double theEndX, double theEndY, const Color& theColor, int theDrawMode)
{
	if (mApp && mApp->Is3DAccelerated() && mWantDDSurface)
	{
		SDL3TextureManager::Instance().DrawLine(this, theStartX, theStartY, theEndX, theEndY, theColor, theDrawMode);
		return;
	}
	MemoryImage::DrawLineAA(theStartX, theStartY, theEndX, theEndY, theColor, theDrawMode);
}

void DDImage::Blt(Image* theImage, int theX, int theY, const Rect& theSrcRect, const Color& theColor, int theDrawMode)
{
	if (mApp && mApp->Is3DAccelerated() && mWantDDSurface)
	{
		SDL3TextureManager::Instance().Blt(this, theImage, theX, theY, theSrcRect, theColor, theDrawMode);
		return;
	}
	MemoryImage::Blt(theImage, theX, theY, theSrcRect, theColor, theDrawMode);
}

void DDImage::BltF(Image* theImage, float theX, float theY, const Rect& theSrcRect, const Rect& theClipRect, const Color& theColor, int theDrawMode)
{
	if (mApp && mApp->Is3DAccelerated() && mWantDDSurface)
	{
		SDL3TextureManager::Instance().BltF(this, theImage, theX, theY, theSrcRect, theClipRect, theColor, theDrawMode);
		return;
	}
	MemoryImage::BltF(theImage, theX, theY, theSrcRect, theClipRect, theColor, theDrawMode);
}

void DDImage::BltRotated(Image* theImage, float theX, float theY, const Rect& theSrcRect, const Rect& theClipRect, const Color& theColor, int theDrawMode, double theRot, float theRotCenterX, float theRotCenterY)
{
	if (mApp && mApp->Is3DAccelerated() && mWantDDSurface)
	{
		SDL3TextureManager::Instance().BltRotated(this, theImage, theX, theY, theSrcRect, theClipRect, theColor, theDrawMode, theRot, theRotCenterX, theRotCenterY);
		return;
	}
	MemoryImage::BltRotated(theImage, theX, theY, theSrcRect, theClipRect, theColor, theDrawMode, theRot, theRotCenterX, theRotCenterY);
}

void DDImage::StretchBlt(Image* theImage, const Rect& theDestRect, const Rect& theSrcRect, const Rect& theClipRect, const Color& theColor, int theDrawMode, bool fastStretch)
{
	if (mApp && mApp->Is3DAccelerated() && mWantDDSurface)
	{
		SDL3TextureManager::Instance().StretchBlt(this, theImage, theDestRect, theSrcRect, theClipRect, theColor, theDrawMode, fastStretch);
		return;
	}
	MemoryImage::StretchBlt(theImage, theDestRect, theSrcRect, theClipRect, theColor, theDrawMode, fastStretch);
}

void DDImage::BltMatrix(Image* theImage, float x, float y, const SexyMatrix3& theMatrix, const Rect& theClipRect, const Color& theColor, int theDrawMode, const Rect& theSrcRect, bool blend)
{
	if (mApp && mApp->Is3DAccelerated() && mWantDDSurface)
	{
		SDL3TextureManager::Instance().BltMatrix(this, theImage, x, y, theMatrix, theClipRect, theColor, theDrawMode, theSrcRect, blend);
		return;
	}
	MemoryImage::BltMatrix(theImage, x, y, theMatrix, theClipRect, theColor, theDrawMode, theSrcRect, blend);
}

void DDImage::BltTrianglesTex(Image* theTexture, const TriVertex theVertices[][3], int theNumTriangles, const Rect& theClipRect, const Color& theColor, int theDrawMode, float tx, float ty, bool blend)
{
	if (mApp && mApp->Is3DAccelerated() && mWantDDSurface)
	{
		SDL3TextureManager::Instance().BltTrianglesTex(this, theTexture, theVertices, theNumTriangles, theClipRect, theColor, theDrawMode, tx, ty, blend);
		return;
	}
	MemoryImage::BltTrianglesTex(theTexture, theVertices, theNumTriangles, theClipRect, theColor, theDrawMode, tx, ty, blend);
}

void DDImage::BltMirror(Image* theImage, int theX, int theY, const Rect& theSrcRect, const Color& theColor, int theDrawMode)
{
	if (mApp && mApp->Is3DAccelerated() && mWantDDSurface)
	{
		SDL3TextureManager::Instance().BltMirror(this, theImage, theX, theY, theSrcRect, theColor, theDrawMode);
		return;
	}
	MemoryImage::BltMirror(theImage, theX, theY, theSrcRect, theColor, theDrawMode);
}

void DDImage::StretchBltMirror(Image* theImage, const Rect& theDestRectOrig, const Rect& theSrcRect, const Rect& theClipRect, const Color& theColor, int theDrawMode, bool fastStretch)
{
	if (mApp && mApp->Is3DAccelerated() && mWantDDSurface)
	{
		SDL3TextureManager::Instance().StretchBltMirror(this, theImage, theDestRectOrig, theSrcRect, theClipRect, theColor, theDrawMode, fastStretch);
		return;
	}
	MemoryImage::StretchBltMirror(theImage, theDestRectOrig, theSrcRect, theClipRect, theColor, theDrawMode, fastStretch);
}
