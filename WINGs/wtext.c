/*
 *  WINGs WMText: multi-line/font/color/graphic text widget
 *
 *  Copyright (c) 1999-2000 Nwanua Elumeze
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include "WINGsP.h"
#include <X11/keysym.h>
#include <X11/Xatom.h>


#define DO_BLINK 0

/* TODO: 
 *
 * -  use currentTextBlock and neighbours for fast paint and layout
 * -  replace copious uses of Refreshtext with appropriate layOut()...
 * -  code replaceSelection
 *    (deleteSelection is same cept replaceSelection(tPtr, NULL); )
 * -  WMFindInTextStream should also highlight found text...

*/


/* a Section is a section of a TextBlock that describes what parts
 	  of a TextBlock has been laid out on which "line"... 
   o  this greatly aids redraw, scroll and selection. 
   o  this is created during layoutLine, but may be later modified.
   o  there may be many Sections per TextBlock, hence the array */
typedef struct {
	int x, y;				/* where to draw it from */
	int w, h;				/* it's width and height (to aid selection) */
	int _y;
	unsigned short begin, end;	/* what part of the text block */
} Section;


/* a TextBlock is a doubly-linked list of TextBlocks containing:
	o text for the block, color and font
    o or a pointer to the widget and the (text) description for its graphic
	o but NOT both */

typedef struct _TextBlock {
	struct _TextBlock *next;	/* next text block in linked list */
	struct _TextBlock *prior;	/* prior text block in linked list */

	char *text;					/* pointer to 8- or 16-bit text */
								/* or to the object's description */
	union {
		WMFont *font;			/* the font */
		WMWidget *widget;		/* the embedded widget */
	} d;						/* description */

	WMColor *color;				/* the color */
	Section *sections;			/* the region for layouts (a growable array) */
								/* an _array_! of size _nsections_ */
	

	unsigned short used;		/* number of chars in this block */
	unsigned short allocated;	/* size of allocation (in chars) */

	unsigned int first:1;		/* first TextBlock in paragraph */
	unsigned int blank:1;		/* ie. blank paragraph */
	unsigned int kanji:1;		/* is of 16-bit characters or not */
	unsigned int graphic:1;		/* embedded object or text: text=0 */
	unsigned int underlined:1;	/* underlined or not */
	unsigned int nsections:8;	/* over how many "lines" a TexBlock wraps */
			 int script:8;		/* script in points: negative for subscript */
	unsigned int marginN:10;	/* which of the margins in WMText to use */
	unsigned int RESERVED:1;
} TextBlock;


/* somehow visible.h  beats the hell outta visible.size.height :-) */
typedef struct {
	unsigned int y;
	unsigned int x;
	unsigned int h;
	unsigned int w;
} myRect;


typedef struct W_Text {
	W_Class widgetClass;	/* the class number of this widget */
	W_View  *view;			/* the view referring to this instance */

	WMRuler *ruler;			/* the ruler subwiget to manipulate paragraphs */

	WMScroller *vS;			/* the vertical scroller */
	int vpos;				/* the current vertical position */
	int prevVpos;			/* the previous vertical position */

	WMScroller *hS;			/* the horizontal scroller */
	int hpos;				/* the current horizontal position */
	int prevHpos;			/* the previous horizontal position */
							/* in short: tPtr->hS?nowrap:wrap  */

	WMFont *dFont;          /* the default font */
	WMColor *dColor;        /* the default color */
	WMPixmap *dBulletPix;   /* the default pixmap for bullets */

	GC bgGC;				/* the background GC to draw with */
	GC fgGC;				/* the foreground GC to draw with */
	Pixmap db;				/* the buffer on which to draw */

	WMRulerMargins *margins;/* a (growable) array of margins to be used */
							/*	by the various TextBlocks */

	myRect visible;			/* the actual rectangle that can be drawn into */
	myRect cursor;			/* the position and (height) of cursor */
	myRect sel;				/* the selection rectangle */
	int docWidth;			/* the width of the entire document */
	int docHeight;			/* the height of the entire document */


	TextBlock *firstTextBlock;
	TextBlock *lastTextBlock;
	TextBlock *currentTextBlock;

	
	WMBag		*gfxItems;	/* a nice bag containing graphic items */

#if DO_BLINK
	WMHandlerID timerID;	/* for nice twinky-winky */
#endif
	WMPoint clicked;		/* where in the _document_ was clicked */
	unsigned short tpos;	/* the character position in the currentTextBlock */
	unsigned short RESERVED;/* space taker upper... */
	

	WMAction *parser;
	WMAction *writer;

	struct {
		unsigned int monoFont:1;	/* whether to ignore formats */
		unsigned int focused:1;     /* whether this instance has input focus */
		unsigned int editable:1;	/* "silly user, you can't edit me" */
		unsigned int ownsSelection:1; /* "I ownz the current selection!" */
		unsigned int pointerGrabbed:1;/* "heh, gib me pointer" */
		unsigned int buttonHeld:1;	/* the user is holding down the button */
		unsigned int waitingForSelection:1;	/* dum dee dumm... */
		unsigned int extendSelection:1;	/* shift-drag to select more regions */

		unsigned int rulerShown:1;	/* whether the ruler is shown or not */
		unsigned int frozen:1;		/* whether screen updates are to be made */
		unsigned int cursorShown:1;	/* whether to show the cursor */
		unsigned int clickPos:1;	/* clicked before=0 or after=1 a graphic: */
									/* within counts as after too */

		unsigned int ignoreNewLine:1;/* turn it into a ' ' when typed */
		unsigned int laidOut:1;		/* have the TextBlocks all been laid out */
		unsigned int prepend:1; 	/* prepend=1, append=0 (for parsers) */
		WMAlignment alignment:2;	/* the alignment for text */
		WMReliefType relief:3;		/* the relief to display with */
		unsigned int RESERVED:4;
		unsigned int nmargins:10;	/* the number of margin arrays */
	} flags;
} Text;

static char *default_bullet[] = {
	"6 6 4 1",
	"   c None s None", ".  c black",
	"X  c white", "o  c #808080",
	" ...  ",
	".XX.. ",
	".XX..o",
	".....o",
	" ...oo",
	"  ooo "};


