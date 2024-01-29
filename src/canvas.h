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

#include "external/raylib/src/raylib.h"

// all fields are readonly
typedef struct canvas_t canvas_t;

void canvas_setPixel(canvas_t *canvas, Vector2 pixel, Color color);
void canvas_blendPixel(canvas_t *canvas, Vector2 pixel, Color color);
void canvas_setToImage(canvas_t *canvas, Image image);
void canvas_colorFlood(canvas_t *canvas, Vector2 source, Color flood);

void canvas_resize(canvas_t *canvas, Vector2 new_size, Color fill);
// factor > 1 increases resolution, factor < 1 decreases resolution.
void canvas_changeResolution(canvas_t *canvas, float factor);

// do not modify or unload the returned texture. it is still owned by the canvas!
Texture2D canvas_nextFrame(canvas_t *canvas);

Image canvas_getContent(canvas_t *canvas);
Vector2 canvas_getSize(canvas_t *canvas);
Color canvas_getPixel(canvas_t *canvas, Vector2 pixel);

void canvas_saveAsImage(canvas_t *canvas, const char *path);

canvas_t *canvas_new(Image content);
void canvas_free(canvas_t *canvas);
