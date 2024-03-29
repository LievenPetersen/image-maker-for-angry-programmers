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
#include "external/deque.h"

#include "canvas.h"

static Texture2D loadImageAsTexture(Image *image);
static bool setTextureToImage(Texture2D *texture, Image *image);
static void ImageResizeCanvasOwn(Image *image, int newWidth, int newHeight, int offsetX, int offsetY, Color fill);
static void imageColorFlood(Image *image, Vector2 source_pixel, Color new_color);

#define MAX_UNDO_STEPS 10000

typedef struct pixel_t {
    Vector2 pos;
    Color color;
} pixel_t;

typedef struct delta_t{
    union {
        pixel_t pixel;
        Image image;
    };
}delta_t;

typedef enum DIFF_TYPE {
    INVALID_DIFF = 0,
    PIXEL_DIFF,
    IMAGE_DIFF,
} DIFF_TYPE;

typedef struct diff_t {
    DIFF_TYPE type;
    size_t action_id;
    delta_t before;
    delta_t after;
} diff_t;

typedef DEQ(diff_t) diff_deq_t;

typedef struct recorder_t {
    diff_deq_t undo_queue;
    diff_deq_t redo_queue;
} recorder_t;

typedef enum DIRECTION{
    DIRECTION_REVERSE = 0,
    DIRECTION_FORWARD = 1,
}DIRECTION;

static void recorder_pop_tail(diff_deq_t *deque){
    diff_t diff = deq_poll(*deque);
    switch(diff.type){
        case IMAGE_DIFF: {
            UnloadImage(diff.before.image);
            UnloadImage(diff.after.image);
        } break;
        // no free required:
        case INVALID_DIFF:
        case PIXEL_DIFF: break;
    }
}

static void recorder_record(recorder_t *rec, diff_t diff){
    if (deq_size(rec->undo_queue) >= MAX_UNDO_STEPS) recorder_pop_tail(&rec->undo_queue);
    deq_push(rec->undo_queue, diff);
    while(deq_size(rec->redo_queue) > 0) recorder_pop_tail(&rec->redo_queue);
}

static diff_t __recorder_wind(diff_deq_t *from, diff_deq_t *to){
    if (deq_size(*from) > 0){
        diff_t diff = deq_pop(*from);
        deq_push(*to, diff);
        return diff;
    } else {
        return (diff_t){.type=INVALID_DIFF};
    }
}

static diff_t recorder_wind(recorder_t *rec, DIRECTION dir){
    return dir == DIRECTION_REVERSE? __recorder_wind(&rec->undo_queue, &rec->redo_queue)
        : __recorder_wind(&rec->redo_queue, &rec->undo_queue);
}

static void recorder_free(recorder_t *rec){
    while(recorder_wind(rec, DIRECTION_FORWARD).type != INVALID_DIFF); // push redos into undo queue
    while(deq_size(rec->undo_queue) > 0) recorder_pop_tail(&rec->undo_queue);
    deq_free(rec->undo_queue);
    deq_free(rec->redo_queue);
}

struct canvas_t{
    Image buffer;
    Vector2 size;
    Texture2D texture;
    recorder_t rec;
    diff_deq_t draw_queue;
    size_t action_counter;
};

// --- API ---

canvas_t *canvas_new(Image content){
    canvas_t *new = calloc(1, sizeof(*new));
    new->buffer = ImageCopy(content);
    new->texture = loadImageAsTexture(&content);
    new->size = (Vector2){content.width, content.height};
    return new;
}

void canvas_free(canvas_t *canvas){
    UnloadTexture(canvas->texture);
    UnloadImage(canvas->buffer);
    deq_free(canvas->draw_queue);
    recorder_free(&canvas->rec);
    free(canvas);
}


// functions interacting directly with canvas buffer and texture

// -- modifying function

static void __canvas_queue_diff(canvas_t *canvas, diff_t diff, DIRECTION dir){
    if (diff.type == INVALID_DIFF) return;

    if (dir == DIRECTION_REVERSE){
        delta_t temp = diff.before;
        diff.before = diff.after;
        diff.after = temp;
    }
    deq_push(canvas->draw_queue, diff);

    // modify buffer
    delta_t delta = diff.after;
    switch (diff.type){
        case PIXEL_DIFF:{
            Color color = delta.pixel.color;
            ImageDrawPixel(&canvas->buffer, delta.pixel.pos.x, delta.pixel.pos.y, color);
        } break;
        case IMAGE_DIFF:{
            canvas->buffer = ImageCopy(delta.image); // TODO: properly free the initial buffer on exit / when it is no longer in the undo queue.
            canvas->size.x = canvas->buffer.width;
            canvas->size.y = canvas->buffer.height;
        } break;
        case INVALID_DIFF: /*what the hell man (unreachable)*/ break;
    }
}

