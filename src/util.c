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

#include <stdio.h>

#include "util.h"


Color HSVToColor(Vector3 hsv, unsigned char alpha){
    Color color = ColorFromHSV(hsv.x, hsv.y, hsv.z);
    color.a = alpha;
    return color;
}

void setFromRGBA(color_t *color, Color rgba){
    color->rgba = rgba;
    color->hsv = ColorToHSV(rgba);
}

void setFromHSV(color_t *color, Vector3 hsv){
    color->hsv = hsv;
    color->rgba = HSVToColor(hsv, color->rgba.a);
}

void toggleTool(enum CURSOR_MODE *cursor, enum CURSOR_MODE tool){
    if (*cursor != tool) *cursor = tool;
    else *cursor = CURSOR_DEFAULT;
}

void setWindowTitleToPath(const char *image_path){
    char title[300];
    sprintf(title, "%s - Image maker for angry programmers", image_path);
    SetWindowTitle(title);
}
