#include <string.h>
#include <gdk/gdkkeysyms.h>
#include "pcb.h"

static void
undo(int dummy)
{
	while (pcb.action->prev) {
		play_action(PCB_UNDO);
		pcb.action = pcb.action->prev;
		if (pcb.action->act & PCB_MARKER)
			break;
	}
	pcb.flags |= PCB_DIRTY;
}

static void
redo(int dummy)
{
	if (pcb.action->next) do {
		pcb.action = pcb.action->next;
		play_action(PCB_DO);
	} while (pcb.action->next && !(pcb.action->act & PCB_MARKER));
	pcb.flags |= PCB_DIRTY;
}

#define init_transaction()	do { pcb.new_action = pcb.action; } while (0)

static void
commit_transaction(void)
{
	if (pcb.action != pcb.new_action) {
		pcb.new_action->act |= PCB_MARKER;
		redo(0);
	}
}

static void
new_action(void)
{
	PcbAction	*cur;

	while (pcb.new_action->next) {
		cur = pcb.new_action->next;
		pcb.new_action->next = cur->next;
		g_free(cur);
	}
	pcb.new_action->next = g_malloc0(sizeof(PcbAction));
	pcb.new_action->next->prev = pcb.new_action;
	pcb.new_action = pcb.new_action->next;
}

static void
new_item_action(int act, PcbItem *item)
{
	new_action();
	pcb.new_action->act = act | item->type;
	pcb.new_action->layers = item->layers;
	switch (item->type) {
	case PCB_POINT:
		pcb.new_action->c = item->p;
		break;
	case PCB_LINE:
		pcb.new_action->c = item->l.point[0]->p;
		pcb.new_action->l = item->l.point[1]->p;
		break;
	}
}

#define remove_item(item)	new_item_action(PCB_REMOVE, item)

#define change_layers(item, layers)	do {		\
	new_item_action(PCB_CHANGE_LAYERS, item);	\
	pcb.new_action->newlayers = layers;		\
} while (0)

/* layers is 0 for layers where lines should be removed */
static void
remove_connected_lines(PcbItem *point, unsigned long long layers)
{
	PcbItem *cur;

	for (cur = point->next; cur; cur = cur->next) {
		if (cur->type == PCB_LINE && !(cur->layers & layers) &&
		    !(cur->flags & PCB_MARKED) &&
		    (cur->l.point[0] == point || cur->l.point[1] == point)) {
			cur->flags |= PCB_MARKED;
			remove_item(cur);
		}
	}
}

static void
modify_layers_selected(unsigned long long layers)
{
	PcbItem			*cur;

	init_transaction();
	for (cur = pcb.items; cur; cur = cur->next) {
		if (cur->flags & PCB_SELECTED) {
			if (cur->type == PCB_POINT)
				remove_connected_lines(cur, layers);
			if (layers)
				change_layers(cur, layers);
			else
				remove_item(cur);
		}
	}
	commit_transaction();
}

static void
change_layers_selected(int dummy)
{
	unsigned long long	layers;

	if (pcb.mode != PCB_SELECT)
		return;
	if (pcb.marked_layer == -1) {
		pcb.marked_layer = pcb.curlayer;
		return;
	}
	layers = LAYERS_BETWEEN(LAYER(pcb.marked_layer), CUR_LAYER());
	g_print("change layers to [%llx]\n", layers);
	pcb.marked_layer = -1;
	modify_layers_selected(layers);
}

static void
delete_selected(int dummy)
{
	if (pcb.mode == PCB_SELECT)
		modify_layers_selected(0);
}

static void
toggle_poi_selected(int dummy)
{
	PcbItem	*cur;

	init_transaction();
	if (pcb.mode != PCB_SELECT)
		return;
	for (cur = pcb.items; cur; cur = cur->next) {
		if ((cur->flags & PCB_SELECTED) && cur->type == PCB_POINT) {
			new_item_action(PCB_TOGGLE_FLAGS, cur);
			pcb.new_action->flags = PCB_POI;
		}
	}
	commit_transaction();
}

PcbItem *
find_point(PcbCoordinate c, unsigned long long layers)
{
	PcbItem *cur;

	for (cur = pcb.items; cur; cur = cur->next) {
		if (cur->type == PCB_POINT && (cur->layers & layers) &&
		    ceq(cur->p, c))
			break;
	}
	return cur;
}

