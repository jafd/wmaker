

#include <WINGsP.h>
#include <X11/cursorfont.h>

#include "wtableview.h"

struct W_TableColumn {
    WMTableView *table;
    WMWidget *titleW;
    char *title;
    int width;
    int minWidth;
    int maxWidth;
    
    void *id;

    WMTableColumnDelegate *delegate;

    unsigned resizable:1;
    unsigned editable:1;
};



static void handleResize(W_ViewDelegate *self, WMView *view);


WMTableColumn *WMCreateTableColumn(char *title)
{
    WMTableColumn *col = wmalloc(sizeof(WMTableColumn));
    
    col->table = NULL;
    col->titleW = NULL;
    col->width = 50;
    col->minWidth = 5;
    col->maxWidth = 0;
    
    col->id = NULL;
    
    col->title = wstrdup(title);
    
    col->delegate = NULL;
    
    col->resizable = 1;
    col->editable = 0;
        
    return col;
}


void WMSetTableColumnId(WMTableColumn *column, void *id)
{
    column->id = id;
}


void *WMGetTableColumnId(WMTableColumn *column)
{
    return column->id;
}


void WMSetTableColumnWidth(WMTableColumn *column, unsigned width)
{
    if (column->maxWidth == 0)
	column->width = WMAX(column->minWidth, width);
    else
	column->width = WMAX(column->minWidth, WMIN(column->maxWidth, width));
    
    if (column->table) {
	handleResize(NULL, W_VIEW(column->table));
    }
}


void WMSetTableColumnDelegate(WMTableColumn *column, 
			      WMTableColumnDelegate *delegate)
{
    column->delegate = delegate;
}


void WMSetTableColumnConstraints(WMTableColumn *column, 
				 unsigned minWidth, unsigned maxWidth)
{
    wassertr(minWidth <= maxWidth);
    
    column->minWidth = minWidth;
    column->maxWidth = maxWidth;
    
    if (column->width < column->minWidth)
	WMSetTableColumnWidth(column, column->minWidth);
    else if (column->width > column->maxWidth || column->maxWidth == 0)
	WMSetTableColumnWidth(column, column->maxWidth);
}


void WMSetTableColumnEditable(WMTableColumn *column, Bool flag)
{
    column->editable = flag;
}


WMTableView *WMGetTableColumnTableView(WMTableColumn *column)
{
    return column->table;
}



struct W_TableView {
    W_Class widgetClass;
    WMView *view;

    WMFrame *header;
    
    WMLabel *corner;
    
    WMScrollView *scrollView;
    WMView *tableView;

    WMArray *columns;
    WMArray *splitters;
    
    int tableWidth;
    
    int rows;
    
    GC gridGC;
    WMColor *gridColor;
    
    Cursor splitterCursor;
    
    void *dataSource;

    WMTableViewDelegate *delegate;
    
    unsigned headerHeight;
    
    unsigned rowHeight;
    
    unsigned drawsGrid:1;
};

static W_Class tableClass = 0;


static W_ViewDelegate viewDelegate = {
    NULL,
	NULL,
	handleResize,
	NULL,
	NULL
};





static void handleEvents(XEvent *event, void *data);
static void handleTableEvents(XEvent *event, void *data);


static void scrollObserver(void *self, WMNotification *notif)
{
    WMTableView *table = (WMTableView*)self;
    WMRect rect;
    int i, x;
    
    rect = WMGetScrollViewVisibleRect(table->scrollView);
    
    x = 0;
    for (i = 0; i < WMGetArrayItemCount(table->columns); i++) {
	WMTableColumn *column;
	
	column = WMGetFromArray(table->columns, i);
	
	WMMoveWidget(column->titleW, x + rect.pos.x, 0);
	
	if (i > 0) {
	    WMView *splitter;

	    splitter = WMGetFromArray(table->splitters, i-1);
	    W_MoveView(splitter, x + rect.pos.x - 1, 0);
	}
	
	x += W_VIEW_WIDTH(WMWidgetView(column->titleW)) + 1;
    }
}


static void splitterHandler(XEvent *event, void *data)
{
    WMTableView *table = (WMTableView*)data;
    
    
}