static void 
paintText(Text *tPtr)
{   
	TextBlock *tb = tPtr->firstTextBlock;
	WMFont *font;
	GC gc, greyGC;
	char *text;
	int len, y, c, s, done=False;
	int prev_y=-23;
	WMScreen *scr = tPtr->view->screen;
	Display *dpy = tPtr->view->screen->display;
	Window win = tPtr->view->window;
	
	
	if (!tPtr->view->flags.realized || !tPtr->db || tPtr->flags.frozen)
		return;

	XFillRectangle(dpy, tPtr->db, tPtr->bgGC, 
		0, 0, WMWidgetWidth(tPtr), WMWidgetWidth(tPtr));
//tPtr->visible.w, tPtr->visible.h);

	tb = tPtr->firstTextBlock;
    if (!tb)
      goto _copy_area;
        

	if (tPtr->flags.ownsSelection) {
		greyGC = WMColorGC(WMGrayColor(scr));
			//XFillRectangle(dpy, tPtr->db, greyGC,
//			tPtr->sel.x, tPtr->sel.y-tPtr->vpos, tPtr->sel.w, tPtr->sel.h);
	//	XDrawRectangle(dpy, tPtr->db, tPtr->fgGC, 
	//		tPtr->sel.x, tPtr->sel.y-tPtr->vpos, tPtr->sel.w, tPtr->sel.h);
	}

	done = False;
	while (!done && tb) {

		if (tb->graphic) {
			tb = tb->next;
			continue;
		}

		for(s=0; s<tb->nsections && !done; s++) {


			if (tb->sections[s]._y  > tPtr->vpos + tPtr->visible.h) {
				done = True;
				break;
			}

			if ( tb->sections[s].y + tb->sections[s].h < tPtr->vpos) 
				continue; 

			if (tPtr->flags.monoFont) {
				font = tPtr->dFont;
				gc = tPtr->fgGC;
			} else {
				font = tb->d.font;
				gc = WMColorGC(tb->color);
			}

			if (tPtr->flags.ownsSelection) {

				if (prev_y != tb->sections[s]._y
					&& (tb->sections[s]._y >= tPtr->visible.y + tPtr->sel.y) 
					&& (tb->sections[s]._y + tb->sections[s].h 
						<= tPtr->visible.y + tPtr->sel.y + tPtr->sel.h)) {
					XFillRectangle(dpy, tPtr->db, greyGC,
						tPtr->visible.x, 
						tPtr->visible.y + tb->sections[s]._y - tPtr->vpos, 
						tPtr->visible.w,  tb->sections[s].h);

				} else if ( prev_y != tb->sections[s]._y
					&& (tb->sections[s]._y <= tPtr->visible.y + tPtr->sel.y)
					&& (tb->sections[s]._y + tb->sections[s].h 
						>= tPtr->visible.y + tPtr->sel.y)
					&& (tPtr->sel.x >= tb->sections[s].x)
					&& (tPtr->visible.y + tPtr->sel.y + tPtr->sel.h
						>= tb->sections[s]._y + tb->sections[s].h)) {
					XFillRectangle(dpy, tPtr->db, greyGC,
						tPtr->clicked.x, 
						tPtr->visible.y + tb->sections[s]._y - tPtr->vpos,
						tPtr->visible.w - tPtr->sel.x, tb->sections[s].h);

				} else if (prev_y != tb->sections[s]._y
					&& (tb->sections[s]._y <= tPtr->sel.y + tPtr->sel.h)
					&& (tb->sections[s]._y >= tPtr->sel.y)) {  
					XFillRectangle(dpy, tPtr->db, greyGC,
						tPtr->visible.x, 
						tPtr->visible.y + tb->sections[s]._y - tPtr->vpos,
						tPtr->sel.x + tPtr->sel.w -tPtr->visible.x,	
						tb->sections[s].h);

				} else if ( prev_y != tb->sections[s]._y
					&& (tb->sections[s]._y <= tPtr->sel.y)
					&& (tb->sections[s]._y + tb->sections[s].h 
						>= tPtr->visible.y + tPtr->sel.y + tPtr->sel.h)  ) { 
					XFillRectangle(dpy, tPtr->db, greyGC,
						tPtr->sel.x, 
						tPtr->visible.y + tb->sections[s]._y - tPtr->vpos,
						tPtr->sel.w,tb->sections[s].h);
				}
			
			}

			prev_y = tb->sections[s]._y;

			len = tb->sections[s].end - tb->sections[s].begin;
			text = &(tb->text[tb->sections[s].begin]);
			y = tb->sections[s].y - tPtr->vpos;
			WMDrawString(scr, tPtr->db, gc, font, 
				tb->sections[s].x, y, text, len);

			if (tb->underlined) { 
				XDrawLine(dpy, tPtr->db, gc, 	
					tb->sections[s].x, y + font->y + 1,
					tb->sections[s].x + tb->sections[s].w, y + font->y + 1);
			}
						
		}

		tb = (!done? tb->next : NULL);

	}
			
 	c = WMGetBagItemCount(tPtr->gfxItems);
	if (c > 0 && !tPtr->flags.monoFont) {
		int j; 
		WMWidget *wdt;
		for(j=0; j<c; j++) {
			tb = (TextBlock *) WMGetFromBag(tPtr->gfxItems, j);
			wdt = tb->d.widget;
			if (tb->sections[0]._y + tb->sections[0].h <= tPtr->vpos 
				|| tb->sections[0]._y >= tPtr->vpos + tPtr->visible.h ) {
		
				if ((W_VIEW(wdt))->flags.mapped) {
					WMUnmapWidget(wdt);
				}
			} else {
				if (!(W_VIEW(wdt))->flags.mapped) {
					WMMapWidget(wdt);
					//WMLowerWidget(wdt);
				}
				
				if (tPtr->flags.ownsSelection && 0
					//&& (tb->sections[s]._y >= tPtr->sel.y) 
					//&& (tb->sections[s]._y + tb->sections[s].h 
					){ //	<= tPtr->sel.y + tPtr->sel.h)) {
					XFillRectangle(dpy, tPtr->db, greyGC,
						tb->sections[0].x, tb->sections[0].y - tPtr->vpos, 
						tb->sections[0].w, tb->sections[0].h);
				}

				WMMoveWidget(wdt, 3 + tb->sections[0].x + tPtr->visible.x, 
					tb->sections[0].y - tPtr->vpos);

				if (tb->underlined) { 
					XDrawLine(dpy, tPtr->db, WMColorGC(tb->color), 
					tb->sections[0].x,
					tb->sections[0].y + WMWidgetHeight(wdt) + 1,
					tb->sections[0].x + tb->sections[0].w,
					tb->sections[0].y + WMWidgetHeight(wdt) + 1);
		} } } }
	

_copy_area:

	if (tPtr->flags.editable && tPtr->flags.cursorShown 
		&& tPtr->cursor.x != -23 && tPtr->flags.focused) { 
		int y = tPtr->cursor.y - tPtr->vpos;
		XDrawLine(dpy, tPtr->db, tPtr->fgGC,
			tPtr->cursor.x, y,
			tPtr->cursor.x, y + tPtr->cursor.h);
	}
//printf("%d %d %d\n", tPtr->cursor.x, tPtr->cursor.y, tPtr->cursor.h);


	XCopyArea(dpy, tPtr->db, win, tPtr->bgGC, 
		0, 0,
        tPtr->visible.w, tPtr->visible.h, 
		tPtr->visible.x, tPtr->visible.y);

	W_DrawRelief(scr, win, 0, 0,
		tPtr->view->size.width, tPtr->view->size.height, 
		tPtr->flags.relief);		

	if (tPtr->ruler && tPtr->flags.rulerShown)
		XDrawLine(dpy, win, 
			tPtr->fgGC, 2, 42, 
			tPtr->view->size.width-4, 42);

}


#if DO_BLINK

#define CURSOR_BLINK_ON_DELAY   600
#define CURSOR_BLINK_OFF_DELAY  400

static void
blinkCursor(void *data)
{
    Text *tPtr = (Text*)data;

	if (tPtr->flags.cursorShown) {
		tPtr->timerID = WMAddTimerHandler(CURSOR_BLINK_OFF_DELAY, 
			blinkCursor, data);
    } else {
		tPtr->timerID = WMAddTimerHandler(CURSOR_BLINK_ON_DELAY, 
			blinkCursor, data);
    }
	paintText(tPtr);
	tPtr->flags.cursorShown = !tPtr->flags.cursorShown;
}
#endif

static TextBlock *
getFirstNonGraphicBlockFor(TextBlock *tb, short dir)
{
	if (!tb)
		return NULL;
	while (tb) {
		if (!tb->graphic)
			break;
		tb = (dir? tb->next : tb->prior);
	}

	return tb;
}
		

static  void
cursorToTextPosition(Text *tPtr, int x, int y)
{
	TextBlock *tb = NULL;
	int done=False, s, pos, len, _w, _y, dir=1; /* 1 == "down" */
	char *text;

	y += (tPtr->vpos - tPtr->visible.y);
	if (y<0) 
		y = 0;

	x -= (tPtr->visible.x - 2); 
	if (x<0) 
		x=0;

	/* clicked is relative to document, not window... */
	tPtr->clicked.x = x;
	tPtr->clicked.y = y;

	if (! (tb = tPtr->currentTextBlock)) {
		if (! (tb = tPtr->firstTextBlock)) {
			tPtr->tpos = 0;
			tPtr->cursor.h = tPtr->dFont->height;
			tPtr->cursor.y = 2;
			tPtr->cursor.x = 2;
			return;
		}
	}

	/* first, which direction? Most likely, newly clicked 
		position will be close to previous */
	dir = !(y < tb->sections[0].y);

	if (tPtr->flags.monoFont && tb->graphic) {
		tb = getFirstNonGraphicBlockFor(tb, 1);
		if (!tb) {
			tPtr->currentTextBlock = 
				(dir? tPtr->lastTextBlock : tPtr->firstTextBlock);
			tPtr->tpos = 0;
			return;
		}
	}

	s = (dir? 0 : tb->nsections-1);
	if ( y >= tb->sections[s]._y 
		&& y <= tb->sections[s]._y + tb->sections[s].h) { 
			printf("got it\n");
			goto _doneV;
	}

	/* get the first section of the TextBlock that lies about 
		the vertical click point */
	done = False;
	while (!done && tb) { 

		if (tPtr->flags.monoFont && tb->graphic) {
			if(tb->next)
				tb = tb->next;
			continue;
		}

		s = (dir? 0 : tb->nsections-1);
		while (!done && (dir? (s<tb->nsections) : (s>=0) )) {

			if ( y >= tb->sections[s]._y 
				&& y <= tb->sections[s]._y + tb->sections[s].h) { 
					done = True;
				} else {
					dir? s++ : s--;
				}
		}
					
		if (!done) {
			if ( (dir? tb->next : tb->prior))  {
				tb = (dir ? tb->next : tb->prior);
			} else {
				pos = tb->used;
				break; //goto _doneH;
			}
		}
	}

	
	if (s<0 || s>=tb->nsections) {
		s = (dir? tb->nsections-1 : 0);
	}

_doneV:
	/* we have the line, which TextBlock on that line is it? */
	pos = 0;
	if (tPtr->flags.monoFont && tb->graphic)
		tb = getFirstNonGraphicBlockFor(tb, dir);
	if (tb)  {
		if ((dir? tb->sections[s].x >= x : tb->sections[s].x < x))
			; //goto _doneH;
		_y = tb->sections[s]._y;
	}

	while (tb) { 

		if (tPtr->flags.monoFont && tb->graphic) {
			tb = (dir ? tb->next : tb->prior);
			continue;
		}

		if (dir) {
			if (tb->graphic) {
				_w = WMWidgetWidth(tb->d.widget);
			} else {
				text = &(tb->text[tb->sections[s].begin]);
				len = tb->sections[s].end - tb->sections[s].begin;
				_w = WMWidthOfString(tb->d.font, text, len);
				if (tb->sections[s].x + _w >= x)
					break;
				
			}
		} else { 
			if (tb->sections[s].x  <= x)
				break;
		}

#if 0
		if ((dir? tb->next : tb->prior)) {
			TextBlock *nxt = (dir? tb->next : tb->prior);
			if (tPtr->flags.monoFont && nxt->graphic) {
				nxt = getFirstNonGraphicBlockFor(nxt, dir);
				if (!nxt) {
					pos = 0;
					goto _doneH;
				}
			}
			if (_y != nxt->sections[s]._y) {
				/* this must be the last/first on this line. stop */
				pos = (dir? tb->used : 0);
				goto _doneH;
			}
		}
#endif

		if ( (dir? tb->next : tb->prior))  {
			tb = (dir ? tb->next : tb->prior);
		} else { 
			done = True;
			break;
		}

		if (tb) 
			s = (dir? 0 : tb->nsections-1);
	}
		
	/* we have said TextBlock, now where within it? */
	if (tb && !tb->graphic) {
		WMFont *f = tb->d.font;
		len = tb->sections[s].end - tb->sections[s].begin;
		text = &(tb->text[tb->sections[s].begin]);

		_w = x - tb->sections[s].x;
		pos = 0;

		while (pos<len && WMWidthOfString(f, text, pos+1) <  _w)
			pos++;

		tPtr->cursor.x = tb->sections[s].x + 
			(pos? WMWidthOfString(f, text, pos) : 0);
		pos += tb->sections[s].begin;
		tPtr->tpos = (pos<tb->used)? pos : tb->used;
	}
		
	tPtr->currentTextBlock = tb;
	tPtr->cursor.h = tb->sections[s].h;
	tPtr->cursor.y = tb->sections[s]._y;

	
if (!tb)
printf("will hang :-)\n");

}

