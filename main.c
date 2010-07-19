#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <err.h>
#include "pcb.h"

Pcb	pcb;	/* One for now */

static PcbItem *
find_action_item(PcbItem ***cur, int unchange)
{
	PcbItem	**stor;

	if (!cur)
		cur = &stor;
	for (*cur = &pcb.items; **cur; *cur = &(**cur)->next) {
		if ((**cur)->type != (pcb.action->act & PCB_ITEM) ||
		    (**cur)->layers !=
		    (unchange ? pcb.action->newlayers : pcb.action->layers))
			continue;
		switch ((**cur)->type) {
		case PCB_POINT:
			if (ceq((**cur)->p, pcb.action->c))
				return **cur;
			break;
		case PCB_LINE:
			if ((ceq((**cur)->l.point[0]->p, pcb.action->c) &&
			     ceq((**cur)->l.point[1]->p, pcb.action->l)) ||
			    (ceq((**cur)->l.point[0]->p, pcb.action->l) &&
			     ceq((**cur)->l.point[1]->p, pcb.action->c)))
				return **cur;
			break;
		}
	}
	return **cur;
}

static int
play_add(void)
{
	PcbItem	**cur;

	if (find_action_item(&cur, 0)) {
		switch (pcb.action->act & PCB_ITEM) {
		case PCB_POINT:
			g_warning("invalid add point: %f %f\n",
			    pcb.action->c.x, pcb.action->c.y);
			break;
		case PCB_LINE:
			g_warning("invalid add line: %f %f %f %f\n",
			    pcb.action->c.x, pcb.action->c.y,
			    pcb.action->l.x, pcb.action->l.y);
			break;
		}
		return 0;
	}
	*cur = new_pcb_item(pcb);
	(*cur)->type = pcb.action->act & PCB_ITEM;
	(*cur)->layers = pcb.action->layers;
	(*cur)->flags = pcb.action->flags;
	switch ((*cur)->type) {
	case PCB_POINT:
		(*cur)->p = pcb.action->c;
		g_print("add point [%llx] (%f %f)\n", (*cur)->layers,
		    (*cur)->p.x, (*cur)->p.y);
		break;
	case PCB_LINE:
		if (((*cur)->l.point[0] =
		    find_point(pcb.action->c, pcb.action->layers)) &&
		    ((*cur)->l.point[1] =
		    find_point(pcb.action->l, pcb.action->layers))) {
			g_print("add line [%llx] (%f %f) (%f %f)\n",
			    (*cur)->layers,
			    (*cur)->l.point[0]->p.x, (*cur)->l.point[0]->p.y,
			    (*cur)->l.point[1]->p.x, (*cur)->l.point[1]->p.y);
			break;
		}
		g_warning("invalid line: %f %f %f %f\n", pcb.action->c.x,
		    pcb.action->c.y, pcb.action->l.x, pcb.action->l.y);
		/* FAlLTHRU */
	default:
		g_free(*cur);
		*cur = NULL;
		return 0;
	}
	show_item(*cur);
	return 1;
}

static int
play_remove(void)
{
	PcbItem	**cur;
	PcbItem *this;

	if (!find_action_item(&cur, 0)) {
		switch (pcb.action->act & PCB_ITEM) {
		case PCB_POINT:
			g_warning("invalid remove point: %f %f\n",
			    pcb.action->c.x, pcb.action->c.y);
			break;
		case PCB_LINE:
			g_warning("invalid remove line: %f %f %f %f\n",
			    pcb.action->c.x, pcb.action->c.y,
			    pcb.action->l.x, pcb.action->l.y);
			break;
		}
		return 0;
	}

	switch ((*cur)->type) {
	case PCB_POINT:
		g_print("remove point [%llx] (%f %f)\n", (*cur)->layers,
		    (*cur)->p.x, (*cur)->p.y);
		break;
	case PCB_LINE:
		g_print("remove line [%llx] (%f %f) (%f %f)\n", (*cur)->layers,
		    (*cur)->l.point[0]->p.x, (*cur)->l.point[0]->p.y,
		    (*cur)->l.point[1]->p.x, (*cur)->l.point[1]->p.y);
		break;
	}
	this = *cur;
	(*cur) = this->next;
	hide_item(this);
	pcb.action->layers = this->layers;
	pcb.action->flags = this->flags & PCB_STORED_FLAGS;
	g_free(this);
	return 1;
}

static int
play_toggle_flags()
{
	PcbItem	**cur;

	if ((pcb.action->act & PCB_ITEM) != PCB_POINT) {
		g_warning("invalid toggle: wrong item type\n");
		return 0;
	}
	if (!find_action_item(&cur, 0)) {
		g_warning("invalid toggle: %f %f\n",
		    pcb.action->c.x, pcb.action->c.y);
		return 0;
	}
	(*cur)->flags ^= pcb.action->flags;
	g_print("toggle flags %x [%llx] (%f %f)\n", pcb.action->flags,
	    (*cur)->layers, (*cur)->p.x, (*cur)->p.y);
	show_item(*cur);
	return 1;
}

