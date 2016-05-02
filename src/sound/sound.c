#include "assets/sound/sound.h"
#include <stdlib.h>

void sound_delete(struct sound* snd) {
    free(snd->data);
    free(snd);
}