static void
updateScrollers(Text *tPtr)
{   

	if (tPtr->flags.frozen)
		return;

	if (tPtr->vS) {
		if (tPtr->docHeight < tPtr->visible.h) {
			WMSetScrollerParameters(tPtr->vS, 0, 1);
			tPtr->vpos = 0;
		} else {   
			float vmax = (float)(tPtr->docHeight);
			WMSetScrollerParameters(tPtr->vS,
				((float)tPtr->vpos)/(vmax - (float)tPtr->visible.h),
				(float)tPtr->visible.h/vmax);
		}       
	} else tPtr->vpos = 0;
    
	if (tPtr->hS)
		;
} 

static void
scrollersCallBack(WMWidget *w, void *self)
{   
	Text *tPtr = (Text *)self;
    Bool scroll = False;
    Bool dimple = False;
	int which;

	if (!tPtr->view->flags.realized || tPtr->flags.frozen) 
		return;

	if (w == tPtr->vS) {
		float vmax; 
		int height; 
		vmax = (float)(tPtr->docHeight);
		height = tPtr->visible.h;

 		which = WMGetScrollerHitPart(tPtr->vS);
		switch(which) { 
			case WSDecrementLine:
				if (tPtr->vpos > 0) {
					if (tPtr->vpos>16) tPtr->vpos-=16;
					else tPtr->vpos=0;
					scroll=True;
			}break;
			case WSIncrementLine: {
				int limit = tPtr->docHeight - height;
				if (tPtr->vpos < limit) {
					if (tPtr->vpos<limit-16) tPtr->vpos+=16;
					else tPtr->vpos=limit;
					scroll = True;
			}}break;
			case WSDecrementPage:
				tPtr->vpos -= height;

				if (tPtr->vpos < 0)
					tPtr->vpos = 0;
				dimple = True;
				scroll = True;
				printf("dimple needs to jump to mouse location ;-/\n");
				break;
			case WSIncrementPage:
				tPtr->vpos += height;
				if (tPtr->vpos > (tPtr->docHeight - height))
					tPtr->vpos = tPtr->docHeight - height;
				dimple = True;
				scroll = True;
				printf("dimple needs to jump to mouse location ;-/\n");
			break;
            
            
			case WSKnob:
				tPtr->vpos = WMGetScrollerValue(tPtr->vS)
					* (float)(tPtr->docHeight - height);
					scroll = True;
			break; 
            
			case WSKnobSlot:
			case WSNoPart:
printf("WSNoPart, WSKnobSlot\n");
#if 0
float vmax = (float)(tPtr->docHeight);
((float)tPtr->vpos)/(vmax - (float)tPtr->visible.h), 
(float)tPtr->visible.h/vmax;
dimple =where mouse is.
#endif
			break;
		} 
		scroll = (tPtr->vpos != tPtr->prevVpos);
		tPtr->prevVpos = tPtr->vpos;
	}
		
	if (w == tPtr->hS)
		;
    
	if (scroll) {
/*
		if (0&&dimple) { 
			if (tPtr->rulerShown) 
				XClearArea(tPtr->view->screen->display, tPtr->view->window, 22, 47,
					tPtr->view->size.width-24, tPtr->view->size.height-49, True);
			else    
				XClearArea(tPtr->view->screen->display, tPtr->view->window, 22, 2,
					tPtr->view->size.width-24, tPtr->view->size.height-4, True);
		 	}
*/
		if (which == WSDecrementLine || which == WSIncrementLine)
			updateScrollers(tPtr);
		paintText(tPtr);
	}
}



typedef struct {
    TextBlock *tb;
	unsigned short begin, end;	/* what part of the text block */
	
} myLineItems;


static int
layOutLine(Text *tPtr, myLineItems *items, int nitems, int x, int y,
int pwidth)
{   
	int i, j=0, lw = 0, line_height=0, max_d=0, len, n;
	WMFont *font;
	char *text;
	TextBlock *tb;
	Bool gfx=0;
	TextBlock *tbsame=NULL;

	for(i=0; i<nitems; i++) {
		tb = items[i].tb;

		if (tb->graphic) { 
			if (!tPtr->flags.monoFont) {
				WMWidget *wdt = tb->d.widget;
				line_height = WMAX(line_height, WMWidgetHeight(wdt));
				if (tPtr->flags.alignment != WALeft) 
					lw += WMWidgetWidth(wdt);
				gfx = True;
			}
			
		} else { 
			font = (tPtr->flags.monoFont)?tPtr->dFont : tb->d.font;
			max_d = WMAX(max_d, font->height-font->y); 
			line_height = WMAX(line_height, font->height);
			text = &(tb->text[items[i].begin]);
			len = items[i].end - items[i].begin;
			if (tPtr->flags.alignment != WALeft) 
				lw += WMWidthOfString(font,	text, len);
		}
	}
	
	if (tPtr->flags.alignment == WARight) {
		j = pwidth - lw;
	} else if (tPtr->flags.alignment == WACenter) {
		j = (int) ((float)(pwidth - lw))/2.0;
	}   
	if (gfx)
		y+=10;

	for(i=0; i<nitems; i++) {
		tb = items[i].tb;

		if (tbsame == tb) {  /*extend it, since it's on same line */
			tb->sections[tb->nsections-1].end = items[i].end;
			n = tb->nsections-1;
		} else {
			tb->sections = wrealloc(tb->sections,
				(++tb->nsections)*sizeof(Section));
			n = tb->nsections-1;
			tb->sections[n]._y = y;
			tb->sections[n].x = x+j;
			tb->sections[n].h = line_height;
			tb->sections[n].begin = items[i].begin;
			tb->sections[n].end = items[i].end;
		}
		if (tb->graphic) { 
			if (!tPtr->flags.monoFont) {
				WMWidget *wdt = tb->d.widget;
				tb->sections[n].y = 1 + max_d +
					y + line_height - WMWidgetHeight(wdt);
				tb->sections[n].w = WMWidgetWidth(wdt);
				x += tb->sections[n].w;
			}
		} else {
			font = (tPtr->flags.monoFont)? tPtr->dFont : tb->d.font;
			len = items[i].end - items[i].begin;
			text = &(tb->text[items[i].begin]);

			tb->sections[n].y = y+line_height-font->y;
			tb->sections[n].w = 
				WMWidthOfString(font,  
					&(tb->text[tb->sections[n].begin]), 
					tb->sections[n].end - tb->sections[n].begin);

			x += WMWidthOfString(font,  text, len); 
		} 

		tbsame = tb;
	}

	return line_height+(gfx?10:0);
					
}


static void 
output(char *ptr, int len)
{   
    char s[len+1]; 
    memcpy(s, ptr, len);
    s[len] = 0; 
    //printf(" s is [%s] (%d)\n",  s, strlen(s));
    printf("[%s]\n",  s);
}


/* tb->text doesn't necessarily end in '\0' hmph! (strchr) */
static inline char *
mystrchr(char *s, char needle, unsigned short len)
{
	char *haystack = s;

	if (!haystack || len < 1)
		return NULL;

	while ( (int) (haystack - s) < len ) {
		if (*haystack == needle)	
			return haystack; 
		haystack++;
	}
	return NULL;
}

#define MAX_TB_PER_LINE 64

static void
layOutDocument(Text *tPtr) 
{
	TextBlock *tb;
	myLineItems items[MAX_TB_PER_LINE];
	WMFont *font;
	Bool lhc = !tPtr->flags.laidOut; /* line height changed? */
	int prev_y;

	int nitems=0, x=0, y=0, lw = 0, width=0;
	int pwidth = tPtr->visible.w - tPtr->visible.x; 
		
	char *start=NULL, *mark=NULL;
	int begin, end;

	if (tPtr->flags.frozen)
		return;

	if (!(tb = tPtr->firstTextBlock)) 
		return;

	if (0&&tPtr->flags.laidOut) {
		tb = tPtr->currentTextBlock;
		if (tb->sections && tb->nsections>0) 
			prev_y = tb->sections[tb->nsections-1]._y;
y+=10;
printf("1 prev_y %d \n", prev_y); 

		/* search backwards for textblocks on same line */
		while (tb) {
			if (!tb->sections || tb->nsections<1) {
				tb = tPtr->firstTextBlock;
				break;
			}
			if (tb->sections[tb->nsections-1]._y != prev_y) {
				tb = tb->next;
				break;
			}
		//	prev_y = tb->sections[tb->nsections-1]._y;
			tb = tb->prior;
		}
		y = 0;//tb->sections[tb->nsections-1]._y;
printf("2 prev_y %d \n\n", tb->sections[tb->nsections-1]._y); 
	}


	while (tb) {

		if (tb->sections && tb->nsections>0) {
			wfree(tb->sections);
			tb->sections = NULL;
			tb->nsections = 0;
		} 

		if (tb->first) {
			y += layOutLine(tPtr, items, nitems, x, y, pwidth);
			x = 0;//tPtr->visible.x+2;
			nitems = 0;
			lw = 0;
		}
			
		if (tb->graphic) { 
			if (!tPtr->flags.monoFont) { 
				width = WMWidgetWidth(tb->d.widget);
				if (width > pwidth)printf("rescale graphix to fit?\n");
				lw += width;
				if (lw >= pwidth - x 
				|| nitems >= MAX_TB_PER_LINE) {
					y += layOutLine(tPtr, items, nitems, x, y, pwidth);
					nitems = 0;
					x = 0;//tPtr->visible.x+2;
					lw = width;
				}

    			items[nitems].tb = tb;
    			items[nitems].begin = 0;
    			items[nitems].end = 0;
				nitems++;
			}

		} else if ((start = tb->text)) {
			begin = end = 0;
			font = tPtr->flags.monoFont?tPtr->dFont:tb->d.font;

			while (start) {
				mark = mystrchr(start, ' ', tb->used);
				if (mark) {
					end += (int)(mark-start)+1;
					start = mark+1;
				} else {
					end += strlen(start);
					start = mark;
				}

				if (end > tb->used)
					end = tb->used;

				if (end-begin > 0) {
					
					width = WMWidthOfString(font, 
						&tb->text[begin], end-begin);

					if (width > pwidth) { /* break this tb up */
						char *t = &tb->text[begin];
						int l=end-begin, i=0;
						do { 
							width = WMWidthOfString(font, t, ++i);
						} while (width < pwidth && i < l);  
						end = begin+i;
						if (start) // and since (nil)-4 = 0xfffffffd
							 start -= l-i;	
					}

					lw += width;
				}

				if ((lw >= pwidth - x) 
				|| nitems >= MAX_TB_PER_LINE) { 
					y += layOutLine(tPtr, items, nitems, x, y, pwidth); 
					lw = width;
					x = 0; //tPtr->visible.x+2;
					nitems = 0;
				}

				items[nitems].tb = tb;
				items[nitems].begin = begin;
				items[nitems].end = end;
				nitems++;

				begin = end;
			}
		}
		tb = tb->next;
	}


	if (nitems > 0) 
		y += layOutLine(tPtr, items, nitems, x, y, pwidth);               
	if (lhc) {
		tPtr->docHeight = y+10;
		updateScrollers(tPtr);
	}
	tPtr->flags.laidOut = True;
		
}