static void
select_item(PcbItem *item)
{
	if (!item)
		return;
	if ((item->flags ^= PCB_SELECTED) & PCB_SELECTED)
		pcb.selected++;
	else
		pcb.selected--;
	show_item(item);
}

static void
deselect_all(int dummy)
{
	PcbItem	*cur;

	if (pcb.selected) {
		for (cur = pcb.items; cur; cur = cur->next) {
			if (cur->flags & PCB_SELECTED)
				select_item(cur);
		}
		pcb.selected = 0;
	}
	pcb.marked_layer = -1;
}

static void
add_point(PcbCoordinate c, unsigned long long layers)
{
	PcbItem	*item;

	if ((item = find_point(c, layers))) {
		g_print("conflicting point found, not adding\n");
		deselect_all(0);
		select_item(item);
		return;
	}
	init_transaction();
	new_action();
	pcb.new_action->act = PCB_ADD | PCB_POINT;
	pcb.new_action->layers = layers;
	pcb.new_action->c = c;
	commit_transaction();
}

#define sq(n)		((n) * (n))
#define sqdist(a, b)	(sq((a).x - (b).x) + sq((a).y - (b).y))

/* currently only points */
static PcbItem *
find_closest_item(PcbCoordinate c)
{
	PcbItem	*best = NULL;
	PcbItem	*cur;
	gdouble	bestdist = 256.0;	/* square of distance (avoid sqrt()) */
	gdouble curdist;

	for (cur = pcb.items; cur; cur = cur->next) {
		switch (cur->type) {
		case PCB_POINT:
			curdist = sqdist(cur->p, c);
			break;
		default:
			continue;
		}
		if (curdist < 16.0 && curdist < bestdist) {
			best = cur;
			bestdist = curdist;
		}
	}
	return best;
}
#define find_closest_point find_closest_item

static void
select_connected(PcbItem *item)
{
	PcbItem	*cur;

	if (!item)
		return;
	if (item->flags & PCB_SELECTED)
		return;
	select_item(item);

	switch (item->type) {
	case PCB_POINT:
		for (cur = pcb.items; cur; cur = cur->next) {
			if (cur->flags & PCB_SELECTED || cur->type != PCB_LINE)
				continue;
			if (cur->l.point[0] == item) {
				select_item(cur);
				select_connected(cur->l.point[1]);
			} else if (cur->l.point[1] == item) {
				select_item(cur);
				select_connected(cur->l.point[0]);
			}
		}
		break;
	case PCB_LINE:
		select_connected(item->l.point[0]);
		select_connected(item->l.point[1]);
		break;
	}
}

static PcbItem *
find_line(PcbItem *points[])
{
	PcbItem	*cur;

	for (cur = pcb.items; cur; cur = cur->next) {
		if (cur->type == PCB_LINE && (cur->layers & CUR_LAYER()) &&
		    (!memcmp(cur->l.point, points, sizeof(*points) * 2) ||
		     (cur->l.point[0] == points[1] &&
		      cur->l.point[1] == points[0])))
			break;
	}
	return cur;
}

static void
try_autolimit(PcbItem *point)
{
	PcbItem	*line = NULL;
	PcbItem	*cur;

	g_print("try autolimit\n");
	for (cur = pcb.items; cur; cur = cur->next) {
		if (cur->type == PCB_LINE &&
		    (cur->l.point[0] == point || cur->l.point[1] == point)) {
			if (line)
				return;
			g_print("found\n");
			line = cur;
		}
	}
	if (line)
		change_layers(point, LAYERS_BETWEEN(line->layers, CUR_LAYER()));
}

static void
toggle_line(PcbItem *points[])
{
	init_transaction();
	new_action();
	pcb.new_action->layers = CUR_LAYER();
	pcb.new_action->c = points[0]->p;
	pcb.new_action->l = points[1]->p;
	if (find_line(points))
		pcb.new_action->act = PCB_REMOVE | PCB_LINE;
	else {
		pcb.new_action->act = PCB_ADD | PCB_LINE;
		if (pcb.flags & PCB_AUTOLIMIT) {
			try_autolimit(points[0]);
			try_autolimit(points[1]);
		}
	}
	commit_transaction();
}

