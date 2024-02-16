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

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "external/raylib/src/raylib.h"
#include "external/raylib/src/raymath.h"

#include "canvas.h"
#include "menu.h"
#include "util.h"

Vector2 RectangleCenter(Rectangle rect){
    return (Vector2){rect.x + rect.width/2.0f, rect.y + rect.height/2.0f};
}

Vector2 Vector2FloorPositive(Vector2 vec){
    // this might be incorrect for negative numbers
    return (Vector2){(float)(int)vec.x, (float)(int)vec.y};
}


int main(int argc, char **argv){

    // draw loading screen
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1000, 800, "Image maker for angry programmers");
    SetExitKey(KEY_NULL); // disable exit on KEY_ESCAPE;
    SetTargetFPS(60);

    BeginDrawing();
    char *loading_text = "loading...";
    int loadingFontSize = DEFAULT_FONT_SIZE;
    DrawText(loading_text, (GetScreenWidth()-MeasureText(loading_text, loadingFontSize))/2, GetScreenHeight()/2, loadingFontSize, WHITE);
    EndDrawing();

    // -- initialize application --

    // generate standard 8x8 image
    Image starter_image = GenImageColor(8, 8, STD_COLOR);
    canvas_t *prep_canvas = canvas_new(starter_image);
    UnloadImage(starter_image);

    bool has_loaded_image = false;

    char filename[MAX_FILENAME_SIZE];

    // load image from provided path
    if (argc == 2){
        Image img = LoadImage(argv[1]);
        if (!IsImageReady(img)){
            printf("Error: failed to load image from '%s'\n", argv[1]);
            UnloadImage(img);
            return 1;
        }
        canvas_setToImage(prep_canvas, img);
        if (strnlen(argv[1], MAX_FILENAME_SIZE) == MAX_FILENAME_SIZE){
            filename[MAX_FILENAME_SIZE-1] = 0; // brutal approach to make string fit.
            // TODO: more graceful solution, that doesn't rip out the postfix.
        }
        sprintf(filename, "%s", argv[1]);
        setWindowTitleToPath(filename);

        has_loaded_image = true;
        UnloadImage(img);
    }

    if (!has_loaded_image){
        sprintf(filename, "out.png");
        has_loaded_image = true;
    }

    menu_state_t menu_state = initMenu(filename);
    menu_state_t *ms = &menu_state;

    shared_state_t state = {
        .active_color = {{0}}, // maybe disable Wmissing-braces to get rid of extra braces?
        .canvas = prep_canvas,
        .cursor = CURSOR_DEFAULT,
        .showGrid = true,
        .forceImageResize = true,
        .forceMenuResize = true,
        .forceWindowResize = true,
    };

    shared_state_t *s = &state;

    bool isMouseDrawing = false;
    Vector2 prev_pixel = {0};

    // this could possibly be encapsulated. Any change in floating scale requires scale to be set as well.
    float _floating_scale = 0; // used to affect scale. To enable small changes a float is needed.
    int scale = 0; // floored floating scale that should be used everywhere.

    Rectangle drawingBounds = {0};
    Rectangle menu_rect = {0};
    Vector2 image_position = {0};

    // Track hsv + alpha instead of rgba,
    // because rgba can only store lossy hue values which leads to color-picker jitters when color approaches white or black.
    setFromRGBA(&s->active_color, DFT_COLOR); // important to initialize from rgba, because HSV does not supply alpha information.

    while(!WindowShouldClose()){
        if (IsWindowResized() || s->forceWindowResize){
            s->forceWindowResize = false;
            s->forceMenuResize = true;
            s->forceImageResize = true;
        }
        if (s->forceMenuResize){
            s->forceMenuResize = false;
            s->forceImageResize = true;

            int menu_width = 20*ms->font_size/3; // allow font size to force a minimum width
            menu_rect = (Rectangle){0, 0, menu_width, GetScreenHeight()};
        }
        if (s->forceImageResize){
            s->forceImageResize = false;
            drawingBounds = (Rectangle){menu_rect.width, 0, GetScreenWidth()-menu_rect.width, GetScreenHeight()};
            // TODO support images bigger than drawingBounds
            // Not sure if this code should stay here. On one hand it is convenient to be able to find the image easily. On the other hand it is jarring to resize and move the image.
            Vector2 canvas_size = canvas_getSize(s->canvas);
            _floating_scale = MAX(1.0f, floor(MIN(drawingBounds.width / canvas_size.x, drawingBounds.height / canvas_size.y)));
            scale = (int)_floating_scale;
            image_position = (Vector2){drawingBounds.width - canvas_size.x*scale, drawingBounds.height - canvas_size.y*scale};
            image_position = Vector2Scale(image_position, 0.5);
            image_position = Vector2Add((Vector2){drawingBounds.x, drawingBounds.y}, image_position);
        }
        // handle mouse and keyboard input
        if (!ms->isEditingFileName){ // name field can overlap with the canvas

            Rectangle image_bounds = {image_position.x, image_position.y, canvas_getSize(s->canvas).x*scale, canvas_getSize(s->canvas).y*scale};
            bool isHoveringMenu = CheckCollisionPointRec(GetMousePosition(), menu_rect);
            bool isHoveringImage = !isHoveringMenu && CheckCollisionPointRec(GetMousePosition(), image_bounds);

            // Detect start of pixel drawing mode
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && isHoveringImage){
                isMouseDrawing = s->cursor == CURSOR_DEFAULT;
                prev_pixel.x = -1; // set to out of bounds, so that any valid pixel is different from it.
                canvas_nextPixelStroke(s->canvas);
            }

            // detect canvas mouse down
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && isHoveringImage){
                Vector2 pixel = Vector2FloorPositive(Vector2Scale(Vector2Subtract(GetMousePosition(), (Vector2){image_bounds.x, image_bounds.y}), 1.0f/(float)scale));
                // set pixel
                if (s->cursor == CURSOR_DEFAULT && isMouseDrawing){
                    if (pixel.x != prev_pixel.x || pixel.y != prev_pixel.y){
                        canvas_blendPixel(s->canvas, pixel, s->active_color.rgba);
                        prev_pixel = pixel;
                    }
                }
                // pick color with pipette (only on press, not continuously)
                if (s->cursor == CURSOR_PIPETTE && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
                    s->cursor = CURSOR_DEFAULT;
                    Color pixel_color = canvas_getPixel(s->canvas, pixel);
                    setFromRGBA(&s->active_color, pixel_color);
                }
                // color fill
                else if (s->cursor == CURSOR_COLOR_FILL && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
                    s->cursor = CURSOR_DEFAULT;
                    canvas_colorFlood(s->canvas, pixel, s->active_color.rgba);
                }
            }
            // shortcuts
            if (!ms->isEditingHexField){
                bool isCtrlDown = (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL));
                bool isShiftDown = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
                // TODO: investigate if ctrl-+ ctrl-- can be detected for all +/- keys on the keyboard for all +/- keys on the keyboard. with glfw (glfwGetKeyName)
                int pressed_key = 0;
                while((pressed_key = GetKeyPressed())){ // a key was pressed this frame (this gets the next key in the queue, but we don't use that.)
                    switch(pressed_key){
                        case KEY_G: s->showGrid = !s->showGrid; break;
                        case KEY_P: toggleTool(&s->cursor, CURSOR_PIPETTE); break;
                        case KEY_F: toggleTool(&s->cursor, CURSOR_COLOR_FILL); break;
                        case KEY_C: if(isCtrlDown) toggleTool(&s->cursor, CURSOR_PIPETTE); break; // still toggle, to conveniently escape the mode without reaching for KEY_ESCAPE.
                        case KEY_S: if(isCtrlDown) canvas_saveAsImage(s->canvas, ms->filename); break;
                        case KEY_R: if(isCtrlDown) if(canvas_redo(s->canvas)) s->forceImageResize = true; break;
                        case KEY_U: if(canvas_undo(s->canvas)) s->forceImageResize = true; break;
                        case KEY_Y: // fallthrough
                        case KEY_Z: if(isCtrlDown) isShiftDown? (canvas_redo(s->canvas)? s->forceImageResize = true:false) : (canvas_undo(s->canvas)? s->forceImageResize=true:false); break;
                        case KEY_ESCAPE: s->cursor = CURSOR_DEFAULT; break;
                        case KEY_HOME: s->forceWindowResize = true; break; // this causes the canvas to be centered again.
                        case KEY_KP_ADD: {ms->font_size += 1; s->forceMenuResize = true;} break;
                        case KEY_KP_SUBTRACT: {ms->font_size -= 1; s->forceMenuResize = true;} break;
                    }
                }

                Vector2 pan = {0};
                if(IsKeyDown(KEY_LEFT))  pan.x += 1;
                if(IsKeyDown(KEY_RIGHT)) pan.x -= 1;
                if(IsKeyDown(KEY_UP))    pan.y += 1;
                if(IsKeyDown(KEY_DOWN))  pan.y -= 1;
                const float key_pan_speed = MIN(GetScreenHeight(), GetScreenWidth()); // scale panning speed with window size.
                image_position = Vector2Add(image_position, Vector2Scale(pan, GetFrameTime()*key_pan_speed));
            }
            // mouse panning
            // FIXME: image_bounds becomes outdated from here on
            if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) || IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)){
                // if this solution proves too flimsy, track the mouse position relative to it's position at the start of the pan.
                image_position = Vector2Add(image_position, GetMouseDelta());
            }
            float mouse_scroll;
            if ((mouse_scroll = GetMouseWheelMove())){
                const float scroll_speed = 1.2; // must be > 1
                float new_scale = MAX(mouse_scroll>0.0f? _floating_scale*scroll_speed : _floating_scale/scroll_speed, 1.0f);

                // Zoom to mouse formula: new_pos = focus + (new_scale/old_scale) (focus - old_pos)
                Vector2 focus = isHoveringMenu? RectangleCenter(drawingBounds) : GetMousePosition();
                Vector2 focus_offset = Vector2Subtract(focus, image_position);
                Vector2 scaled_summand = Vector2Scale(focus_offset, (float)((int)new_scale)/(float)((int)_floating_scale)); // poor man's floorf
                image_position = Vector2Subtract(focus, scaled_summand);

                _floating_scale = new_scale;
                scale = (int)_floating_scale;
            }
            // after all moving functions are done:
            // ensure that part of the canvas is still in the draw area.
            Vector2 image_size = Vector2Scale(canvas_getSize(s->canvas), (int)_floating_scale);
            if (image_position.x + image_size.x < drawingBounds.x + scale) image_position.x = drawingBounds.x - image_size.x + scale;
            if (image_position.y + image_size.y < drawingBounds.y + scale) image_position.y = drawingBounds.y - image_size.y + scale;
            if (drawingBounds.x + drawingBounds.width  < image_position.x + scale) image_position.x = drawingBounds.x + drawingBounds.width - scale;
            if (drawingBounds.y + drawingBounds.height < image_position.y + scale) image_position.y = drawingBounds.y + drawingBounds.height - scale;
        }

        BeginDrawing();
        ClearBackground(FAV_COLOR);

        // draw image
        Vector2 floored_image_position = {(int)image_position.x, (int)image_position.y}; // image_position is not an integer value at this point, which can cause slight distortions when drawing. outright flooring it degrades zoom precision.
        DrawTextureEx(canvas_nextFrame(s->canvas), floored_image_position, 0, scale, WHITE); // use int scale, so that every pixel of the texture is drawn as the same multiple. This is important for drawing the grid.

        // draw grid
        const Color GRID_COLOR = DARKGRAY;
        if(s->showGrid && scale >= 3){
            for (int i = 1; i < canvas_getSize(s->canvas).x; i++){
                DrawLineEx((Vector2){floored_image_position.x + i*scale, floored_image_position.y}, (Vector2){floored_image_position.x + i*scale, floored_image_position.y + canvas_getSize(s->canvas).y*scale}, 1, GRID_COLOR);
            }
            for (int j = 1; j < canvas_getSize(s->canvas).y; j++){
                DrawLineEx((Vector2){floored_image_position.x, floored_image_position.y + j*scale}, (Vector2){floored_image_position.x + canvas_getSize(s->canvas).x*scale, floored_image_position.y + j*scale}, 1, GRID_COLOR);
            }
            DrawRectangleLines(floored_image_position.x-1, floored_image_position.y-1, canvas_getSize(s->canvas).x*scale+2, canvas_getSize(s->canvas).y*scale+2, GRID_COLOR);
        }

        drawMenu(s, ms, menu_rect);

        EndDrawing();
    }

    canvas_free(s->canvas);
    unloadMenu(ms);

    CloseWindow();
}