WMTableView *WMCreateTableView(WMWidget *parent)
{
    WMTableView *table = wmalloc(sizeof(WMTableView));
    WMScreen *scr = WMWidgetScreen(parent);
    
    memset(table, 0, sizeof(WMTableView));

    if (!tableClass) {
	tableClass = W_RegisterUserWidget();
    }
    table->widgetClass = tableClass;
    
    table->view = W_CreateView(W_VIEW(parent));
    if (!table->view) 
	goto error;
    table->view->self = table;
        
    table->view->delegate = &viewDelegate;
    
    table->headerHeight = 20;
    
    table->scrollView = WMCreateScrollView(table);
    if (!table->scrollView)
	goto error;
    WMResizeWidget(table->scrollView, 10, 10);
    WMSetScrollViewHasVerticalScroller(table->scrollView, True);
    WMSetScrollViewHasHorizontalScroller(table->scrollView, True);
    {
	WMScroller *scroller;
	scroller = WMGetScrollViewHorizontalScroller(table->scrollView);
	WMAddNotificationObserver(scrollObserver, table,
				  WMScrollerDidScrollNotification,
				  scroller);
    }
    WMMoveWidget(table->scrollView, 1, 2+table->headerHeight);    
    WMMapWidget(table->scrollView);

    table->header = WMCreateFrame(table);
    WMMoveWidget(table->header, 22, 2);
    WMMapWidget(table->header);
    WMSetFrameRelief(table->header, WRFlat);
    table->corner = WMCreateLabel(table);
    WMResizeWidget(table->corner, 20, table->headerHeight);
    WMMoveWidget(table->corner, 2, 2);
    WMMapWidget(table->corner);
    WMSetLabelRelief(table->corner, WRRaised);
    WMSetWidgetBackgroundColor(table->corner, scr->darkGray);
    

    table->tableView = W_CreateView(W_VIEW(parent));
    if (!table->tableView) 
	goto error;
    W_ResizeView(table->tableView, 100, 1000);
    W_MapView(table->tableView);    
    
    WMSetScrollViewContentView(table->scrollView, table->tableView);

    table->tableView->flags.dontCompressExpose = 1;
    
    table->gridColor = WMDarkGrayColor(scr);
    
    {
	XGCValues gcv;
	
	gcv.foreground = WMColorPixel(table->gridColor);
	gcv.dashes = 1;
	gcv.line_style = LineOnOffDash;
	table->gridGC = XCreateGC(WMScreenDisplay(scr), W_DRAWABLE(scr),
				  GCForeground, &gcv);
    }
        
    table->drawsGrid = 1;
    table->rowHeight = 16;

    table->tableWidth = 1;
    
    table->columns = WMCreateArray(4);
    table->splitters = WMCreateArray(4);
    
    table->splitterCursor = XCreateFontCursor(WMScreenDisplay(scr),
					      XC_sb_h_double_arrow);
    
    WMCreateEventHandler(table->view, ExposureMask|StructureNotifyMask,
                         handleEvents, table);

    WMCreateEventHandler(table->tableView, ExposureMask,
                         handleTableEvents, table);
    
    return table;
    
 error:
    if (table->scrollView) 
	WMDestroyWidget(table->scrollView);
    if (table->tableView)
	W_DestroyView(table->tableView);
    if (table->view)
	W_DestroyView(table->view);
    wfree(table);
    return NULL;
}


