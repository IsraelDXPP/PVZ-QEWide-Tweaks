#ifndef __HYPERLINKWIDGET_H__
#define __HYPERLINKWIDGET_H__

#include "ButtonWidget.h"
#include "../../ConstEnums.h"

namespace Sexy
{

class HyperlinkWidget : public ButtonWidget
{
public:
	Color					mColor;
	Color					mOverColor;
	int						mUnderlineSize;
	int						mUnderlineOffset;
#ifdef SEXY_CONSOLE
	bool					mEnableGlow;
	float					mGlowAngle;
#endif

public:
	HyperlinkWidget(int theId, ButtonListener* theButtonListener);
	virtual ~HyperlinkWidget(){};

	void					Draw(Graphics* g);
	void					MouseEnter();
	void					MouseLeave();	
};

}

#endif //__HYPERLINKWIDGET_H__