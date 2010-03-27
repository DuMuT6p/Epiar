/**\file			ui_window.h
 * \author			Chris Thielen (chris@luethy.net)
 * \date			Created: Unknown (2006?)
 * \date			Modified: Sunday, November 22, 2009
 * \brief
 * \details
 */

#ifndef __H_WINDOW__
#define __H_WINDOW__

#include "Graphics/image.h"
#include "UI/ui.h"
#include "UI/ui_scrollbar.h"

class Window : public Widget {
	public:
		Window( int x, int y, int w, int h, string caption );
		bool AddChild( Widget *widget );
		Widget *DetermineMouseFocus( int relx, int rely );
		void Draw( int relx = 0, int rely = 0 );
	
		bool MouseDrag( int x, int y );

		string GetType( void ){return string("Window");}

	private:
		Image *bitmaps[9];

		Scrollbar *hscrollbar;
		Scrollbar *vscrollbar;
};

#endif // __H_WINDOW__