static void
textDidResize(W_ViewDelegate *self, WMView *view)
{   
	Text *tPtr = (Text *)view->self;
	unsigned short w = WMWidgetWidth(tPtr);
	unsigned short h = WMWidgetHeight(tPtr);
	unsigned short rh = 0, vw = 0;

	if (tPtr->ruler && tPtr->flags.rulerShown) { 
		WMMoveWidget(tPtr->ruler, 2, 2);
		WMResizeWidget(tPtr->ruler, w - 4, 40);
		rh = 40;
	}   
		
	if (tPtr->vS) { 
		WMMoveWidget(tPtr->vS, 1, rh + 2);
		WMResizeWidget(tPtr->vS, 20, h - rh - 3);
		vw = 20;
		WMSetRulerOffset(tPtr->ruler,22);
	} else WMSetRulerOffset(tPtr->ruler, 2);

	if (tPtr->hS) {
		if (tPtr->vS) {
			WMMoveWidget(tPtr->hS, vw, h - 21);
			WMResizeWidget(tPtr->hS, w - vw - 1, 20);
		} else {
			WMMoveWidget(tPtr->hS, vw+1, h - 21);
			WMResizeWidget(tPtr->hS, w - vw - 2, 20);
		}
	}

	tPtr->visible.x = (tPtr->vS)?22:2;
	tPtr->visible.y = (tPtr->ruler && tPtr->flags.rulerShown)?43:3;
	tPtr->visible.w = tPtr->view->size.width - tPtr->visible.x - 12;
	tPtr->visible.h = tPtr->view->size.height - tPtr->visible.y;
	tPtr->visible.h -= (tPtr->hS)?20:0;

	tPtr->margins[0].left = tPtr->margins[0].right = tPtr->visible.x;
	tPtr->margins[0].body = tPtr->visible.x;
	tPtr->margins[0].right = tPtr->visible.w;


	if (tPtr->view->flags.realized) {

		if (tPtr->db) {
			XFreePixmap(tPtr->view->screen->display, tPtr->db);
			tPtr->db = (Pixmap) NULL;
		}

		if (tPtr->visible.w < 10) tPtr->visible.w = 10;
		if (tPtr->visible.h < 10) tPtr->visible.h = 10;
	
		//if (size change or !db
		if(!tPtr->db) { 
			tPtr->db = XCreatePixmap(tPtr->view->screen->display, 
				tPtr->view->window, tPtr->visible.w, 
				tPtr->visible.h, tPtr->view->screen->depth);
		}
	}
	
	WMRefreshText(tPtr, tPtr->vpos, tPtr->hpos);
}

W_ViewDelegate _TextViewDelegate =
{
	NULL,
	NULL,
	textDidResize,
	NULL,
};

/* nice, divisble-by-16 blocks */
static inline unsigned short
reqBlockSize(unsigned short requested)
{
	return requested + 16 - (requested%16);
}


static void
clearText(Text *tPtr)
{
	if (!tPtr->firstTextBlock) 
		return;

	while (tPtr->currentTextBlock) 
		WMDestroyTextBlock(tPtr, WMRemoveTextBlock(tPtr));

	tPtr->firstTextBlock = NULL;
	tPtr->currentTextBlock = NULL;
	tPtr->lastTextBlock = NULL;
}

static void
deleteTextInteractively(Text *tPtr, KeySym ksym)
{
	TextBlock *tb = tPtr->currentTextBlock;

	if (!tb)
		return;

	if (ksym == XK_BackSpace) { 
		if ( (tb->used > 0)  && (tPtr->tpos > 0) 
			&& (tPtr->tpos <= tb->used) && !tb->graphic) {
			tPtr->tpos--;
			memmove(&(tb->text[tPtr->tpos]),
				&(tb->text[tPtr->tpos + 1]), tb->used - tPtr->tpos);
			tb->used--;
		}

		if (tPtr->tpos < 1) { 
			TextBlock *prior = tb->prior;
			if	(tb->used == 0) 
				WMDestroyTextBlock(tPtr, WMRemoveTextBlock(tPtr));
			if (prior || !tPtr->currentTextBlock) {
				tPtr->currentTextBlock = prior;
				if(tPtr->currentTextBlock)
					tPtr->tpos = tPtr->currentTextBlock->used;
			} else tPtr->tpos = 0;
				
		}

	}


#if 0
		} else if (!back && tb->next) {
			TextBlock *next = tb->next; 
			tPtr->currentTextBlock = next;
			tPtr->tpos = 0;
		}
	} 
#endif
	
	

}
	

static void
insertTextInteractively(Text *tPtr, char *text, int len)
{
	TextBlock *tb;
	char *newline = NULL;
    
	if (!tPtr->flags.editable || len < 1 || !text)
		return;

	if(tPtr->flags.ignoreNewLine && *text == '\n' && len == 1)
		return;

	if (tPtr->flags.ignoreNewLine) {
		int i;
		for(i=0; i<len; i++) {
			if (text[i] == '\n') 
				text[i] = ' ';
		}
	}

	tb = tPtr->currentTextBlock;
	if (!tb || tb->graphic) {
		text[len] = 0;
		WMAppendTextStream(tPtr, text);
		if (tPtr->currentTextBlock) { 
			tPtr->tpos = tPtr->currentTextBlock->used;
		}
		return;
	}

	if ((newline = strchr(text, '\n'))) {
		int nlen = (int)(newline-text);
		int s = tb->used - tPtr->tpos;
		char save[s];

		if (!tb->blank && (1||nlen>0 )) {
			if (1||(s > 0) ) { 
    			memcpy(save, &tb->text[tPtr->tpos], s);
				tb->used -= (tb->used - tPtr->tpos);
			}
			text[nlen] = 0;
			insertTextInteractively(tPtr, text, nlen);
			newline++;
			WMAppendTextStream(tPtr, newline);
			if (s>0)
				insertTextInteractively(tPtr, save, s);
		} else { 
			if(tb->blank) {
				WMAppendTextStream(tPtr, text);
			} else {
				printf("split me\n");
			}
				
		}

	} else { 
		if (tb->used + len >= tb->allocated) {
			tb->allocated = reqBlockSize(tb->used+len);
			tb->text = wrealloc(tb->text, tb->allocated);
		}
		
		if (tb->blank) {
			memcpy(tb->text, text, len);
			tb->used = len;
 			tPtr->tpos = len;
			tb->blank = False;
		} else {
			memmove(&(tb->text[tPtr->tpos+len]), &tb->text[tPtr->tpos],
				tb->used-tPtr->tpos+1);
			memmove(&tb->text[tPtr->tpos], text, len);
 			tb->used += len;
 			tPtr->tpos += len;
		}

	}
}


static void 
selectRegion(Text *tPtr, int x, int y)
{
	if (x < 0 || y < 0) 
		return;
	y += (tPtr->flags.rulerShown? 40: 0);
	y += tPtr->vpos;
	if (y>10) y -= 10;  /* the original offset */

	x -= tPtr->visible.x-2;
	if (x<0) x=0;

    tPtr->sel.x = WMAX(0, WMIN(tPtr->clicked.x, x));
    tPtr->sel.w = abs(tPtr->clicked.x - x);
    tPtr->sel.y = WMAX(0, WMIN(tPtr->clicked.y, y));
    tPtr->sel.h = abs(tPtr->clicked.y - y);

	tPtr->flags.ownsSelection = True;
	paintText(tPtr);
}


static void
pasteText(WMView *view, Atom selection, Atom target, Time timestamp,
	void *cdata, WMData *data)
{     
	Text *tPtr = (Text *)view->self;
	char *str; 
    
	tPtr->flags.waitingForSelection = False;

	if (data) {
		str = (char*)WMDataBytes(data);
		if (tPtr->parser) { 
			(tPtr->parser) (tPtr, (void *) str);
		} else { 
			insertTextInteractively(tPtr, str, strlen(str));
		}
		WMRefreshText(tPtr, tPtr->vpos, tPtr->hpos);
	} else {
		int n;
		str = XFetchBuffer(tPtr->view->screen->display, &n, 0);
		if (str) {
			str[n] = 0;
			if (tPtr->parser) { 
				(tPtr->parser) (tPtr, (void *) str);
			} else { 
				insertTextInteractively(tPtr, str, n);
			}
			WMRefreshText(tPtr, tPtr->vpos, tPtr->hpos);
			XFree(str);
		}
	}
}