static int
play_change_layers(int unchange)
{
	PcbItem	**cur;

	if ((pcb.action->act & PCB_ITEM) != PCB_POINT) {
		g_warning("invalid change layers: wrong item type\n");
		return 0;
	}
	if (!find_action_item(&cur, unchange)) {
		g_warning("invalid toggle layers: %f %f\n",
		    pcb.action->c.x, pcb.action->c.y);
		return 0;
	}
	(*cur)->layers = unchange ? pcb.action->layers : pcb.action->newlayers;
	g_print("change layers to [%llx] (%f %f)\n", (*cur)->layers,
	    (*cur)->p.x, (*cur)->p.y);
	show_item(*cur);
	return 1;
}

int
play_action(int flag)
{
	switch ((pcb.action->act & PCB_ACTION) ^ flag) {
	case PCB_ADD:
		return play_add();
	case PCB_REMOVE:
		return play_remove();
	case PCB_TOGGLE_FLAGS:
	case PCB_UNTOGGLE_FLAGS:
		return play_toggle_flags();
	case PCB_CHANGE_LAYERS:
	case PCB_UNCHANGE_LAYERS:
		return play_change_layers(flag);
	}
	return 0;
}

void
save_project(int dummy)
{
	FILE	*f;
	PcbItem	*item;
	int	i;

	if (!(f = fopen(pcb.filename, "w"))) {
		g_warning("Can't open file: %s: %s\n", pcb.filename,
		    strerror(errno));
		return;
	}
	fprintf(f, "depcb-project-0\nlayers %d\ncurlayer %d\nsize %.1f %.1f\n",
	    pcb.layers, pcb.curlayer, pcb.width, pcb.height);
	for (i = 0; i < pcb.layers; i++)
		fprintf(f, "imagefile %s\n", pcb.layer[i].filename);
	for (item = pcb.items; item; item = item->next) {
		switch (item->type) {
		case PCB_POINT:
			fprintf(f, "point %llx %.1f %.1f %d\n",
			    item->layers, item->p.x, item->p.y,
			    item->flags & PCB_SAVED_FLAGS);
			break;
		case PCB_LINE:
			fprintf(f, "line %llx %.1f %.1f %.1f %.1f\n",
			    item->layers,
			    item->l.point[0]->p.x, item->l.point[0]->p.y,
			    item->l.point[1]->p.x, item->l.point[1]->p.y);
			break;
		}
	}
	fclose(f);
	pcb.flags &= ~PCB_DIRTY;
}

static void
tokenize(char *s, char **tok, int *tokens)
{
	*tokens = 0;
	while (*tokens < PCB_MAX_TOKENS) {
		while (*s && isspace(*s))
			s++;
		if (!*s || *tokens == PCB_MAX_TOKENS)
			break;
		tok[(*tokens)++] = s;
		while (*s && !isspace(*s))
			s++;
		if (!*s)
			break;
		*s++ = 0;
	}
}

