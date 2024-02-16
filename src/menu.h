/*
 *  zlib license:
 *
 *  Copyright (c) 2024 Lieven Petersen
 *
 *  This software is provided 'as-is', without any express or implied
 *  warranty. In no event will the authors be held liable for any damages
 *  arising from the use of this software.
 *
 *  Permission is granted to anyone to use this software for any purpose,
 *  including commercial applications, and to alter it and redistribute it
 *  freely, subject to the following restrictions:
 *
 *  1. The origin of this software must not be misrepresented; you must not
 *  claim that you wrote the original software. If you use this software
 *  in a product, an acknowledgment in the product documentation would be
 *  appreciated but is not required.
 *  2. Altered source versions must be plainly marked as such, and must not be
 *  misrepresented as being the original software.
 *  3. This notice may not be removed or altered from any source distribution.
 */

#ifndef __MENU_H
#define __MENU_H

#include "external/raylib/src/raylib.h"


#include "canvas.h"
#include "util.h"

#define FAV_COLOR ((Color){0x18, 0x18, 0x18, 0xFF}) // sorry, but AA is a bit impractical
#define STD_COLOR ((Color){0xFF, 0x00, 0x00, 0xFF})
#define DFT_COLOR ((Color){0xFF, 0xD7, 0x00, 0xFF})

#ifndef DEFAULT_FONT_SIZE
#define DEFAULT_FONT_SIZE 30
#endif //DEFAULT_FONT_SIZE

// DIM_STRLEN-1 digits can be entered when resizing the canvas
#define DIM_STRLEN 4

#define MAX_FILENAME_SIZE 200


typedef struct shared_state_t{
    color_t active_color;
    canvas_t *canvas;
    enum CURSOR_MODE cursor;
    bool forceImageResize;
    bool forceMenuResize;
    bool forceWindowResize;
    bool showGrid;
}shared_state_t;

typedef struct menu_state_t{
    // TODO: make field + isEditing abstraction
    char *hex_field, *x_field, *y_field, *filename, *filename_old;
    bool isEditingHexField, isEditingFileName, isEditingXField, isEditingYField;
    int font_size;
    #ifndef DISABLE_CUSTOM_FONT
    Font *fonts;
    Font font;
    #endif
}menu_state_t;

menu_state_t initMenu(char *image_name);
void drawMenu(shared_state_t *s, menu_state_t *mf, Rectangle menu_rect);
void unloadMenu(menu_state_t *ms);

// utils
#define MIN(a, b) (a<b? (a) : (b))
#define MAX(a, b) (a>b? (a) : (b))

#endif // __MENU_H