static  void
releaseSelection(Text *tPtr)
{
	tPtr->flags.ownsSelection = False;
	paintText(tPtr);
}

static WMData *
requestHandler(WMView *view, Atom selection, Atom target, 
	void *cdata, Atom *type)
{
	Text *tPtr = view->self;
	Display *dpy = tPtr->view->screen->display;
	Atom TEXT = XInternAtom(dpy, "TEXT", False);
	Atom COMPOUND_TEXT = XInternAtom(dpy, "COMPOUND_TEXT", False);
	
	*type = target;
	if (target == XA_STRING || target == TEXT || target == COMPOUND_TEXT) {
		char *text = WMGetTextSelected(tPtr);
		WMData *data = WMCreateDataWithBytes(text, strlen(text));
		wfree(text);
		return data;
	} else { 
		Atom PIXMAP = XInternAtom(dpy, "PIXMAP", False);
		WMData *data = WMCreateDataWithBytes("bleh", 4);
		if(target == PIXMAP) {
			printf("whoa! pixmap WMGetTextSelectedObjects\n");
		}
		return data;
	}

	return NULL;
}
		

static void
lostHandler(WMView *view, Atom selection, void *cdata)
{
	releaseSelection((WMText *)view->self);
}   

static WMSelectionProcs selectionHandler = {
	requestHandler, lostHandler, NULL 
};

static void
ownershipObserver(void *observerData, WMNotification *notification)
{
	WMText *to = (WMText *)observerData;
    WMText *tw = (WMText *)WMGetNotificationClientData(notification);
    if (to != tw) 
		lostHandler(to->view, XA_PRIMARY, NULL);
}


static  void
handleTextKeyPress(Text *tPtr, XEvent *event)
{
	char buffer[2];
	KeySym ksym;
	int control_pressed = False;
//	int h=1;

	if (((XKeyEvent *) event)->state & ControlMask)
		control_pressed = True;
    buffer[XLookupString(&event->xkey, buffer, 1, &ksym, NULL)] = 0;
    
    switch(ksym) {
		case XK_Right:
		case XK_Left:
		case XK_Down:
		case XK_Up: 
			printf("arrows %ld\n", ksym);
		break;

		case XK_BackSpace:
		case XK_Delete:
		case XK_KP_Delete:
			deleteTextInteractively(tPtr, ksym);
			WMRefreshText(tPtr, tPtr->vpos, tPtr->hpos);
		break;

		case XK_Control_R :
		case XK_Control_L :
			control_pressed = True;
		break;

		case XK_Return:
			buffer[0] = '\n';
		default:
		if (buffer[0] != 0 && !control_pressed) {
			insertTextInteractively(tPtr, buffer, 1);
			WMRefreshText(tPtr, tPtr->vpos, tPtr->hpos);

		} else if (control_pressed && ksym==XK_r) {
			Bool i = !tPtr->flags.rulerShown; 
			WMShowTextRuler(tPtr, i);
			tPtr->flags.rulerShown = i;
		 }   
		else if (control_pressed && buffer[0] == '') 
				XBell(tPtr->view->screen->display, 0);
	}

	if (!control_pressed && tPtr->flags.ownsSelection) 
		releaseSelection(tPtr);
}

static void
handleWidgetPress(XEvent *event, void *data)
{
	TextBlock *tb = (TextBlock *)data;
	Text *tPtr;
	WMWidget *w;

	if (!tb)
		return;
	/* this little bit of nastiness here saves a boatload of trouble */
	w = (WMWidget *)(((W_VIEW(tb->d.widget))->parent)->self);
	if (W_CLASS(w) != WC_Text) 
		return;
	tPtr = (Text*)w;
	tPtr->currentTextBlock = getFirstNonGraphicBlockFor(tb, 1);
	if (!tPtr->currentTextBlock)
		tPtr->currentTextBlock = tb;
	tPtr->tpos = 0;
	output(tPtr->currentTextBlock->text, tPtr->currentTextBlock->used);
	//if (!tPtr->flags.focused) {
	//	WMSetFocusToWidget(tPtr);
    //  tPtr->flags.focused = True;
	//} 	

}

static void
handleActionEvents(XEvent *event, void *data)
{
	Text *tPtr = (Text *)data;
	Display *dpy = event->xany.display;
	KeySym ksym;
	

	switch (event->type) {
		case KeyPress:
			ksym = XLookupKeysym((XKeyEvent*)event, 0);  
			if (ksym == XK_Shift_R || ksym == XK_Shift_L) { 
				tPtr->flags.extendSelection = True;
				return;
			} 
			if (!tPtr->flags.editable || tPtr->flags.buttonHeld) {
				XBell(dpy, 0);
				return;
			}

			if (tPtr->flags.waitingForSelection) 
				return;
			if (tPtr->flags.focused) {
				XGrabPointer(dpy, W_VIEW(tPtr)->window, False, 
					PointerMotionMask|ButtonPressMask|ButtonReleaseMask,
					GrabModeAsync, GrabModeAsync, None, 
					tPtr->view->screen->invisibleCursor, CurrentTime);
				tPtr->flags.pointerGrabbed = True;
				handleTextKeyPress(tPtr, event);
				
			} break;

		case KeyRelease:
			ksym = XLookupKeysym((XKeyEvent*)event, 0);  
			if (ksym == XK_Shift_R || ksym == XK_Shift_L) {
				tPtr->flags.extendSelection = False;
				return;
			//end modify flag so selection can be extended
			} 
		break;


		case MotionNotify:
			if (tPtr->flags.pointerGrabbed) {
				tPtr->flags.pointerGrabbed = False;
				XUngrabPointer(dpy, CurrentTime);
			}
			if ((event->xmotion.state & Button1Mask)) {
				if (!tPtr->flags.ownsSelection) { 
					WMCreateSelectionHandler(tPtr->view, XA_PRIMARY,
						event->xbutton.time, &selectionHandler, NULL);
					tPtr->flags.ownsSelection = True;
				}
				selectRegion(tPtr, event->xmotion.x, event->xmotion.y);
			}
		break;


		case ButtonPress: 
			tPtr->flags.buttonHeld = True;
			if (tPtr->flags.extendSelection) {
				selectRegion(tPtr, event->xmotion.x, event->xmotion.y);
				return;
			}
			if (event->xbutton.button == Button1) { 

				if (!tPtr->flags.focused) {
					WMSetFocusToWidget(tPtr);
              		tPtr->flags.focused = True;
				}	

				if (tPtr->flags.ownsSelection) 
					releaseSelection(tPtr);
				cursorToTextPosition(tPtr, event->xmotion.x, event->xmotion.y);
				paintText(tPtr);
				if (tPtr->flags.pointerGrabbed) {
					tPtr->flags.pointerGrabbed = False;
					XUngrabPointer(dpy, CurrentTime);
					break;
				}   
			}

			if (event->xbutton.button == WINGsConfiguration.mouseWheelDown) 
				WMScrollText(tPtr, -16);
			else if (event->xbutton.button == WINGsConfiguration.mouseWheelUp) 
				WMScrollText(tPtr, 16);
			break;

		case ButtonRelease:
			tPtr->flags.buttonHeld = False;
			if (tPtr->flags.pointerGrabbed) {
				tPtr->flags.pointerGrabbed = False;
				XUngrabPointer(dpy, CurrentTime);
				break;
			}   
			if (event->xbutton.button == WINGsConfiguration.mouseWheelDown
				|| event->xbutton.button == WINGsConfiguration.mouseWheelUp)
				break;

			if (event->xbutton.button == Button2 && tPtr->flags.editable) {
				char *text = NULL;
				int n;
				if (!WMRequestSelection(tPtr->view, XA_PRIMARY, XA_STRING,
					event->xbutton.time, pasteText, NULL)) {
					text = XFetchBuffer(tPtr->view->screen->display, &n, 0);
					if (text) {
						text[n] = 0;
						if (tPtr->parser) { 
							(tPtr->parser) (tPtr, (void *) text);
						} else { 
							insertTextInteractively(tPtr, text, n-1);
						}
						WMRefreshText(tPtr, tPtr->vpos, tPtr->hpos);
						XFree(text);
					} else tPtr->flags.waitingForSelection = True;
			} }

			break;

	}

}


static void 
handleEvents(XEvent *event, void *data)
{
	Text *tPtr = (Text *)data;

	switch(event->type) {
		case Expose: 
			if(!tPtr->db) { 
				tPtr->db = XCreatePixmap(tPtr->view->screen->display, 
				tPtr->view->window, tPtr->visible.w, 
				tPtr->visible.h, tPtr->view->screen->depth);
			}
			if(tPtr->hS) { 
				if (!((W_VIEW(tPtr->hS))->flags.mapped)) {
					WMRealizeWidget(tPtr->hS);
					WMMapWidget(tPtr->hS);
				}
			}
			if(tPtr->vS) { 
				if (!((W_VIEW(tPtr->vS))->flags.mapped)) {
					WMRealizeWidget(tPtr->vS);
					WMMapWidget(tPtr->vS);
				}
			}
			if (!event->xexpose.count && tPtr->view->flags.realized)
				paintText(tPtr);
		break;

		case FocusIn: 
			if (W_FocusedViewOfToplevel(W_TopLevelOfView(tPtr->view))
				!= tPtr->view)
				return;
			tPtr->flags.focused = True;
#if DO_BLINK
			if (tPtr->flags.editable && !tPtr->timerID) {
				tPtr->timerID = WMAddTimerHandler(12+0*CURSOR_BLINK_ON_DELAY,
					blinkCursor, tPtr);
			}
#endif

		break;

		case FocusOut: 
			tPtr->flags.focused = False;
			paintText(tPtr);
#if DO_BLINK
			if (tPtr->timerID) {
				WMDeleteTimerHandler(tPtr->timerID);
				tPtr->timerID = NULL;
			}
#endif
        break;

		case DestroyNotify:
			printf("destroy");
		//for(...)WMRemoveTextParagraph(tPtr, para);
        break;

	}
}




