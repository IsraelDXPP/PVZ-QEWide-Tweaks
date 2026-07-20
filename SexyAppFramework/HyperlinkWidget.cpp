#include "HyperlinkWidget.h"
#include "Graphics.h"
#include "ImageFont.h"
#include "SysFont.h"
#include "WidgetManager.h"

#include <cmath>

using namespace Sexy;

HyperlinkWidget::HyperlinkWidget(int theId, ButtonListener* theButtonListener) :
	ButtonWidget(theId, theButtonListener),
	mColor(255, 255, 255),
	mOverColor(255, 255, 255)
{	
	mDoFinger = true;
	mUnderlineOffset = 3;
	mUnderlineSize = 1;
#ifdef SEXY_CONSOLE
	mEnableGlow = false;
	mGlowAngle = 0.0f;
#endif
}

void HyperlinkWidget::Draw(Graphics* g)
{
	if (mFont == NULL)
		mFont = new SysFont(mWidgetManager->mApp, "Arial Unicode MS", 10); //baz changed

	int aFontX = (mWidth - mFont->StringWidth(mLabel))/2;
	int aFontY = (mHeight + mFont->GetAscent())/2 - 1;

	if (mIsOver)
		g->SetColor(mOverColor);
	else
		g->SetColor(mColor);

	g->SetFont(mFont);	
	g->DrawString(mLabel, aFontX, aFontY);

#ifdef SEXY_CONSOLE
	if (mEnableGlow)
	{
		float aAlpha = std::sin(mGlowAngle) * 255.0f;
		if (aAlpha < 0.0f)
			aAlpha = 0.0f;
		g->SetColorizeImages(true);
		g->SetColor(Color(255, 255, 255, (int)aAlpha));
		g->DrawString(mLabel, aFontX, aFontY);
		g->SetColorizeImages(false);
	}
#endif

	for (int i = 0; i < mUnderlineSize; i++)
		g->FillRect(aFontX, aFontY+mUnderlineOffset+i, mFont->StringWidth(mLabel), 1);
}

void HyperlinkWidget::MouseEnter()
{
	ButtonWidget::MouseEnter();

	MarkDirtyFull();
}

void HyperlinkWidget::MouseLeave()
{
	ButtonWidget::MouseLeave();

	MarkDirtyFull();
}