#include "DDInterface.h"
#include "DDImage.h"
#include "D3DInterface.h"
#include "D3DTester.h"
#include "SexyAppBase.h"
#include "AutoCrit.h"
#include "Graphics.h"
#include "PerfTimer.h"
#include "Debug.h"
#include "MemoryImage.h"
#define SDL_DISABLE_OLD_NAMES
#include <SDL3/SDL.h>
#include "SDL3TextureManager.h"
#include "../LawnApp.h"

using namespace Sexy;

DDInterface::DDInterface(SexyAppBase* theApp)
{
	mApp = theApp;
	mScreenImage = NULL;
	mPendingScreenImage = NULL;
	mHasPendingScreenSwap = false;
	mRedAddTable = NULL;
	mGreenAddTable = NULL;
	mBlueAddTable = NULL;
	mInitialized = false;
	mVideoOnlyDraw = false;	
	mScanLineFailCount = 0;
	mAspectCorrect = true;
	mAspectNoStretch = false;

	mNextCursorX = 0;
	mNextCursorY = 0;
	mCursorX = 0;
	mCursorY = 0;
	mInRedraw = false;
	mCursorWidth = 64;
	mCursorHeight = 64;
	mCursorImage = NULL;
	mHasOldCursorArea = false;
	mNewCursorAreaImage = NULL;
	mOldCursorAreaImage = NULL;
	mInitCount = 0;
	mRefreshRate = 60;
	mMillisecondsPerFrame = 1000/mRefreshRate;

	mD3DInterface = new D3DInterface;
	mIs3D = false;

	mD3DTester = NULL;
	mScreenTex = NULL;
	mLastTexW = 0;
	mLastTexH = 0;
}

DDInterface::~DDInterface()
{
	delete [] mRedAddTable;
	delete [] mGreenAddTable;
	delete [] mBlueAddTable;

	Cleanup();

	delete mD3DInterface;
	delete mD3DTester;
}

std::string DDInterface::ResultToString(int theResult)
{
	switch (theResult)
	{
	case RESULT_OK:
		return "RESULT_OK";
	case RESULT_FAIL:
		return "RESULT_FAIL";
	case RESULT_3D_FAIL:
		return "RESULT_3D_FAIL";
	default:
		return "RESULT_UNKNOWN";
	}
}

int DDInterface::Init(HWND theWindow, bool IsWindowed)
{
	AutoCrit anAutoCrit(mCritSect);

	mInitialized = false;
	Cleanup();

	mHWnd = theWindow;
	mWidth = mApp->mWidth;
	mHeight = mApp->mHeight;
	mAspect.Set(mWidth, mHeight);
	mDesktopWidth = GetSystemMetrics(SM_CXSCREEN);
	mDesktopHeight = GetSystemMetrics(SM_CYSCREEN);
	mDesktopAspect.Set(mDesktopWidth, mDesktopHeight);
	mDisplayWidth = mWidth;
	mDisplayHeight = mHeight;
	mDisplayAspect = mAspect;
	mPresentationRect = Rect(0, 0, mWidth, mHeight);
	mApp->mScreenBounds = mPresentationRect;
	mFullscreenBits = mApp->mFullscreenBits;
	mIsWindowed = IsWindowed;
	mHasOldCursorArea = false;
	mAspectCorrect = mApp->mAspectCorrect;
	mAspectNoStretch = mApp->mAspectNoStretch;

	mRefreshRate = 60;
	mMillisecondsPerFrame = 1000 / mRefreshRate;

	mOldCursorAreaImage = new DDImage(this);
	mOldCursorAreaImage->mWidth = mCursorWidth;
	mOldCursorAreaImage->mHeight = mCursorHeight;
	static_cast<MemoryImage*>(mOldCursorAreaImage)->SetImageMode(false, false);

	mNewCursorAreaImage = new DDImage(this);
	mNewCursorAreaImage->mWidth = mCursorWidth;
	mNewCursorAreaImage->mHeight = mCursorHeight;
	static_cast<MemoryImage*>(mNewCursorAreaImage)->SetImageMode(false, false);

	// Setup software pixel format (ARGB8888)
	mRGBBits = 32;
	mRedMask = 0x00FF0000;
	mGreenMask = 0x0000FF00;
	mBlueMask = 0x000000FF;
	mRedShift = 16;
	mGreenShift = 8;
	mBlueShift = 0;
	mRedBits = 8;
	mGreenBits = 8;
	mBlueBits = 8;

	delete[] mRedAddTable;
	delete[] mGreenAddTable;
	delete[] mBlueAddTable;

	mRedAddTable = new int[255 * 2 + 1];
	mGreenAddTable = new int[255 * 2 + 1];
	mBlueAddTable = new int[255 * 2 + 1];

	for (int i = 0; i < 255 * 2 + 1; i++)
	{
		mRedAddTable[i] = min(i, 255);
		mGreenAddTable[i] = min(i, 255);
		mBlueAddTable[i] = min(i, 255);
	}

	for (int i = 0; i < 256; i++)
	{
		mRedConvTable[i] = ((i * mRedMask) / 255) & mRedMask;
		mGreenConvTable[i] = ((i * mGreenMask) / 255) & mGreenMask;
		mBlueConvTable[i] = ((i * mBlueMask) / 255) & mBlueMask;
	}

	SetVideoOnlyDraw(mVideoOnlyDraw);

	mInitCount++;
	mInitialized = true;

	return RESULT_OK;
}

