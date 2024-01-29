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

#include <assert.h>
#include <stdbool.h>

#include "external/raylib/src/raylib.h"

#include "external/stack.h"

#include "canvas.h"

typedef struct pixel_t {
    Vector2 pos;
    Color color;
} pixel_t;


struct canvas_t{
    Image buffer;
    Vector2 size;
    Texture2D texture;
    pixel_t *draw_queue;
    bool replace;
};

bool saveTextureAsImage(Texture2D tex, const char *path){
    Image result = LoadImageFromTexture(tex);

    bool success = IsImageReady(result)
        && ExportImage(result, path);

    UnloadImage(result);

    if (!success){
        printf("Error while saving image!\n");
    }
    return success;
}

// changes image and texture to PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
Texture2D loadImageAsTexture(Image *image){
    ImageFormat(image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    Texture2D buffer = LoadTextureFromImage(*image);
    assert(buffer.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    return buffer;
}

// replaces the existing texture and unloads it. Returns true on success.
bool setTextureToImage(Texture2D *texture, Image *image){
    Texture2D new_buffer = loadImageAsTexture(image);
    if (!IsTextureReady(new_buffer)){
        UnloadTexture(new_buffer);
        // TODO: fail operation
        return false;
    } else {
        UnloadTexture(*texture);
        *texture = new_buffer;
        return true;
    }
}

// fill color for resize is not implemented yet in raylib, so do it here.
void ImageResizeCanvasOwn(Image *image, int newWidth, int newHeight, int offsetX, int offsetY, Color fill){  // Resize canvas and fill with color
    int oldWidth = image->width;
    int oldHeight = image->height;
    ImageResizeCanvas(image, newWidth, newHeight, offsetX, offsetY, fill);

    // Fill color for ImageResizeCanvas was not implemented until RAYLIB_VERSION "5.1-dev", pull request #3720.
    // Thus prior versions need to fill the canvas manually.
    if (RAYLIB_VERSION_MAJOR < 5 || (RAYLIB_VERSION_MAJOR == 5 && RAYLIB_VERSION_MINOR < 1) || strcmp(RAYLIB_VERSION, "5.1-dev")){
        if (offsetY > 0){
            ImageDrawRectangle(image, 0, 0, newWidth, offsetY, fill);
        }
        if (offsetY + oldHeight < newHeight){
            ImageDrawRectangle(image, 0, offsetY + oldHeight, newWidth, newHeight - offsetY - oldHeight, fill);
        }
        if (offsetX > 0){
            ImageDrawRectangle(image, 0, offsetY, offsetY, oldHeight, fill);
        }
        if (offsetX + oldWidth < newWidth){
            ImageDrawRectangle(image, offsetX + oldWidth, offsetY, newWidth - offsetX - oldWidth, oldHeight, fill);
        }
    }
}

void imageColorFlood(Image *image, Vector2 source_pixel, Color new_color){
    Color old_color = GetImageColor(*image, source_pixel.x, source_pixel.y);

    Color *pixels = (Color*)image->data;
    size_t pixel_count = image->height * image->width;
    size_t *stack = NULL;
    stack_reserve_capacity(stack, pixel_count);
    bool *visited = malloc(pixel_count * sizeof(*visited));
    if (visited == NULL){
        perror("unable to allocate sufficient memory\n");
        stack_free(stack);
        free(visited);
        return;
    }

    memset(visited, 0, pixel_count * sizeof(*visited));

    size_t source_pixel_idx = (int)source_pixel.x + (int)source_pixel.y * image->width;
    stack_push(stack, source_pixel_idx);

    while(stack_size(stack) > 0){
        size_t pixel_idx = stack_pop(stack);
        pixels[pixel_idx] = new_color;

        if (pixel_idx >= 1 && pixel_idx % image->width > 0){ // is not wrapping left to the previous row.
            size_t west = pixel_idx - 1;
            if (memcmp(&pixels[west], &old_color, sizeof(old_color)) == 0){
                if (!visited[west]){
                    stack_push(stack, west);
                    visited[west] = true;
                }
            }
        }
        if (pixel_idx < pixel_count - 1 && (pixel_idx % image->width) + 1 < (size_t)image->width){ // is not wrapping right to the next row.
            size_t east = pixel_idx + 1;
            if (memcmp(&pixels[east], &old_color, sizeof(old_color)) == 0){
                if (!visited[east]){
                    stack_push(stack, east);
                    visited[east] = true;
                }
            }
        }
        if (pixel_idx < pixel_count - image->width){
            size_t south = pixel_idx + image->width;
            if (memcmp(&pixels[south], &old_color, sizeof(old_color)) == 0){
                if (!visited[south]){
                    stack_push(stack, south);
                    visited[south] = true;
                }
            }
        }
        if (pixel_idx >= (size_t)image->width){
            size_t north = pixel_idx - image->width;
            if (memcmp(&pixels[north], &old_color, sizeof(old_color)) == 0){
                if (!visited[north]){
                    stack_push(stack, north);
                    visited[north] = true;
                }
            }
        }
    }

    stack_free(stack);
    free(visited);
}


void canvas_setToImage(canvas_t *canvas, Image image){
    UnloadImage(canvas->buffer);
    canvas->buffer = ImageCopy(image);
    canvas->replace = true;
    canvas->size.x = canvas->buffer.width;
    canvas->size.y = canvas->buffer.height;
    // empty the draw queue, as it is overwritten by this
    while(stack_size(canvas->draw_queue) > 0) (void)stack_pop(canvas->draw_queue);
}

Texture2D canvas_nextFrame(canvas_t *canvas){
    if (canvas->replace){
        setTextureToImage(&canvas->texture, &canvas->buffer);
        // this should already have happened, but just to make sure
        canvas->size.x = canvas->buffer.width;
        canvas->size.y = canvas->buffer.height;
        canvas->replace = false;
    }
    while(stack_size(canvas->draw_queue) > 0){
        pixel_t pixel = stack_pop(canvas->draw_queue);
        Rectangle rect = {pixel.pos.x, pixel.pos.y, 1, 1};
        UpdateTextureRec(canvas->texture, rect, &pixel.color);
    }
    return canvas->texture;
}

canvas_t *canvas_new(Image content){
    canvas_t *new = calloc(1, sizeof(*new));
    stack_reserve_capacity(new->draw_queue, 3);
    canvas_setToImage(new, content);
    return new;
}

void canvas_blendPixel(canvas_t *canvas, Vector2 pixel, Color color){
    Color new_color = color;
    if (color.a < 255){
        Color old_color = canvas_getPixel(canvas, pixel); // TODO: cache image!
        new_color = ColorAlphaBlend(old_color, color, WHITE);
    }
    canvas_setPixel(canvas, pixel, new_color);
}

void canvas_setPixel(canvas_t *canvas, Vector2 pixel, Color color){
    ImageDrawPixel(&canvas->buffer, pixel.x, pixel.y, color);
    stack_push(canvas->draw_queue, (pixel_t){pixel, color});
}

inline Image canvas_getContent(canvas_t *canvas){
    return ImageCopy(canvas->buffer); // could avoid this copy by redoing the logic of all tool functions.
}

void canvas_saveAsImage(canvas_t *canvas, const char *path){
    saveTextureAsImage(canvas->texture, path);
}

inline Vector2 canvas_getSize(canvas_t *canvas){
    return canvas->size;
}

inline Color canvas_getPixel(canvas_t *canvas, Vector2 pixel){
    return GetImageColor(canvas->buffer, pixel.x, pixel.y);
}

void canvas_resize(canvas_t *canvas, Vector2 new_size, Color fill){
    Image image = canvas_getContent(canvas);
    ImageResizeCanvasOwn(&image, new_size.x, new_size.y, 0, 0, fill);
    canvas_setToImage(canvas, image);
    UnloadImage(image);
}

// factor > 1 increases resolution, factor < 1 decreases resolution.
void canvas_changeResolution(canvas_t *canvas, float factor){
    Image image = canvas_getContent(canvas);
    ImageResizeNN(&image, factor*image.width, factor*image.height); // seems to just use top left corner when down-scaling 0.5x to determine color??
    canvas_setToImage(canvas, image);
    UnloadImage(image);
}

void canvas_colorFlood(canvas_t *canvas, Vector2 source, Color flood){
    Image image = canvas_getContent(canvas);
    imageColorFlood(&image, source, flood);
    canvas_setToImage(canvas, image);
    UnloadImage(image);
}

void canvas_free(canvas_t *canvas){
    UnloadTexture(canvas->texture);
    stack_free(canvas->draw_queue);
    free(canvas);
}