static void
insertPlainText(WMText *tPtr, char *text) 
{
	char *start, *mark; 
	void *tb = NULL;
    
	if (!text) {
		clearText(tPtr);
		return;
	}

	start = text;
	while (start) {
		mark = strchr(start, '\n');
		if (mark) {
			tb = WMCreateTextBlockWithText(start, tPtr->dFont, 
				tPtr->dColor, True, (int)(mark-start));
			start = mark+1;
		} else {
			if (start && strlen(start)) {
				tb = WMCreateTextBlockWithText(start, tPtr->dFont,
					tPtr->dColor, False, strlen(start));
			} else tb = NULL;
			start = mark;
		}

		if (tPtr->flags.prepend) 
			WMPrependTextBlock(tPtr, tb);
		else
			WMAppendTextBlock(tPtr, tb);
	}
	return;

}


static void
rulerMoveCallBack(WMWidget *w, void *self)
{   
    Text *tPtr = (Text *)self;
	if (!tPtr)	
		return;
	if (W_CLASS(tPtr) != WC_Text)
		return;
	
	paintText(tPtr);
}

	
static void
rulerReleaseCallBack(WMWidget *w, void *self)
{
	Text *tPtr = (Text *)self;
	if (!tPtr)	
		return;
	if (W_CLASS(tPtr) != WC_Text)
		return;

	WMRefreshText(tPtr, tPtr->vpos, tPtr->hpos);
	return;
}


#if 0
static unsigned
draggingEntered(WMView *self, WMDraggingInfo *info)
{
printf("draggingEntered\n");
	return WDOperationCopy;
}   


static unsigned
draggingUpdated(WMView *self, WMDraggingInfo *info)
{
printf("draggingUpdated\n");
	return WDOperationCopy;
}   


static void
draggingExited(WMView *self, WMDraggingInfo *info)
{
printf("draggingExited\n");
}

static void
prepareForDragOperation(WMView *self, WMDraggingInfo *info)
{
	printf("draggingExited\n");
	return;//"bll"; //"application/X-color";
}   


static Bool
performDragOperation(WMView *self, WMDraggingInfo *info) //, WMData *data)
{
    char *colorName = "Blue";// (char*)WMDataBytes(data);
    WMColor *color; 
	WMText *tPtr = (WMText *)self->self;
    
	if (!tPtr)
		return False;

    if (tPtr->flags.monoFont)
		return False;

    color = WMCreateNamedColor(W_VIEW_SCREEN(self), colorName, True);
printf("color [%s] %p\n", colorName, color);
	if(color) {
	    WMSetTextSelectionColor(tPtr, color);
		WMReleaseColor(color);
	}
    
    return;// True;
}   

static void
concludeDragOperation(WMView *self, WMDraggingInfo *info)
{
	printf("concludeDragOperation\n");
}


static WMDragDestinationProcs _DragDestinationProcs = {
	draggingEntered,
	draggingUpdated,
	draggingExited,
	prepareForDragOperation,
	performDragOperation,
	concludeDragOperation
}; 
#endif


WMText *
WMCreateText(WMWidget *parent)
{
	Text *tPtr = wmalloc(sizeof(Text));
	if (!tPtr) {
		printf("could not create text widget\n");
		return NULL;
	}

#if 0
   printf("sizeof:\n");
    printf(" TextBlock %d\n", sizeof(TextBlock));
    printf(" Section %d\n", sizeof(Section));
    printf(" char * %d\n", sizeof(char *));
    printf(" void * %d\n", sizeof(void *));
    printf(" short %d\n", sizeof(short));
    printf(" Text %d\n", sizeof(Text));
#endif

	memset(tPtr, 0, sizeof(Text));
	tPtr->widgetClass = WC_Text;
	tPtr->view = W_CreateView(W_VIEW(parent));
	if (!tPtr->view) {
		perror("could not create text's view\n");
		free(tPtr);
		return NULL;
    }
	tPtr->view->self = tPtr;
	tPtr->view->attribs.cursor = tPtr->view->screen->textCursor;
	tPtr->view->attribFlags |= CWOverrideRedirect | CWCursor;
	W_ResizeView(tPtr->view, 250, 200);
	tPtr->bgGC = WMColorGC(tPtr->view->screen->white);
	tPtr->fgGC = WMColorGC(tPtr->view->screen->black);
	W_SetViewBackgroundColor(tPtr->view, tPtr->view->screen->white);

	tPtr->ruler = NULL;
    tPtr->vS = NULL;
    tPtr->hS = NULL;

	tPtr->dFont = NULL; 
	//tPtr->dFont = WMCreateFont(tPtr->view->screen, 
	 //	"-*-fixed-medium-r-normal--26-*-*-*-*-*-*-*");
	//"-sony-fixed-medium-r-normal--24-230-75-75-c-120-jisx0201.1976-0");
	//	"-*-times-bold-r-*-*-12-*-*-*-*-*-*-*,"
     //   "-*-fixed-medium-r-normal-*-12-*");
    if (!tPtr->dFont)
		tPtr->dFont = WMRetainFont(tPtr->view->screen->normalFont);

	tPtr->dColor = WMBlackColor(tPtr->view->screen);

	tPtr->view->delegate = &_TextViewDelegate;

#if DO_BLINK
	tPtr->timerID = NULL;
#endif

	WMCreateEventHandler(tPtr->view, ExposureMask|StructureNotifyMask
		|EnterWindowMask|LeaveWindowMask|FocusChangeMask, 
		handleEvents, tPtr);

	WMCreateEventHandler(tPtr->view, ButtonReleaseMask|ButtonPressMask
		|KeyReleaseMask|KeyPressMask|Button1MotionMask, 
		handleActionEvents, tPtr);
	
	WMAddNotificationObserver(ownershipObserver, tPtr, "_lostOwnership", tPtr);
	
#if 0
	WMSetViewDragDestinationProcs(tPtr->view, &_DragDestinationProcs);
	{
		char *types[2] = {"application/X-color", NULL};
		WMRegisterViewForDraggedTypes(tPtr->view, types);
	}
#endif


	

	tPtr->firstTextBlock = NULL;
	tPtr->lastTextBlock = NULL;
	tPtr->currentTextBlock = NULL;
	tPtr->tpos = 0;

	tPtr->gfxItems = WMCreateArrayBag(4);

	tPtr->parser = NULL;
	tPtr->writer = NULL;

	tPtr->sel.x = tPtr->sel.y = 2;
	tPtr->sel.w = tPtr->sel.h = 0;

	tPtr->clicked.x = tPtr->clicked.y = 2;

	tPtr->visible.x = tPtr->visible.y = 2;
	tPtr->visible.h = tPtr->view->size.height;
	tPtr->visible.w = tPtr->view->size.width - 12;
	
	tPtr->cursor.x = -23;

	tPtr->docWidth = 0;
	tPtr->docHeight = 0;
	tPtr->dBulletPix = WMCreatePixmapFromXPMData(tPtr->view->screen,
		default_bullet);
	tPtr->db = (Pixmap) NULL;

	tPtr->margins = wmalloc(sizeof(WMRulerMargins));
	tPtr->margins[0].left = tPtr->margins[0].right = tPtr->visible.x;
	tPtr->margins[0].body = tPtr->visible.x;
	tPtr->margins[0].right = tPtr->visible.w;

	tPtr->flags.nmargins = 1;
	tPtr->flags.rulerShown = False;
	tPtr->flags.monoFont = False;	
	tPtr->flags.focused = False;
	tPtr->flags.editable  = True;
	tPtr->flags.ownsSelection  = False;
	tPtr->flags.pointerGrabbed  = False;
	tPtr->flags.buttonHeld = False;
	tPtr->flags.waitingForSelection  = False;
	tPtr->flags.extendSelection = False;
	tPtr->flags.rulerShown  = False;
	tPtr->flags.frozen  = False;
	tPtr->flags.cursorShown = True;
	tPtr->flags.clickPos = 1;
	tPtr->flags.ignoreNewLine = False;
	tPtr->flags.laidOut = False;
	tPtr->flags.prepend = False;
	tPtr->flags.relief = WRFlat;
	tPtr->flags.alignment = WALeft;

	return tPtr;
}


void 
WMPrependTextStream(WMText *tPtr, char *text) 
{
	if (!tPtr)
		return;

	tPtr->flags.prepend = True;
	if (text && tPtr->parser)
		(tPtr->parser) (tPtr, (void *) text);
	else
		insertPlainText(tPtr, text);

}

void 
WMAppendTextStream(WMText *tPtr, char *text) 
{
	if (!tPtr)
		return;

	tPtr->flags.prepend = False;
	if (text && tPtr->parser)
		(tPtr->parser) (tPtr, (void *) text);
	else
		insertPlainText(tPtr, text);

}


