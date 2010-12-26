#ifndef __PCB_H
#define __PCB_H

#include <math.h>
#include <gtk/gtk.h>
#include <goocanvas.h>

/* Tokens per line of file */
#define PCB_MAX_TOKENS	8

typedef struct {
	gdouble	x, y;
} PcbCoordinate;

/* type */
#define PCB_EMPTY	0
#define PCB_POINT	1
#define PCB_LINE	2

/* flags - saved */
#define PCB_POI			0x0001
#define PCB_BEND		0x0002
#define PCB_SAVED_FLAGS		0x000f
/* flags - stored in undo list */
#define PCB_SELECTED		0x0010
#define PCB_STORED_FLAGS	0x00ff
/* flags - private */
#define PCB_MARKED		0x0100

typedef struct _PcbItem {
	struct _PcbItem		*next;
	unsigned long long	layers;
	int			type;
	int			flags;
	union {
		PcbCoordinate	p;
		struct {
			struct _PcbItem	*point[2];
		} l;
	};
	GooCanvasItem		*canvas_item[0];
} PcbItem;

#define new_pcb_item(pcb) \
    g_malloc0(sizeof(PcbItem) + (pcb).layers * sizeof(GooCanvasItem *))

#define LAYER(layer)		((unsigned long long)1 << (layer))
#define CUR_LAYER()		LAYER(pcb.curlayer)
#define LAYERS_BELOW(bit)	((bit) - 1)
#define ALL_LAYERS()		LAYERS_BELOW(LAYER(pcb.layers))
#define LAYERS_BETWEEN(a, b)	(LAYERS_BELOW(MAX(a, b) << 1) ^ \
   				 LAYERS_BELOW(MIN(a, b)))

/* action */
#define PCB_ITEM		0x0f
#define PCB_ACTION		0xf0

/* "undo" flag */
#define PCB_DO			0x00
#define PCB_UNDO		0x10
/* actions */
#define PCB_ADD			0x20
#define PCB_REMOVE		0x30
#define PCB_TOGGLE_FLAGS	0x40
#define PCB_UNTOGGLE_FLAGS	0x50
#define PCB_CHANGE_LAYERS	0x60
#define PCB_UNCHANGE_LAYERS	0x70
/* last action in a transaction */
#define PCB_MARKER		0x100

typedef struct _PcbAction {
	struct _PcbAction	*next, *prev;
	int			act;
	int			flags;
	unsigned long long	layers, newlayers;
	PcbCoordinate		c;
	union {
		PcbCoordinate	l;
	};
} PcbAction;

typedef struct {
	GdkPixbuf	*img;
	GooCanvas	*canvas;
	GooCanvasItem	*root, *olay;
	char		*filename;
} PcbLayer;

/* mode */
#define PCB_SELECT	0
#define PCB_ADD_POINT	1
#define PCB_ADD_VIA	2
#define PCB_ADD_LINE	3
#define PCB_EXAMINE	4
#define PCB_TRACE	5

/* flags */
#define PCB_DIRTY	0x1
#define PCB_AUTOLIMIT	0x2

typedef struct {
	GtkWidget	*window, *scrolled;
	PcbLayer	*layer;
	PcbItem		*items;
	PcbAction	*action, *new_action;
	gdouble		width, height, scale;
	int		layers, curlayer, marked_layer;
	int		mode;
	int		flags;
	int		selected;
	PcbCoordinate	c[2];
	int		coords;
	char		*filename;
} Pcb;

extern Pcb	pcb;

PcbItem *	find_point(PcbCoordinate c, unsigned long long layer);
int		play_action(int flag);
void		save_project(int);

void		redisplay(void);
void		undisplay(void);
void		show_item(PcbItem *item);
void		hide_item(PcbItem *item);
void		show_overlays(int layer);
void		hide_overlays(int layer);
void		init_layer_events(GooCanvasItem *w);
void		init_window_events(GtkWidget *w);
void		init_display(void);

/* Coordinate equality (for the purpose of matching/clashing points */
#define ceq(a, b)	\
    (fabs((a).x - (b).x) < 1.5 && (fabs((a).y - (b).y)) < 1.5)

#endif /* __PCB_H */