void canvas_setPixel(canvas_t *canvas, Vector2 pixel, Color color){
    Color old_color = GetImageColor(canvas->buffer, pixel.x, pixel.y);
    diff_t diff = {.type=PIXEL_DIFF, .before.pixel={pixel, old_color}, .after.pixel={pixel, color}, .action_id=canvas->action_counter};
    recorder_record(&canvas->rec, diff);
    __canvas_queue_diff(canvas, diff, DIRECTION_FORWARD);
}

void canvas_setToImage(canvas_t *canvas, Image image){
    // FIXME: using canvas->buffer might have unwanted implications on free.
    diff_t diff = {.type=IMAGE_DIFF, .before.image=canvas->buffer, .after.image=ImageCopy(image), .action_id=0}; // TODO: make images part of action_counter
    recorder_record(&canvas->rec, diff);
    __canvas_queue_diff(canvas, diff, DIRECTION_FORWARD);
}

// start of a drawing action that groups the pixels of following canvas calls.
void canvas_nextPixelStroke(canvas_t *canvas){
    canvas->action_counter++;
    if (canvas->action_counter == 0) canvas->action_counter += 1;
}

// return true if the size of the canvas changed
static bool canvas_retrace(canvas_t *canvas, DIRECTION dir){
    bool size_changed = false;
    diff_t diff = recorder_wind(&canvas->rec, dir);
    size_t action_id = diff.action_id;
    do {
        switch(diff.type){
            case INVALID_DIFF: printf("reached the end of recorded changes\n");break; // TODO: present in UI
            case PIXEL_DIFF: __canvas_queue_diff(canvas, diff, dir); break;
            case IMAGE_DIFF: {
                __canvas_queue_diff(canvas, diff, dir);
                size_changed |= diff.before.image.width != diff.after.image.width || diff.before.image.height != diff.after.image.height;
            } break;
        }
        diff = recorder_wind(&canvas->rec, dir);
        if (diff.type != INVALID_DIFF && (action_id == 0 || diff.action_id != action_id)){
            recorder_wind(&canvas->rec, !dir);
            break;
        }
    } while(action_id != 0 && action_id == diff.action_id && diff.type != INVALID_DIFF);
    return size_changed;
}

// -- query functions

inline Image canvas_getContent(canvas_t *canvas){
    return ImageCopy(canvas->buffer); // could avoid this copy by redoing the logic of all tool functions.
}

inline Color canvas_getPixel(canvas_t *canvas, Vector2 pixel){
    return GetImageColor(canvas->buffer, pixel.x, pixel.y);
}

// this function has the side effect of evaluating and applying any queued modifications to the texture.
Texture2D canvas_nextFrame(canvas_t *canvas){
    while(deq_size(canvas->draw_queue) > 0){
        diff_t diff = deq_poll(canvas->draw_queue);
        switch(diff.type){
            case IMAGE_DIFF: {
                Image image = diff.after.image;
                if (IsImageReady(image)){
                    setTextureToImage(&canvas->texture, &image);
                    canvas->size.x = image.width;
                    canvas->size.y = image.height;
                }
            } break;
            case PIXEL_DIFF: {
                pixel_t pixel = diff.after.pixel;
                Rectangle rect = {pixel.pos.x, pixel.pos.y, 1, 1};
                UpdateTextureRec(canvas->texture, rect, &pixel.color);
            } break;
            case INVALID_DIFF: break;
        }
    }
    return canvas->texture;
}

bool canvas_saveAsImage(canvas_t *canvas, const char *path){
    if (!ExportImage(canvas->buffer, path)){
        perror("Error while saving image!\n");
        return false;
    }
    return true;
}

inline Vector2 canvas_getSize(canvas_t *canvas){
    return canvas->size;
}

// functions indirectly interacting with canvas struct

// return true if the size of the canvas changed
bool canvas_undo(canvas_t *canvas){
    printf("undo\n");
    return canvas_retrace(canvas, DIRECTION_REVERSE);
}

// return true if the size of the canvas changed
bool canvas_redo(canvas_t *canvas){
    printf("redo\n");
    return canvas_retrace(canvas, DIRECTION_FORWARD);
}

void canvas_blendPixel(canvas_t *canvas, Vector2 pixel, Color color){
    Color new_color = color;
    if (color.a < 255){
        Color old_color = canvas_getPixel(canvas, pixel); // TODO: cache image!
        new_color = ColorAlphaBlend(old_color, color, WHITE);
    }
    canvas_setPixel(canvas, pixel, new_color);
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


// -- utility functions --

// changes image and texture to PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
static Texture2D loadImageAsTexture(Image *image){
    ImageFormat(image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    Texture2D buffer = LoadTextureFromImage(*image);
    assert(buffer.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    return buffer;
}

// replaces the existing texture and unloads it. Returns true on success.
static bool setTextureToImage(Texture2D *texture, Image *image){
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
static void ImageResizeCanvasOwn(Image *image, int newWidth, int newHeight, int offsetX, int offsetY, Color fill){  // Resize canvas and fill with color
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

static void imageColorFlood(Image *image, Vector2 source_pixel, Color new_color){
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
