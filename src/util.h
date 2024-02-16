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

#ifndef __UTIL_H
#define __UTIL_H

#include "external/raylib/src/raylib.h"

// fields are READONLY
typedef struct color_t{
    Color rgba;  // READONLY
    Vector3 hsv; // READONLY
}color_t;

Color HSVToColor(Vector3 hsv, unsigned char alpha);
void setFromRGBA(color_t *color, Color rgba);
void setFromHSV(color_t *color, Vector3 hsv);

enum CURSOR_MODE{
    CURSOR_DEFAULT,
    CURSOR_PIPETTE,
    CURSOR_COLOR_FILL,
};

void toggleTool(enum CURSOR_MODE *cursor, enum CURSOR_MODE tool);

void setWindowTitleToPath(const char *image_path);

#endif // __UTIL_H
