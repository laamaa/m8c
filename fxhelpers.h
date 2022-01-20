#ifndef FXHELPERS_H_
#define FXHELPERS_H_

#include "command.h"
#include "render.h"

int note_to_hex(int note_char);
void update_active_notes_data(struct draw_character_command *command, struct active_notes *active_notes);

#endif