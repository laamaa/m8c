#include "fxhelpers.h"

#include <string.h>

#include "command.h"
#include "render.h"

/* Converts the note characters displayed on M8 screen to hex values for use in
 * visualizers */
int note_to_hex(int note_char) {
  switch (note_char) {
  case 45: // - = note off
    return 0x00;
  case 67: // C
    return 0x01;
  case 68: // D
    return 0x03;
  case 69: // E
    return 0x05;
  case 70: // F
    return 0x06;
  case 71: // G
    return 0x08;
  case 65: // A
    return 0x0A;
  case 66: // B
    return 0x0C;
  default:
    return 0x00;
  }
}

// Updates the active notes information array with the current command's data
void update_active_notes_data(struct draw_character_command *command, struct active_notes *active_notes) {

  // Channels are 10 pixels apart, starting from y=70
  int channel_number = command->pos.y / 10 - 7;

  // Note playback information starts from X=288
  if (command->pos.x == 288) {
    active_notes[channel_number].note = note_to_hex(command->c);
  }

  // Sharp notes live at x = 296
  if (command->pos.x == 296 && !strcmp((char *)&command->c, "#"))
    active_notes[channel_number].sharp = 1;
  else
    active_notes[channel_number].sharp = 0;

  // Note octave, x = 304
  if (command->pos.x == 304) {
    int8_t octave = command->c - 48;

    // Octaves A-B
    if (octave == 17 || octave == 18)
      octave -= 7;

    // If octave hasn't been applied to the note yet, do it
    if (octave > 0)
      active_notes[channel_number].octave = octave * 0x0C;
    else
      active_notes[channel_number].octave = 0;
  }
}