void WMAddTableViewColumn(WMTableView *table, WMTableColumn *column)
{
    int width;
    int i;
    WMScreen *scr = WMWidgetScreen(table);
    int count;
    
    column->table = table;

    WMAddToArray(table->columns, column);
        
    if (!column->titleW) {
	column->titleW = WMCreateLabel(table);
	WMSetLabelRelief(column->titleW, WRRaised);
	WMSetLabelFont(column->titleW, scr->boldFont);
	WMSetLabelTextColor(column->titleW, scr->white);
	WMSetWidgetBackgroundColor(column->titleW, scr->darkGray);
	WMSetLabelText(column->titleW, column->title);
	W_ReparentView(WMWidgetView(column->titleW),
		       WMWidgetView(table->header), 0, 0);
	if (W_VIEW_REALIZED(table->view))
	    WMRealizeWidget(column->titleW);
	WMMapWidget(column->titleW);
    }
    
    if (WMGetArrayItemCount(table->columns) > 1) {
	WMView *splitter = W_CreateView(WMWidgetView(table->header));

	W_SetViewBackgroundColor(splitter, WMWhiteColor(scr));

	if (W_VIEW_REALIZED(table->view))
	    W_RealizeView(splitter);

	W_ResizeView(splitter, 2, table->headerHeight-1);
	W_MapView(splitter);
	
	W_SetViewCursor(splitter, table->splitterCursor);
	WMCreateEventHandler(splitter, ButtonPressMask|ButtonReleaseMask
			     |PointerMotionMask, splitterHandler, table);

	WMAddToArray(table->splitters, splitter);
    }

    count = WMGetArrayItemCount(table->columns);
    for (i = 0, width = 0; i < count; i++) {
	WMTableColumn *column = WMGetFromArray(table->columns, i);
	
	WMMoveWidget(column->titleW, width, 0);
	WMResizeWidget(column->titleW, column->width-1, table->headerHeight);
	
	if (i > 0) {
	    WMView *splitter = WMGetFromArray(table->splitters, i-1);

	    W_MoveView(splitter, width-1, 0);
	}
	width += column->width;
    }
        
    wassertr(table->delegate && table->delegate->numberOfRows);
    
    table->rows = table->delegate->numberOfRows(table->delegate, table);

    W_ResizeView(table->tableView, width+1,
		 table->rows * table->rowHeight + 1);
    
    table->tableWidth = width + 1;
}


void WMSetTableViewHeaderHeight(WMTableView *table, unsigned height)
{
    table->headerHeight = height;
    
    handleResize(NULL, table->view);
}


void WMSetTableViewDelegate(WMTableView *table, WMTableViewDelegate *delegate)
{
    table->delegate = delegate;
}


WMView *WMGetTableViewDocumentView(WMTableView *table)
{
    return table->tableView;
}


void *WMTableViewDataForCell(WMTableView *table, WMTableColumn *column, 
			     int row)
{
    return (*table->delegate->valueForCell)(table->delegate, column, row);
}


WMRect WMTableViewRectForCell(WMTableView *table, WMTableColumn *column, 
			      int row)
{
    WMRect rect;
    int i;
    
    rect.pos.x = 0;
    rect.pos.y = row * table->rowHeight;
    rect.size.height = table->rowHeight;
    
    for (i = 0; i < WMGetArrayItemCount(table->columns); i++) {
	WMTableColumn *col;
	col = WMGetFromArray(table->columns, i);
	
	if (col == column) {
	    rect.size.width = col->width;
	    break;
	}
	
	rect.pos.x += col->width;
    }
    return rect;
}


void WMSetTableViewDataSource(WMTableView *table, void *source)
{
    table->dataSource = source;
}


void *WMGetTableViewDataSource(WMTableView *table)
{
    return table->dataSource;
}


void WMSetTableViewBackgroundColor(WMTableView *table, WMColor *color)
{
    W_SetViewBackgroundColor(table->tableView, color);
}


void WMSetTableViewGridColor(WMTableView *table, WMColor *color)
{
    WMReleaseColor(table->gridColor);
    table->gridColor = WMRetainColor(color);
    XSetForeground(WMScreenDisplay(WMWidgetScreen(table)), table->gridGC, WMColorPixel(color));
}


static void drawGrid(WMTableView *table, WMRect rect)
{
    WMScreen *scr = WMWidgetScreen(table);
    Display *dpy = WMScreenDisplay(scr);
    int i;
    int y1, y2;
    int x1, x2;
    int xx;
    Drawable d = W_VIEW_DRAWABLE(table->tableView);
    GC gc = table->gridGC;
    
#if 0
    char dashl[1] = {1};
    
    XSetDashes(dpy, gc, 0, dashl, 1);
    
    y1 = (rect.pos.y/table->rowHeight - 1)*table->rowHeight;
    y2 = y1 + (rect.size.height/table->rowHeight+2)*table->rowHeight;
#endif
    y1 = 0;
    y2 = W_VIEW_HEIGHT(table->tableView);

    xx = 0;
    for (i = 0; i < WMGetArrayItemCount(table->columns); i++) {
	WMTableColumn *column;
	
	if (xx >= rect.pos.x && xx <= rect.pos.x+rect.size.width) {
	    XDrawLine(dpy, d, gc, xx, y1, xx, y2);
	}
	column = WMGetFromArray(table->columns, i);
	xx += column->width;
    }
    XDrawLine(dpy, d, gc, xx, y1, xx, y2);
    
    
    x1 = rect.pos.x;
    x2 = WMIN(x1 + rect.size.width, xx);
    
    if (x2 <= x1)
	return;
#if 0
    XSetDashes(dpy, gc, (rect.pos.x&1), dashl, 1);
#endif
    
    y1 = rect.pos.y - rect.pos.y%table->rowHeight;
    y2 = y1 + rect.size.height + table->rowHeight;
    
    for (i = y1; i <= y2; i += table->rowHeight) {
	XDrawLine(dpy, d, gc, x1, i, x2, i);
    }    
}


