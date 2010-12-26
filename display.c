#include "pcb.h"

static void
hide_item_layer(PcbItem *item, int layer)
{
	if (item->canvas_item[layer]) {
		goo_canvas_item_remove(item->canvas_item[layer]);
		item->canvas_item[layer] = NULL;
	}
}

void
hide_item(PcbItem *item)
{
	int	i;

	for (i = 0; i < pcb.layers; i++)
		hide_item_layer(item, i);
}

static void
show_point(PcbItem *item, int layer)
{
#define rad 3.0
#define siz 3.5
	if (item->flags & PCB_POI)
		item->canvas_item[layer] =
		    goo_canvas_rect_new(pcb.layer[layer].olay,
			item->p.x - siz, item->p.y - siz, siz * 2, siz * 2,
			"stroke-color", "black", "fill-color",
			item->flags & PCB_SELECTED ? "red" : "yellow", NULL);
	else if (item->flags & PCB_BEND)
		item->canvas_item[layer] =
		    goo_canvas_ellipse_new(pcb.layer[layer].olay,
			item->p.x, item->p.y, 2, 2,
			"stroke-color", "black", "fill-color",
			item->flags & PCB_SELECTED ? "green" : "black", NULL);
	else
		item->canvas_item[layer] =
		    goo_canvas_ellipse_new(pcb.layer[layer].olay,
			item->p.x, item->p.y, rad, rad,
			"stroke-color", "black", "fill-color",
			item->flags & PCB_SELECTED ? "green" : "white", NULL);
#undef rad
#undef siz
}

static void
show_line(PcbItem *item, int layer)
{
	item->canvas_item[layer] = goo_canvas_polyline_new_line(pcb.layer[layer].olay,
	    item->l.point[0]->p.x, item->l.point[0]->p.y,
	    item->l.point[1]->p.x, item->l.point[1]->p.y,
	    "stroke-color", item->flags & PCB_SELECTED ? "green" : "black",
	    NULL);
}

static void
show_item_layer(PcbItem *item, int layer)
{
	if (!pcb.layer->olay)
		return;
	hide_item_layer(item, layer);
	if (!(item->layers & LAYER(layer)))
		return;
	switch (item->type) {
	case PCB_POINT:
		show_point(item, layer);
		break;
	case PCB_LINE:
		show_line(item, layer);
		break;
	}
}

void
show_item(PcbItem *item)
{
	int	i;

	for (i = 0; i < pcb.layers; i++)
		show_item_layer(item, i);
}

void
undisplay(void)
{
	gtk_container_remove(GTK_CONTAINER(pcb.scrolled),
	    GTK_WIDGET(pcb.layer[pcb.curlayer].canvas));
}

void
redisplay(void)
{
	gtk_container_add(GTK_CONTAINER(pcb.scrolled),
	    GTK_WIDGET(pcb.layer[pcb.curlayer].canvas));
	gtk_widget_show_all(pcb.window);
}

void
show_overlays(int layer)
{
	PcbItem *cur;

	g_print("show overlays on layer %d\n", layer);
	pcb.layer[layer].olay = goo_canvas_group_new(pcb.layer[layer].root,
	    "x", 0.0, "y", 0.0, "width", pcb.width, "height", pcb.height, NULL);
	for (cur = pcb.items; cur; cur = cur->next)
		show_item_layer(cur, layer);
}

void
hide_overlays(int layer)
{
	PcbItem *cur;

	g_print("hide overlays on layer %d\n", layer);
	if (pcb.layer[layer].olay) {
		goo_canvas_item_remove(pcb.layer[layer].olay);
		pcb.layer[layer].olay = NULL;
	}
	for (cur = pcb.items; cur; cur = cur->next)
		cur->canvas_item[layer] = NULL;
}

void
init_display()
{
	int	i;

	pcb.scale = 1.0;
	for (i = 0; i < pcb.layers; i++) {
		pcb.layer[i].canvas = GOO_CANVAS(goo_canvas_new());
		gtk_object_ref(GTK_OBJECT(pcb.layer[i].canvas));
		goo_canvas_set_bounds(pcb.layer[i].canvas,
		    0, 0, pcb.width, pcb.height);
		pcb.layer[i].root =
		    goo_canvas_get_root_item(pcb.layer[i].canvas);
		goo_canvas_image_new(pcb.layer[i].root, pcb.layer[i].img, 0, 0,
		    NULL);
		init_layer_events(pcb.layer[i].root);
		show_overlays(i);
	}

	pcb.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(GTK_WINDOW(pcb.window), 800, 600);
	gtk_window_set_title(GTK_WINDOW(pcb.window), pcb.filename);

	init_window_events(pcb.window);

	pcb.scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(pcb.scrolled);
	gtk_container_add(GTK_CONTAINER(pcb.window), pcb.scrolled);

	redisplay();
}
