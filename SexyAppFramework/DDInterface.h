#ifndef __DDINTERFACE_H__
#define __DDINTERFACE_H__

#include "Common.h"
#include "CritSect.h"
#include "NativeDisplay.h"
#include "Rect.h"
#include "Ratio.h"

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

namespace Sexy
{

class SexyAppBase;
class DDImage;
class Image;
class MemoryImage;
class D3DInterface;
class D3DTester;

typedef std::set<DDImage*> DDImageSet;

class DDInterface : public NativeDisplay
{
public:
	enum
	{
		RESULT_OK					= 0,
		RESULT_FAIL					= 1,
		RESULT_DD_CREATE_FAIL		= 2,
		RESULT_SURFACE_FAIL			= 3,
		RESULT_EXCLUSIVE_FAIL		= 4,
		RESULT_DISPCHANGE_FAIL		= 5,
		RESULT_INVALID_COLORDEPTH	= 6,
		RESULT_3D_FAIL				= 7
	};

	SexyAppBase*			mApp;
	D3DInterface*			mD3DInterface;
	D3DTester*				mD3DTester;
	bool					mIs3D;

	CritSect				mCritSect;
	bool					mInRedraw;
	int						mWidth;
	int						mHeight;
	Ratio					mAspect;
	int						mDesktopWidth;
	int						mDesktopHeight;
	Ratio					mDesktopAspect;
	bool					mIsWidescreen;
	int						mDisplayWidth;
	int						mDisplayHeight;
	Ratio					mDisplayAspect;
	bool					mAspectCorrect;
	bool					mAspectNoStretch;

	SDL_Texture*			mScreenTex;
	int						mLastTexW;
	int						mLastTexH;

	Rect					mPresentationRect;
	int						mFullscreenBits;
	DWORD					mRefreshRate;
	DWORD					mMillisecondsPerFrame;
	int						mScanLineFailCount;

	int*					mRedAddTable;
	int*					mGreenAddTable;
	int*					mBlueAddTable;

	ulong					mRedConvTable[256];
	ulong					mGreenConvTable[256];
	ulong					mBlueConvTable[256];

	bool					mInitialized;
	HWND					mHWnd;
	bool					mIsWindowed;
	DDImage*				mScreenImage;
	DDImage*				mPendingScreenImage;
	bool					mHasPendingScreenSwap;
	DDImageSet				mDDImageSet;
	bool					mVideoOnlyDraw;
	ulong					mInitCount;

	int						mCursorWidth;
	int						mCursorHeight;
	int						mNextCursorX;
	int						mNextCursorY;
	int						mCursorX;
	int						mCursorY;
	Image*					mCursorImage;
	bool					mHasOldCursorArea;	
	Image*					mOldCursorAreaImage;
	Image*					mNewCursorAreaImage;	

	std::string				mErrorString;

public:
	ulong					GetColorRef(ulong theRGB);
	void					AddDDImage(DDImage* theDDImage);
	void					RemoveDDImage(DDImage* theDDImage);
	void					Remove3DData(MemoryImage* theImage); // for 3d texture cleanup

	void					Cleanup();
	bool					Do3DTest(HWND theHWND);

public:
	DDInterface(SexyAppBase* theApp);
	virtual ~DDInterface();

	static std::string		ResultToString(int theResult);

	DDImage*				GetScreenImage();
	int						Init(HWND theWindow, bool IsWindowed);	
	void					SyncScreenImage();	
	bool					Redraw(Rect* theClipRect = NULL);	
	void					SetVideoOnlyDraw(bool videoOnly);
	void					RemapMouse(int& theX, int& theY);

	bool					SetCursorImage(Image* theImage);
	void					SetCursorPos(int theCursorX, int theCursorY);
};

}

#endif //__DDINTERFACE_H__