static void
try_interconnect(void)
{
	PcbItem	*points[2];
	PcbItem	*cur;
	int	found = 0;

	if (pcb.selected != 2)
		return;
	for (cur = pcb.items; cur && found < 2; cur = cur->next) {
		if (cur->flags & PCB_SELECTED)
			points[found++] = cur;
	}
	if (found != 2)
		return;
	toggle_line(points);
	select_item(points[0]);
	select_item(points[1]);
}

inline void
change_mode(int mode)
{
	static char	*desc[] = {
	       	"select",
	       	"add point",
	       	"add via",
	       	"toggle line",
	       	"examine",
	};

	deselect_all(0);
	pcb.mode = mode;
	g_print("%s mode\n", desc[mode]);
}

#define BOTTOM	0
#define DOWN	1
#define UP	2
#define TOP	3

static void
switch_layer(int direction)
{
	int	newlayer = pcb.curlayer;

	switch (direction) {
	case UP:
		if (++newlayer == pcb.layers) {
			g_print("this is the top layer!\n");
			return;
		}
		break;
	case DOWN:
		if (!newlayer--) {
			g_print("this is the bottom layer!\n");
			return;
		}
		break;
	case TOP:
		newlayer = pcb.layers - 1;
		break;
	case BOTTOM:
		newlayer = 0;
		break;
	}
	if (pcb.mode == PCB_ADD_LINE)
		deselect_all(0);
	undisplay();
	pcb.curlayer = newlayer;
	redisplay();
	g_print("layer %d of %d\n", newlayer, pcb.layers);
}

#define ZOOM_IN		0
#define ZOOM_OUT	1
#define ZOOM_NORMAL	2

static void
zoom(int dir)
{
	static char	*desc[] = { "in", "out", "to normal" };
	int		i;

	switch (dir) {
	case ZOOM_IN:
		pcb.scale *= 2.0;
		break;
	case ZOOM_OUT:
		pcb.scale /= 2.0;
		break;
	case ZOOM_NORMAL:
		pcb.scale = 1.0;
		break;
	}
	for (i = 0; i < pcb.layers; i++)
		goo_canvas_set_scale(pcb.layer[i].canvas, pcb.scale);
	g_print("zoom %s: scale %f\n", desc[dir], pcb.scale);
	undisplay();
	redisplay();
}

static void
toggle_overlays(int dummy)
{
	if (pcb.layer[pcb.curlayer].olay)
		hide_overlays(pcb.curlayer);
	else
		show_overlays(pcb.curlayer);
}

static void
toggle_autolimit(int dummy)
{
	pcb.flags ^= PCB_AUTOLIMIT;
	g_print("autolimit %s\n", pcb.flags & PCB_AUTOLIMIT ? "on" : "off");
}

static void
quit(int force)
{
	if (!force && (pcb.flags & PCB_DIRTY)) {
		g_warning("dirty, not quitting ('w' to save, 'Q' to quit)\n");
		return;
	}
	gtk_main_quit();
}

typedef struct {
	int	keyval;
	void	(*func)(int);
	int	param;
	char	*desc;
} PcbKeyBinding;

static void print_help(int);

