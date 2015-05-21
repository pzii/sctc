#include "state.h"
#include "helper.h"

//\cond
#include <assert.h>
#include <stdlib.h>
//\endcond

#define MAX_LISTS 16

struct track_list_state {
	struct track_list *list;
	size_t selected;
	size_t position;
} lists[MAX_LISTS];

static size_t       _current_list = 0;        ///< the index in lists of the currently displayed list (default: 0)
static enum repeat  _repeat       = rep_none; ///< the repeat state, one out of (none, one, all)
static char        *_title_text   = NULL;
static char        *_status_text  = NULL;
static enum color   _status_color;
static size_t       _current_time = 0;

static char        *_tb_title = NULL;
static char        *_tb_text  = NULL;
static size_t       _tb_pos   = 0;
static size_t       _tb_old_pos   = 0;

static char        *_input = NULL;
struct command     *_commands = NULL;

size_t             state_get_current_list()  { return _current_list; }
struct track_list* state_get_list(size_t id) { assert(id < MAX_LISTS); return lists[id].list; }
enum   repeat      state_get_repeat()        { return _repeat; }
char*              state_get_title_text()    { return _title_text; }
char*              state_get_status_text()   { return _status_text; }
enum color         state_get_status_color()  { return _status_color; }
char*              state_get_tb_text()       { return _tb_text; }
char*              state_get_tb_title()      { return _tb_title; }
size_t             state_get_tb_pos()        { return _tb_pos; }
char*              state_get_input()         { return _input; }
struct command*    state_get_commands()      { return _commands; }
size_t             state_get_current_time()  { return _current_time; }

void state_set_commands(struct command *commands) { _commands = commands; }
void state_set_current_list(size_t list)          { _current_list = list;   }

void state_set_lists(struct track_list **_lists) {
	for(size_t i = 0; _lists[i] && i < MAX_LISTS; i++) {
		lists[i].list     = _lists[i];
		lists[i].selected = 0;
		lists[i].position = 0;
	}
}
void state_set_repeat(enum repeat repeat)       { _repeat       = repeat; }
void state_set_title(char *text)                { _title_text   = text;   }
void state_set_current_time(size_t time)        { _current_time = time;   }

void state_set_tb_pos(size_t pos) {
	_tb_old_pos = _tb_pos;
	_tb_pos = pos;
}

void state_set_status(char *text, enum color color) {
	_status_text  = text;
	_status_color = color;
}

void state_set_tb(char *title, char *text) {
	_tb_title = title;
	_tb_text  = text;
}

bool state_init() {
	_input = lcalloc(128, sizeof(char));

	return _input;
}

bool state_finalize() {
	free(_input);

	return true;
}