static WMRange columnsInRect(WMTableView *table, WMRect rect)
{
    WMTableColumn *column;
    int width;
    int i , j;
    int totalColumns = WMGetArrayItemCount(table->columns);
    WMRange range;
    
    j = 0;
    width = 0;
    for (i = 0; i < totalColumns; i++) {
	column = WMGetFromArray(table->columns, i);
	if (j == 0) {
	    if (width <= rect.pos.x && width + column->width > rect.pos.x) {
		range.position = i;
		j = 1;
	    }
	} else {
	    if (width > rect.pos.x + rect.size.width) {
		range.count = i - range.position;
		break;
	    }
	}
	width += column->width;
    }
    
    range.count = WMAX(1, WMIN(range.count, totalColumns - range.position));

    return range;
}


static WMRange rowsInRect(WMTableView *table, WMRect rect)
{
    WMRange range;
    int rh = table->rowHeight;
    
    range.position = WMAX(0, rect.pos.y / rh - 1);
    range.count = WMAX(1, WMIN(rect.size.height / rh + 3, table->rows));

    return range;
}


static void drawRow(WMTableView *table, int row, WMRect clipRect)
{
    int i;
    WMRange cols = columnsInRect(table, clipRect);
    WMTableColumn *column;
        
    for (i = cols.position; i < cols.position+cols.count; i++) {
	column = WMGetFromArray(table->columns, i);

	wassertr(column->delegate && column->delegate->drawCell);
	
	(*column->delegate->drawCell)(column->delegate, column, row);
    }
}


static void repaintTable(WMTableView *table, int x, int y, 
			 int width, int height)
{
    WMRect rect;
    WMRange rows;
    int i;
    
    wassertr(table->delegate && table->delegate->numberOfRows);
    i = (*table->delegate->numberOfRows)(table->delegate, table);

    if (i != table->rows) {
	table->rows = i;
	W_ResizeView(table->tableView, table->tableWidth, 
		     table->rows * table->rowHeight + 1);   
    }
    
    
    rect.pos = wmkpoint(x,y);
    rect.size = wmksize(width, height);

    if (table->drawsGrid) {
	drawGrid(table, rect);
    }
    
    rows = rowsInRect(table, rect);
    for (i = rows.position; 
	 i < WMIN(rows.position+rows.count, table->rows); 
	 i++) {
	drawRow(table, i, rect);
    }
}


static void handleTableEvents(XEvent *event, void *data)
{
    WMTableView *table = (WMTableView*)data;
    
    switch (event->type) {
     case Expose:
	repaintTable(table, event->xexpose.x, event->xexpose.y,
		     event->xexpose.width, event->xexpose.height);
	break;
    }    
}


static void handleEvents(XEvent *event, void *data)
{
    WMTableView *table = (WMTableView*)data;
    WMScreen *scr = WMWidgetScreen(table);
    
    switch (event->type) {
     case Expose:
	W_DrawRelief(scr, W_VIEW_DRAWABLE(table->view), 0, 0, 
		     W_VIEW_WIDTH(table->view), W_VIEW_HEIGHT(table->view),
		     WRSunken);
	break;
    }
}


static void handleResize(W_ViewDelegate *self, WMView *view)
{
    int width;
    int height;
    WMTableView *table = view->self;

    width = W_VIEW_WIDTH(view) - 2;
    height = W_VIEW_HEIGHT(view) - 3;
    
    height -= table->headerHeight; /* table header */

    if (table->corner)
	WMResizeWidget(table->corner, 20, table->headerHeight);
    
    if (table->scrollView) {
	WMMoveWidget(table->scrollView, 1, table->headerHeight + 2);
	WMResizeWidget(table->scrollView, width, height);
    }
    if (table->header)
	WMResizeWidget(table->header, width - 21, table->headerHeight);
}