void DDInterface::Cleanup()
{
	AutoCrit anAutoCrit(mCritSect);

	mInitialized = false;	
	mD3DInterface->Cleanup();

	if (mOldCursorAreaImage != NULL)
	{
		delete mOldCursorAreaImage;
		mOldCursorAreaImage = NULL;
	}

	if (mNewCursorAreaImage != NULL)
	{
		delete mNewCursorAreaImage;
		mNewCursorAreaImage = NULL;
	}

	if (mScreenImage != NULL)	
	{
		delete mScreenImage;
		mScreenImage = NULL;
	}

	if (mPendingScreenImage != NULL)
	{
		delete mPendingScreenImage;
		mPendingScreenImage = NULL;
	}

	if (mScreenTex != NULL)
	{
		SDL_DestroyTexture(mScreenTex);
		mScreenTex = NULL;
	}

	mLastTexW = 0;
	mLastTexH = 0;
}

DDImage* DDInterface::GetScreenImage()
{
	if (mScreenImage == NULL)
		SetVideoOnlyDraw(mVideoOnlyDraw);

	return mScreenImage;
}

void DDInterface::SetVideoOnlyDraw(bool videoOnlyDraw)
{
	AutoCrit anAutoCrit(mCritSect);

	mVideoOnlyDraw = videoOnlyDraw;

	DDImage* aNewImage = new DDImage(this);
	aNewImage->Create(mWidth, mHeight);
	aNewImage->mNoLock = mVideoOnlyDraw;
	aNewImage->mVideoMemory = mVideoOnlyDraw;
	aNewImage->SetImageMode(false, false);
	aNewImage->mIsScreenBuffer = true;

	if (mHasPendingScreenSwap && mPendingScreenImage != NULL)
		delete mPendingScreenImage;

	mPendingScreenImage = aNewImage;
	mHasPendingScreenSwap = true;

	// If mScreenImage is NULL, we swap immediately
	if (mScreenImage == NULL)
		SyncScreenImage();
}

void DDInterface::SyncScreenImage()
{
	AutoCrit anAutoCrit(mCritSect);

	if (mHasPendingScreenSwap && mPendingScreenImage != NULL)
	{
		delete mScreenImage;
		mScreenImage = mPendingScreenImage;

		mPendingScreenImage = NULL;
		mHasPendingScreenSwap = false;
	}
}

void DDInterface::RemapMouse(int& theX, int& theY)
{
	if (mInitialized)
	{
		if (mPresentationRect.mWidth > 0 && mPresentationRect.mHeight > 0)
		{
			theX = (theX - mPresentationRect.mX) * mWidth / mPresentationRect.mWidth;
			theY = (theY - mPresentationRect.mY) * mHeight / mPresentationRect.mHeight;
		}
	}
}

bool DDInterface::SetCursorImage(Image* theImage)
{
	AutoCrit anAutoCrit(mCritSect);

	if (mCursorImage != theImage)
	{
		mCursorImage = theImage;
		return true;
	}
	return false;
}

void DDInterface::SetCursorPos(int theCursorX, int theCursorY)
{
	mNextCursorX = theCursorX;
	mNextCursorY = theCursorY;
	mCursorX = theCursorX;
	mCursorY = theCursorY;
}

bool DDInterface::Redraw(Rect* theClipRect)
{
	// Redraw is now handled in SexyAppBase::Redraw using modern SDL3 texture updates
	return true; 
}

ulong DDInterface::GetColorRef(ulong theRGB)
{
	// ARGB8888 conversion
	BYTE r = (BYTE)((theRGB >> 16) & 0xFF);
	BYTE g = (BYTE)((theRGB >> 8) & 0xFF);
	BYTE b = (BYTE)(theRGB & 0xFF);
	return (0xFF << 24) | (r << 16) | (g << 8) | b;
}

bool DDInterface::Do3DTest(HWND theHWND)
{
	return true; // Assume software rendering is always capable
}

void DDInterface::AddDDImage(DDImage* theImage)
{
}

void DDInterface::RemoveDDImage(DDImage* theImage)
{
}

void DDInterface::Remove3DData(MemoryImage* theImage)
{
	SDL3TextureManager::Instance().Remove(theImage);
}