static int
load_project(char *filename)
{
	char	buf[PATH_MAX];
	char	*tok[PCB_MAX_TOKENS];
	FILE	*f;
	char	*s;
	int	tokens = 0;
	int	state = 0, line = 1, layer = 0;

#define EXPECT(t, s)							\
	if (t != tokens) {						\
		g_error("%s line %d: Expected %d tokens, got %d\n",	\
		   filename, line, t, tokens);				\
		goto Error;						\
	}								\
	if (strcmp(tok[0], s)) {					\
		g_error("%s line %d: Expected %s, got %s\n",		\
		   filename, line, s, tok[0]);				\
		goto Error;						\
	}
#define PARSE_ERROR()							\
	do {								\
		g_error("%s line %d: Parse error\n", filename, line);	\
		goto Error;						\
	} while (0)
#define GET_LAYER(l, s)							\
	l = strtoull(s, NULL, 16);					\
	if (!l || (l & ~ALL_LAYERS()))					\
		PARSE_ERROR();
#define GET_COORD(c, x0, y0)						\
	(c).x = atof(x0);						\
	(c).y = atof(y0);						\
	if ((c).x < 0 || (c).x > pcb.width ||				\
	    (c).y < 0 || (c).y > pcb.height)				\
		PARSE_ERROR();

	pcb.filename = filename;
	if (!(f = fopen(pcb.filename, "r"))) {
		g_error("Can't open file: %s: %s\n", pcb.filename,
		    strerror(errno));
		return -1;
	}
	while (fgets(buf, sizeof(buf), f)) {
		if (state != 0 && state != 4)
			tokenize(buf, tok, &tokens);
		switch (state) {
		case 0:
			if (strcmp(buf, "depcb-project-0\n")) {
				g_error("Not a project file: %s\n%s%s",
				    pcb.filename, "depcb-project-0\n", buf);
				goto Error;
			}
			state++;
			break;
		case 1:
			EXPECT(2, "layers");
			pcb.layers = atoi(tok[1]);
			if (pcb.layers < 1 || pcb.layers > 64)
				PARSE_ERROR();
			pcb.layer = g_malloc0(pcb.layers * sizeof(*pcb.layer));
			pcb.action = g_malloc0(sizeof(PcbAction));
			state++;
			break;
		case 2:
			EXPECT(2, "curlayer");
			pcb.curlayer = atoi(tok[1]);
			if (pcb.curlayer < 0 || pcb.curlayer >= pcb.layers)
				PARSE_ERROR();
			state++;
			break;
		case 3:
			EXPECT(3, "size");
			pcb.width = atof(tok[1]);
			pcb.height = atof(tok[2]);
			state++;
			break;
		case 4:
			if (strncmp(buf, "imagefile ", 10)) {
				g_error("expected imagefile, got %s", buf);
				goto Error;
			}
			s = buf + strlen(buf) - 1;
			if (*s == '\n')
				*s = 0;
			pcb.layer[layer].filename = strdup(buf + 10);
			if (++layer == pcb.layers)
				state++;
			break;
		case 5:
			if (tokens == 5 && !strcmp(tok[0], "point")) {
				pcb.action->act = PCB_ADD | PCB_POINT;
				GET_LAYER(pcb.action->layers, tok[1]);
				GET_COORD(pcb.action->c, tok[2], tok[3]);
				pcb.action->flags = strtol(tok[4], NULL, 16);
				if (!play_action(PCB_DO))
					PARSE_ERROR();
				break;
			}
		       	if (tokens == 6 && !strcmp(tok[0], "line")) {
				pcb.action->act = PCB_ADD | PCB_LINE;
				GET_LAYER(pcb.action->layers, tok[1]);
				GET_COORD(pcb.action->c, tok[2], tok[3]);
				GET_COORD(pcb.action->l, tok[4], tok[5]);
				if (!play_action(PCB_DO))
					PARSE_ERROR();
				break;
			}
			bzero(pcb.action, sizeof(*pcb.action));
			state++;
			/* FALLTHRU */
		case 6:
			g_error("garbage at eof");
			break;
		}
		line++;
	}
	return 0;
Error:
	fclose(f);
	return -1;
}

static int
new_project(int argc, char *argv[])
{
	int	i;

	pcb.layers = argc - 2;
	pcb.layer = g_malloc0(pcb.layers * sizeof(*pcb.layer));
	pcb.action = g_malloc0(sizeof(PcbAction));
	pcb.filename = argv[1];
	for (i = 0; i < pcb.layers; i++)
		pcb.layer[i].filename = argv[i + 2];
	return 0;
}

static void
init_layers(int new)
{
	GError	*error = NULL;
	int	i;

	pcb.marked_layer = -1;
	for (i = 0; i < pcb.layers; i++) {
		g_print("loading %s\n", pcb.layer[i].filename);
		if (!(pcb.layer[i].img =
		    gdk_pixbuf_new_from_file(pcb.layer[i].filename, &error))) {
			errx(1, "Can't open file %s: %s",
			    pcb.layer[i].filename, error->message);
		}
		if (new) {
			int	cur;

			cur = gdk_pixbuf_get_width(pcb.layer[i].img);
			pcb.width = MAX(pcb.width, cur);
			cur = gdk_pixbuf_get_height(pcb.layer[i].img);
			pcb.height = MAX(pcb.height, cur);
		}
	}
}

#define PROGNAME	"DePCB (working title) 0.0pl0"
static void
pcb_init(int argc, char *argv[])
{
	int	new = 0;

	switch (argc) {
	case 1:
		errx(1, PROGNAME "\n\nUsage:\n"
		    "\t%s <projfile>.pcb                 - load project\n"
		    "\t%s <projfile>.pcb <imagefile>...  - start a new project",
		    argv[0], argv[0]);
		/* NOTREACHED */
	case 2:
		load_project(argv[1]);
		break;
	default:
		if (argc > 66)
			errx(1, "More than 64 layers?  "
			    "You've got to be kidding me\n");
		new = 1;
		new_project(argc, argv);
		break;
	}
	g_print("springloaded\n");
	init_layers(new);
	init_display();
}

int
main(int argc, char *argv[])
{
	gtk_init(&argc, &argv);
	pcb_init(argc, argv);
	g_print("\n\n" PROGNAME "\nPress \"h\" in main window for help\n\n\n");
	gtk_main();
	g_print("exit 0\n");
	return 0;
}