PcbKeyBinding	key_binding[] = {
	{ GDK_a,	toggle_autolimit,	0,
	  "a\tToggle autolimiting of vias to layers between two wires\n" },
	{ GDK_c,	change_layers_selected,	0,
	  "c\tIn select mode, change layers of selected items\n" },
	{ GDK_d,	delete_selected,	0,
	  "d\tIn select mode, delete selected items\n" },
	{ GDK_e,	change_mode,		PCB_EXAMINE,
	  "e\tEnter examine mode: click point to show connections\n" },
	{ GDK_h,	print_help,		0,
	  "h\tPrint this help\n" },
	{ GDK_i,	toggle_poi_selected,	0,
	  "i\tIn select mode, toggle point-of-interest on selected items\n" },
	{ GDK_l,	change_mode,		PCB_ADD_LINE,
	  "l\tEnter toggle line mode: click two points to add/remove wire\n" },
	{ GDK_p,	change_mode,		PCB_ADD_POINT,
	  "p\tEnter add point mode: click to add a point on current layer\n" },
	{ GDK_q,	quit,			0,
	  "q\tQuit, unless project is modified\n" },
	{ GDK_Q,	quit,			1,
	  "Q\tQuit, even if project is modified\n" },
	{ GDK_r,	redo,			0,
	  "r\tRedo\n" },
	{ GDK_s,	change_mode,		PCB_SELECT,
	  "s\tEnter select mode: click points to select\n" },
	{ GDK_u,	undo,			0,
	  "u\tUndo\n" },
	{ GDK_v,	change_mode,		PCB_ADD_VIA,
	  "v\tEnter add via mode: click to add a point on all layers\n" },
	{ GDK_w,	save_project,		0,
	  "w\tWrite (save) project\n" },
	{ GDK_x,	toggle_overlays,	0,
	  "x\tShow/hide overlays (you can't do much with overlays hidden)\n" },
	{ GDK_plus,	zoom,			ZOOM_IN,
	  "+\tZoom in\n" },
	{ GDK_minus,	zoom,			ZOOM_OUT,
	  "-\tZoom out\n" },
	{ GDK_0,	zoom,			ZOOM_NORMAL,
	  "0\tZoom to 100%\n" },
	{ GDK_Up,	switch_layer,		UP,
	  "Up\tGo up a layer\n" },
	{ GDK_Down,	switch_layer,		DOWN,
	  "Down\tGo down a layer\n" },
	{ GDK_Home,	switch_layer,		TOP,
	  "Home\tGo to first layer\n" },
	{ GDK_End,	switch_layer,		BOTTOM,
	  "End\tGo to last layer\n" },
	{ GDK_Escape,	deselect_all,		0,
	  "Esc\tDeselect all\n" },
	{ 0,		NULL,			0,
	  NULL },
};

static void
print_help(int dummy)
{
	PcbKeyBinding	*cur;

	g_print("\n\nKeyboard:\n");
	for (cur = key_binding; cur->desc; cur++)
		g_print("%s", cur->desc);
	g_print("\n\n");
}

static gboolean
key_press(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	PcbKeyBinding	*cur;

//	g_print("event->state %x, event->keyval %x\n",
//	    event->state, event->keyval);
//	if (!(event->state & ~(GDK_SHIFT_MASK | GDK_LOCK_MASK))) {
		for (cur = key_binding; cur->keyval; cur++)
			if (cur->keyval == event->keyval) {
				(*cur->func)(cur->param);
				break;
			}
//	}
	return TRUE;
}

static gboolean
btn_press(GooCanvasItem *item, GooCanvasItem *target,
    GdkEventButton *event, gpointer data)
{
	PcbCoordinate	c = { event->x, event->y };

	g_print("button %d @ %f %f\n", event->button, event->x, event->y);
	if (!pcb.layer[pcb.curlayer].olay)
		return TRUE;
	switch (event->button) {
	case 1:
		switch (pcb.mode) {
		case PCB_SELECT:
			select_item(find_closest_item(c));
			break;
		case PCB_ADD_POINT:
			add_point(c, CUR_LAYER());
			break;
		case PCB_ADD_VIA:
			add_point(c, ALL_LAYERS());
			break;
		case PCB_ADD_LINE:
			select_item(find_closest_point(c));
			try_interconnect();
			break;
		case PCB_EXAMINE:
			select_connected(find_closest_item(c));
			break;
		}
		break;
	}
	return TRUE;
}

static gboolean
delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	/* If you return FALSE in the "delete-event" signal handler,
	 * GTK will emit the "destroy" signal. Returning TRUE means
	 * you don't want the window to be destroyed.
	 * This is useful for popping up 'are you sure you want to quit?'
	 * type dialogs. */

	g_print("delete event occurred\n");

	/* Change TRUE to FALSE and the main window will be destroyed with
	 * a "delete-event". */

	return FALSE;
}

static void
destroy(GtkWidget *widget, gpointer data)
{
	g_print("destroy\n");
	gtk_main_quit();
}

void
init_layer_events(GooCanvasItem *w)
{
	g_signal_connect(w, "button_press_event", G_CALLBACK(btn_press), NULL);
}

void
init_window_events(GtkWidget *w)
{
	g_signal_connect(w, "key_press_event", G_CALLBACK(key_press), NULL);
	g_signal_connect(w, "delete-event", G_CALLBACK(delete_event), NULL);
	g_signal_connect(w, "destroy", G_CALLBACK(destroy), NULL);
}
