#include "D3DInterface.h"
#include "DDInterface.h"
#include "Graphics.h"
#include "SexyAppBase.h"
#include <assert.h>

using namespace Sexy;

std::string D3DInterface::mErrorString;
bool gD3DInterfacePreDrawError = false;

// Dummy GUIDs for linker satisfaction without legacy libs
#include <initguid.h>
DEFINE_GUID(IID_IDirect3D7, 0xf5049e77, 0x4861, 0x11d2, 0xa4, 0x7, 0x0, 0xa0, 0xc9, 0x6, 0x29, 0xa8);
DEFINE_GUID(IID_IDirect3DHALDevice, 0x84e63d80, 0x46aa, 0x11cf, 0x81, 0x6f, 0x0, 0x0, 0xc0, 0x20, 0x15, 0x6e);
DEFINE_GUID(IID_IDirectDraw7, 0x15e74261, 0x1e5, 0x11d2, 0x89, 0x68, 0x0, 0xa0, 0xc9, 0x6, 0x29, 0xa8);

D3DInterface::D3DInterface()
{
	mHWnd = NULL;
	mWidth = 640;
	mHeight = 480;
	mSceneBegun = false;
	mIsWindowed = true;
}

D3DInterface::~D3DInterface()
{
}

void D3DInterface::UpdateViewport()
{
}

bool D3DInterface::InitD3D()
{
	return false;
}

bool D3DInterface::InitFromDDInterface(DDInterface *theInterface)
{
	mErrorString = "D3D is disabled in SDL3 build";
	return false;
}

void D3DInterface::Cleanup()
{
}

void D3DInterface::PushTransform(const SexyMatrix3 &theTransform, bool concatenate)
{
}

void D3DInterface::PopTransform()
{
}

bool D3DInterface::PreDraw()
{
	return false;
}

void D3DInterface::Flush()
{
}

void D3DInterface::RemoveMemoryImage(MemoryImage *theImage)
{
}

bool D3DInterface::CreateImageTexture(MemoryImage *theImage)
{
	return false;
}

bool D3DInterface::RecoverBits(MemoryImage* theImage)
{
	return false;
}

void D3DInterface::SetCurTexture(MemoryImage *theImage)
{
}

void D3DInterface::Blt(Image* theImage, float theX, float theY, const Rect& theSrcRect, const Color& theColor, int theDrawMode, bool linearFilter)
{
}

void D3DInterface::BltClipF(Image* theImage, float theX, float theY, const Rect& theSrcRect, const Rect *theClipRect, const Color& theColor, int theDrawMode)
{
}

void D3DInterface::BltMirror(Image* theImage, float theX, float theY, const Rect& theSrcRect, const Color& theColor, int theDrawMode, bool linearFilter)
{
}

void D3DInterface::StretchBlt(Image* theImage, const Rect& theDestRect, const Rect& theSrcRect, const Rect* theClipRect, const Color &theColor, int theDrawMode, bool fastStretch, bool mirror)
{
}

void D3DInterface::BltRotated(Image* theImage, float theX, float theY, const Rect* theClipRect, const Color& theColor, int theDrawMode, double theRot, float theRotCenterX, float theRotCenterY, const Rect& theSrcRect)
{
}

void D3DInterface::BltTransformed(Image* theImage, const Rect* theClipRect, const Color& theColor, int theDrawMode, const Rect &theSrcRect, const SexyMatrix3 &theTransform, bool linearFilter, float theX, float theY, bool center)
{
}

void D3DInterface::DrawLine(double theStartX, double theStartY, double theEndX, double theEndY, const Color& theColor, int theDrawMode)
{
}

void D3DInterface::FillRect(const Rect& theRect, const Color& theColor, int theDrawMode)
{
}

void D3DInterface::DrawTriangle(const TriVertex &p1, const TriVertex &p2, const TriVertex &p3, const Color &theColor, int theDrawMode)
{
}

void D3DInterface::DrawTriangleTex(const TriVertex &p1, const TriVertex &p2, const TriVertex &p3, const Color &theColor, int theDrawMode, Image *theTexture, bool blend)
{
}

void D3DInterface::DrawTrianglesTex(const TriVertex theVertices[][3], int theNumTriangles, const Color &theColor, int theDrawMode, Image *theTexture, float tx, float ty, bool blend)
{
}

void D3DInterface::DrawTrianglesTexStrip(const TriVertex theVertices[], int theNumTriangles, const Color &theColor, int theDrawMode, Image *theTexture, float tx, float ty, bool blend)
{
}

void D3DInterface::FillPoly(const Point theVertices[], int theNumVertices, const Rect *theClipRect, const Color &theColor, int theDrawMode, int tx, int ty)
{
}

void D3DInterface::SetupDrawMode(int theDrawMode, const Color &theColor, Image *theImage)
{
}

HRESULT CALLBACK D3DInterface::PixelFormatsCallback(LPDDPIXELFORMAT theFormat, LPVOID lpContext)
{
	return 0;
}

void D3DInterface::MakeDDPixelFormat(PixelFormat theFormatType, DDPIXELFORMAT* theFormat)
{
}

PixelFormat D3DInterface::GetDDPixelFormat(LPDDPIXELFORMAT theFormat)
{
	return PixelFormat_Unknown;
}

bool D3DInterface::CheckDXError(HRESULT theError, const char *theMsg)
{
	return false;
}

TextureData::TextureData()
{
}

TextureData::~TextureData()
{
}

void TextureData::ReleaseTextures()
{
}

void TextureData::CreateTextureDimensions(MemoryImage *theImage)
{
}

void TextureData::CreateTextures(MemoryImage *theImage, LPDIRECT3DDEVICE7 theDevice, LPDIRECTDRAW7 theDraw)
{
}

void TextureData::CheckCreateTextures(MemoryImage *theImage, LPDIRECT3DDEVICE7 theDevice, LPDIRECTDRAW7 theDraw)
{
}

LPDIRECTDRAWSURFACE7 TextureData::GetTexture(int x, int y, int &width, int &height, float &u1, float &v1, float &u2, float &v2)
{
	return NULL;
}

LPDIRECTDRAWSURFACE7 TextureData::GetTextureF(float x, float y, float &width, float &height, float &u1, float &v1, float &u2, float &v2)
{
	return NULL;
}

void TextureData::Blt(LPDIRECT3DDEVICE7 theDevice, float theX, float theY, const Rect& theSrcRect, const Color& theColor)
{
}

void TextureData::BltTransformed(LPDIRECT3DDEVICE7 theDevice, const SexyMatrix3 &theTrans, const Rect& theSrcRect, const Color& theColor, const Rect *theClipRect, float theX, float theY, bool center)
{
}

void TextureData::BltTriangles(LPDIRECT3DDEVICE7 theDevice, const TriVertex theVertices[][3], int theNumTriangles, DWORD theColor, float tx, float ty)
{
}