char *
WMGetTextStream(WMText *tPtr)
{
	TextBlock *tb = NULL;
	char *text = NULL;
	unsigned long length = 0, where = 0;
	
	if (!tPtr)
		return NULL;

	if (!(tb = tPtr->firstTextBlock))
		return NULL;

	if (tPtr->writer) {
		(tPtr->writer) (tPtr, (void *) text);
		return text;
	}
	
	/* first, how large a buffer would we want? */
	while (tb) {
		if (!tPtr->flags.ignoreNewLine && (tb->first || tb->blank))
			length += 1;
		if (!tb->graphic)
			length += tb->used;
		tb = tb->next;
	}

	text = wmalloc(length+1); /* +1 for the end of string, let's be nice */
	tb = tPtr->firstTextBlock;
	while (tb) {
		if (!tb->graphic) { 
			if (!tPtr->flags.ignoreNewLine && (tb->first || tb->blank))
				text[where++] = '\n';
			memcpy(&text[where], tb->text, tb->used);
			where += tb->used;
		}
		tb = tb->next;
	}
	
	text[where] = 0;
	
	return text;
}
			
		

WMBag *
WMGetTextStreamObjects(WMText *tPtr)
{
	if (!tPtr)
		return NULL;

	return NULL;
}

	
char * 
WMGetTextSelected(WMText *tPtr)
{	
	TextBlock *tb;
	char *text = NULL;
	
	if (!tPtr)
		return NULL;

 	//tb = tPtr->firstTextBlock;
 	tb = tPtr->currentTextBlock;
	if (!tb)
		return NULL;

	text = wmalloc(tb->used+1);
	memcpy(text, tb->text, tb->used);
	text[tb->used] = 0;
	return text;
}

WMBag *
WMGetTextSelectedObjects(WMText *tPtr)
{
	if (!tPtr)
		return NULL;

	return NULL;
}

void *
WMCreateTextBlockWithObject(WMWidget *w, char *description, WMColor *color, 
	unsigned short first, unsigned short reserved)
{
	TextBlock *tb;
	unsigned short length;

	if (!w || !description || !color) 
		return NULL;

	tb = wmalloc(sizeof(TextBlock));
	if (!tb)
		 return NULL;

	length = strlen(description);
	tb->text = (char *)wmalloc(length);
	memset(tb->text, 0, length);
	memcpy(tb->text, description, length);
	tb->used = length;
	tb->blank = False;
	tb->d.widget = w;	
	tb->color = WMRetainColor(color);
	tb->marginN = 0;
	tb->allocated = 0;
	tb->first = first;
	tb->kanji = False;
	tb->graphic = True;
	tb->underlined = False;
	tb->script = 0;
	tb->sections = NULL;
	tb->nsections = 0;
	tb->prior = NULL;
	tb->next = NULL;

	return tb;
}
	
void *
WMCreateTextBlockWithText(char *text, WMFont *font, WMColor *color, 
	unsigned short first, unsigned short length)
{
	TextBlock *tb;

	if (!font || !color) 
		return NULL;

	tb = wmalloc(sizeof(TextBlock));
	if (!tb)
		 return NULL;

	tb->allocated = reqBlockSize(length);
	tb->text = (char *)wmalloc(tb->allocated);
	memset(tb->text, 0, tb->allocated);

	if (length < 1|| !text ) { // || *text == '\n') {
		*tb->text = ' ';
		tb->used = 1;
		tb->blank = True;
	} else {
		memcpy(tb->text, text, length);
		tb->used = length;
		tb->blank = False;
	}

	tb->d.font = WMRetainFont(font);
	tb->color = WMRetainColor(color);
	tb->marginN = 0;
	tb->first = first;
	tb->kanji = False;
	tb->graphic = False;
	tb->underlined = False;
	tb->script = 0;
	tb->sections = NULL;
	tb->nsections = 0;
	tb->prior = NULL;
	tb->next = NULL;
	return tb;
}

void 
WMSetTextBlockProperties(void *vtb, unsigned int first, 
	unsigned int kanji, unsigned int underlined, int script, 
	unsigned int marginN)
{
	TextBlock *tb = (TextBlock *) vtb;
	if (!tb)
		return;

	tb->first = first;
	tb->kanji = kanji;
	tb->underlined = underlined;
	tb->script = script;
	tb->marginN = marginN;
}
	
void 
WMGetTextBlockProperties(void *vtb, unsigned int *first, 
	unsigned int *kanji, unsigned int *underlined, int *script, 
	unsigned int *marginN)
{
	TextBlock *tb = (TextBlock *) vtb;
	if (!tb)
		return;

	if (first) *first = tb->first;
	if (kanji) *kanji = tb->kanji;
	if (underlined) *underlined = tb->underlined;
	if (script) *script = tb->script;
	if (marginN) *marginN = tb->marginN;
}
	


void 
WMPrependTextBlock(WMText *tPtr, void *vtb)
{
	TextBlock *tb = (TextBlock *)vtb;


	if (!tPtr || !tb)
		return;

	if (tb->graphic) {
		WMWidget *w = tb->d.widget;
		WMCreateEventHandler(W_VIEW(w), ButtonPressMask,
			handleWidgetPress, tb);
		if (W_CLASS(w) != WC_TextField && W_CLASS(w) != WC_Text) {
			(W_VIEW(w))->attribs.cursor = tPtr->view->screen->defaultCursor;
			(W_VIEW(w))->attribFlags |= CWOverrideRedirect | CWCursor;
		}
		WMPutInBag(tPtr->gfxItems, (void *)tb);
		tPtr->tpos = 0;
	} else tPtr->tpos = tb->used;

	if (!tPtr->lastTextBlock || !tPtr->firstTextBlock) {
		tb->next = tb->prior = NULL;
		tPtr->lastTextBlock = tPtr->firstTextBlock 
			= tPtr->currentTextBlock = tb;
		return;
	}

	tb->next = tPtr->currentTextBlock;
	tb->prior = tPtr->currentTextBlock->prior;
	if (tPtr->currentTextBlock->prior)
		tPtr->currentTextBlock->prior->next = tb;

	tPtr->currentTextBlock->prior = tb;
	if (!tb->prior)
		tPtr->firstTextBlock = tb;

	tPtr->currentTextBlock = tb;
}
	

void 
WMAppendTextBlock(WMText *tPtr, void *vtb)
{
	TextBlock *tb = (TextBlock *)vtb;

	if (!tPtr || !tb)
		return;

	if (tb->graphic) {
		WMWidget *w = tb->d.widget;
return;
		WMCreateEventHandler(W_VIEW(w), ButtonPressMask,
			handleWidgetPress, tb);
		if (W_CLASS(w) != WC_TextField && W_CLASS(w) != WC_Text) {
			(W_VIEW(w))->attribs.cursor = tPtr->view->screen->defaultCursor;
			(W_VIEW(w))->attribFlags |= CWOverrideRedirect | CWCursor;
		}
		WMPutInBag(tPtr->gfxItems, (void *)tb);
		tPtr->tpos = 0;
	} else tPtr->tpos = tb->used;

	if (!tPtr->lastTextBlock || !tPtr->firstTextBlock) {
		tb->next = tb->prior = NULL;
		tPtr->lastTextBlock = tPtr->firstTextBlock 
			= tPtr->currentTextBlock = tb;
		return;
	}

	tb->next = tPtr->currentTextBlock->next;
	tb->prior = tPtr->currentTextBlock;
	if (tPtr->currentTextBlock->next)
		tPtr->currentTextBlock->next->prior = tb;
	
	tPtr->currentTextBlock->next = tb;

	if (!tb->next)
		tPtr->lastTextBlock = tb;

	tPtr->currentTextBlock = tb;
}

void  *
WMRemoveTextBlock(WMText *tPtr) 
{
	TextBlock *tb = NULL;

	if (!tPtr || !tPtr->firstTextBlock || !tPtr->lastTextBlock
			|| !tPtr->currentTextBlock) {
		printf("cannot remove non existent TextBlock!\b");
		return tb;
	}

	tb = tPtr->currentTextBlock;
	if (tb->graphic) {
		WMDeleteEventHandler(W_VIEW(tb->d.widget), ButtonPressMask,
			handleWidgetPress, tb);
		WMRemoveFromBag(tPtr->gfxItems, (void *)tb);
		WMUnmapWidget(tb->d.widget);
	}
	
	if (tPtr->currentTextBlock == tPtr->firstTextBlock) {
		if (tPtr->currentTextBlock->next) 
			tPtr->currentTextBlock->next->prior = NULL;

		tPtr->firstTextBlock = tPtr->currentTextBlock->next;
		tPtr->currentTextBlock = tPtr->firstTextBlock;

	} else if (tPtr->currentTextBlock == tPtr->lastTextBlock) {
		tPtr->currentTextBlock->prior->next = NULL;
		tPtr->lastTextBlock = tPtr->currentTextBlock->prior;
		tPtr->currentTextBlock = tPtr->lastTextBlock;
	} else {
		tPtr->currentTextBlock->prior->next = tPtr->currentTextBlock->next;
		tPtr->currentTextBlock->next->prior = tPtr->currentTextBlock->prior;
		tPtr->currentTextBlock = tPtr->currentTextBlock->next;
	}
	
	return (void *)tb;
}

void 
WMDestroyTextBlock(WMText *tPtr, void *vtb)
{
	TextBlock *tb = (TextBlock *)vtb;
	if (!tPtr || !tb)
		return;

	if (tb->graphic) {
		/* naturally, there's a danger to destroying 
		 	widgets whose action brings us here:
		ie. press a button to destroy it... need to 
		find a safer way. till then... this stays commented out */
		//WMDestroyWidget(tb->d.widget);
		//wfree(tb->d.widget);
		tb->d.widget = NULL;
	} else {
		WMReleaseFont(tb->d.font);
	}
		
	WMReleaseColor(tb->color);
	if (tb->sections && tb->nsections > 0)
		wfree(tb->sections);
	wfree(tb->text);
	wfree(tb);
	tb = NULL;
}
	

void 
WMRefreshText(WMText *tPtr, int vpos, int hpos)
{
	//TextBlock *tb;

	if (!tPtr || vpos<0 || hpos<0)
		return;

	if (tPtr->flags.frozen)
		return;


	tPtr->flags.laidOut = False;
	layOutDocument(tPtr);
	updateScrollers(tPtr);
	paintText(tPtr);


}

void
WMSetTextForegroundColor(WMText *tPtr, WMColor *color)
{
	if (!tPtr)
		return;

	if (color) 
		tPtr->fgGC = WMColorGC(color);
	else
		tPtr->fgGC = WMColorGC(WMBlackColor(tPtr->view->screen));

	WMRefreshText(tPtr, tPtr->vpos, tPtr->hpos);
}

void
WMSetTextBackgroundColor(WMText *tPtr, WMColor *color)
{
	if (!tPtr)
		return;

	if (color) {
		tPtr->bgGC = WMColorGC(color);
		W_SetViewBackgroundColor(tPtr->view, color);
	} else {
		tPtr->bgGC = WMColorGC(WMWhiteColor(tPtr->view->screen));
		W_SetViewBackgroundColor(tPtr->view, 
			WMWhiteColor(tPtr->view->screen));
	}

	WMRefreshText(tPtr, tPtr->vpos, tPtr->hpos);
}

void
WMSetTextRelief(WMText *tPtr, WMReliefType relief)
{
	if (!tPtr)
		 return;
	tPtr->flags.relief = relief;
	paintText(tPtr);
}

void 
WMSetTextHasHorizontalScroller(WMText *tPtr, Bool shouldhave)
{
	if (!tPtr) 
		return;

	if (shouldhave && !tPtr->hS) {
		tPtr->hS = WMCreateScroller(tPtr); 
		(W_VIEW(tPtr->hS))->attribs.cursor = tPtr->view->screen->defaultCursor;
		(W_VIEW(tPtr->hS))->attribFlags |= CWOverrideRedirect | CWCursor;
		WMSetScrollerArrowsPosition(tPtr->hS, WSAMaxEnd);
		WMSetScrollerAction(tPtr->hS, scrollersCallBack, tPtr);
	} else if (!shouldhave && tPtr->hS) {
		WMUnmapWidget(tPtr->hS);
		WMDestroyWidget(tPtr->hS);
		tPtr->hS = NULL;
	}

	tPtr->hpos = 0;
	tPtr->prevHpos = 0;
	textDidResize(tPtr->view->delegate, tPtr->view);
}


void
WMSetTextHasRuler(WMText *tPtr, Bool shouldhave)
{
	if (!tPtr) 
		return;
		
	if(shouldhave && !tPtr->ruler) {
		tPtr->ruler = WMCreateRuler(tPtr);
		(W_VIEW(tPtr->ruler))->attribs.cursor =
			tPtr->view->screen->defaultCursor;      
		(W_VIEW(tPtr->ruler))->attribFlags |= CWOverrideRedirect | CWCursor;
		WMRealizeWidget(tPtr->ruler);
		WMSetRulerReleaseAction(tPtr->ruler, rulerReleaseCallBack, tPtr);
		WMSetRulerMoveAction(tPtr->ruler, rulerMoveCallBack, tPtr);
	} else if(!shouldhave && tPtr->ruler) {
		WMShowTextRuler(tPtr, False);
		WMDestroyWidget(tPtr->ruler);
		tPtr->ruler = NULL;
	}
	textDidResize(tPtr->view->delegate, tPtr->view);
}   

void
WMShowTextRuler(WMText *tPtr, Bool show)
{
    if(!tPtr)
        return;
	if(!tPtr->ruler)
		return;

    if(tPtr->flags.monoFont)
        show = False;
             
	tPtr->flags.rulerShown = show;
	if(show) { 
		WMMapWidget(tPtr->ruler);
	} else {
		WMUnmapWidget(tPtr->ruler);
	}
    
    textDidResize(tPtr->view->delegate, tPtr->view);
}   


void 
WMSetTextHasVerticalScroller(WMText *tPtr, Bool shouldhave)
{
	if (!tPtr) 
		return;
		
	if (shouldhave && !tPtr->vS) {
		tPtr->vS = WMCreateScroller(tPtr); 
		(W_VIEW(tPtr->vS))->attribs.cursor = tPtr->view->screen->defaultCursor;
		(W_VIEW(tPtr->vS))->attribFlags |= CWOverrideRedirect | CWCursor;
		WMSetScrollerArrowsPosition(tPtr->vS, WSAMaxEnd);
		WMSetScrollerAction(tPtr->vS, scrollersCallBack, tPtr);
	} else if (!shouldhave && tPtr->vS) {
		WMUnmapWidget(tPtr->vS);
		WMDestroyWidget(tPtr->vS);
		tPtr->vS = NULL;
	}

	tPtr->vpos = 0;
	tPtr->prevVpos = 0;
	textDidResize(tPtr->view->delegate, tPtr->view);
}

	

Bool
WMScrollText(WMText *tPtr, int amount)
{
	Bool scroll=False;
	if (!tPtr)
		return False;
	if (amount == 0 || !tPtr->view->flags.realized)
		return False;
	
	if (amount < 0) {
		if (tPtr->vpos > 0) {
			if (tPtr->vpos > amount) tPtr->vpos += amount;
			else tPtr->vpos=0;
			scroll=True;
	} } else {
		int limit = tPtr->docHeight - tPtr->visible.h;
		if (tPtr->vpos < limit) {
			if (tPtr->vpos < limit-amount) tPtr->vpos += amount;
			else tPtr->vpos = limit;
            scroll = True;
    } }   

	if (scroll && tPtr->vpos != tPtr->prevVpos) {
		updateScrollers(tPtr);
		paintText(tPtr);
    }   
	tPtr->prevVpos = tPtr->vpos;
    return scroll;
}   

Bool 
WMPageText(WMText *tPtr, Bool direction)
{
	if (!tPtr) return False;
	if (!tPtr->view->flags.realized) return False;

	return WMScrollText(tPtr, direction?tPtr->visible.h:-tPtr->visible.h);
}

void
WMSetTextEditable(WMText *tPtr, Bool editable)
{
	if (!tPtr)
		return;
	tPtr->flags.editable = editable;
}
	
int 
WMGetTextEditable(WMText *tPtr)
{
	if (!tPtr)
		return 0;
	return tPtr->flags.editable;
}

void
WMSetTextIgnoresNewline(WMText *tPtr, Bool ignore)
{
	if (!tPtr)
		return;
	tPtr->flags.ignoreNewLine = ignore;
}

Bool
WMGetTextIgnoresNewline(WMText *tPtr)
{
	if (!tPtr)
		return True;
	return tPtr->flags.ignoreNewLine;
}

void
WMSetTextUsesMonoFont(WMText *tPtr, Bool mono)
{
	if (!tPtr)
		return;
	if (mono && tPtr->flags.rulerShown)
		;//WMShowTextRuler(tPtr, False);

	tPtr->flags.monoFont = mono;
	WMRefreshText(tPtr, tPtr->vpos, tPtr->hpos);
}

Bool
WMGetTextUsesMonoFont(WMText *tPtr)
{
	if (!tPtr)
		return True;
	return tPtr->flags.monoFont;
}


void
WMSetTextDefaultFont(WMText *tPtr, WMFont *font)
{
	if (!tPtr)
		return;
        
	if (font)
		tPtr->dFont = font;
	else
		tPtr->dFont = WMRetainFont(tPtr->view->screen->normalFont);
}

WMFont *
WMGetTextDefaultFont(WMText *tPtr)
{
	if (!tPtr)
		return NULL;
	else
		return tPtr->dFont;
}

void
WMSetTextAlignment(WMText *tPtr, WMAlignment alignment)
{
	if (!tPtr) 
		return;
	tPtr->flags.alignment = alignment;
	WMRefreshText(tPtr, tPtr->vpos, tPtr->hpos);
}

void
WMSetTextParser(WMText *tPtr, WMAction *parser)
{
	if (!tPtr) 
		return;
	tPtr->parser = parser;
}

void
WMSetTextWriter(WMText *tPtr, WMAction *writer)
{
	if (!tPtr)
		return;
	tPtr->writer = writer;
}

int 
WMGetTextInsertType(WMText *tPtr)
{
	if (!tPtr)
		return 0;
	return tPtr->flags.prepend;
}
	

void 
WMSetTextSelectionColor(WMText *tPtr, WMColor *color)
{
	TextBlock *tb;
	if (!tPtr || !color)
		return;
	
	tb = tPtr->firstTextBlock;
	if (!tb || !tPtr->flags.ownsSelection)
		return;
	
	while (tb) { 
		if(tb->color)
    		WMReleaseColor(tb->color);
		tb->color = WMRetainColor(color);
		tb = tb->next;
	}
	WMRefreshText(tPtr, tPtr->vpos, tPtr->hpos);
}

void 
WMSetTextSelectionFont(WMText *tPtr, WMFont *font)
{
	TextBlock *tb;
	if (!tPtr || !font)
		return;
	
	tb = tPtr->firstTextBlock;
	if (!tb || !tPtr->flags.ownsSelection)
		return;
	
	while (tb) { 
		if (!tb->graphic)
			tb->d.font = WMRetainFont(font);
		tb = tb->next;
	}
	WMRefreshText(tPtr, tPtr->vpos, tPtr->hpos);
}

void
WMFreezeText(WMText *tPtr) 
{
	if (!tPtr)
		return;

	tPtr->flags.frozen = True;
}

void
WMThawText(WMText *tPtr) 
{
	if (!tPtr)
		return;

	tPtr->flags.frozen = False;
}


Bool 
WMFindInTextStream(WMText *tPtr, char *needle)
{
	char *haystack = NULL;
	Bool result;

	if (!tPtr || !needle)
		return False;

	if ( !(haystack = WMGetTextStream(tPtr)))
		return False;

	result = (Bool) strstr(haystack, needle);
	wfree(haystack);
	return result;
}
	
	