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


#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "external/raylib/src/raylib.h"
#include "external/raylib/src/raymath.h"

#define RAYGUI_IMPLEMENTATION
#include "external/raygui.h"

#include "canvas.h"

#define MAX_NAME_FIELD_SIZE 200

#define MIN(a, b) (a<b? (a) : (b))
#define MAX(a, b) (a>b? (a) : (b))

#define FAV_COLOR ((Color){0x18, 0x18, 0x18, 0xFF}) // sorry, but AA is a bit impractical
#define STD_COLOR ((Color){0xFF, 0x00, 0x00, 0xFF})


enum CURSOR_MODE{
    CURSOR_DEFAULT,
    CURSOR_PIPETTE,
    CURSOR_COLOR_FILL,
};

Vector2 RectangleCenter(Rectangle rect){
    return (Vector2){rect.x + rect.width/2.0f, rect.y + rect.height/2.0f};
}

bool isSupportedImageFormat(const char *filePath){
    const char *VALID_EXTENSIONS = ".png;.bmp;.qoi;.raw"; //.tga;.jpg;.jpeg"; // .tga and .jpg don't work for some reason
    return IsFileExtension(filePath, VALID_EXTENSIONS);
}

void setWindowTitleToPath(const char *image_path){
    char title[MAX_NAME_FIELD_SIZE + 100];
    sprintf(title, "%s - Image maker for angry programmers", image_path);
    SetWindowTitle(title);
}

void toggleTool(enum CURSOR_MODE *cursor, enum CURSOR_MODE tool){
    if (*cursor != tool) *cursor = tool;
    else *cursor = CURSOR_DEFAULT;
}


void toolToggleButton(const char *text, enum CURSOR_MODE *cursor, enum CURSOR_MODE tool, int iconId, int y_position, int h_padding, int total_width, int font_size){
    // pipette
    Rectangle pipette_box = {h_padding, y_position, total_width - font_size, font_size};
    if(GuiButton(pipette_box, text)){
        toggleTool(cursor , tool);
    }
    if (*cursor == tool){
        DrawRectangleRec(pipette_box, ColorAlpha(RED, 0.3));
    }
    // TODO: icon should scale with font size.
    GuiDrawIcon(iconId, h_padding + total_width - font_size, y_position, 2, WHITE);
}

// returns > 0  on resize
long dimensionTextBox(const Rectangle bounds, char *str_value, const size_t str_size, long reset_value, bool *isEditing){
    long result = -1;
    if (GuiTextBox(bounds, str_value, str_size, *isEditing)){
        *isEditing = !*isEditing;
        if (!*isEditing){
            char *endptr;
            long value = strtol(str_value, &endptr, 10);
            bool parsed_entirely = *endptr == 0;
            if(parsed_entirely && value > 0){
                result = value;
            } else {
                printf("entered invalid value for x resize!\n");
                sprintf(str_value, "%ld", reset_value);
                // TODO: show error
            }
        }
    }
    return result;
}

Color HSVToColor(Vector3 hsv, unsigned char alpha){
    Color color = ColorFromHSV(hsv.x, hsv.y, hsv.z);
    color.a = alpha;
    return color;
}

// fields are READONLY
typedef struct color_t{
    Color rgba;  // READONLY
    Vector3 hsv; // READONLY
}color_t;

void setFromRGBA(color_t *color, Color rgba){
    color->rgba = rgba;
    color->hsv = ColorToHSV(rgba);
}

void setFromHSV(color_t *color, Vector3 hsv){
    color->hsv = hsv;
    color->rgba = HSVToColor(hsv, color->rgba.a);
}


int main(int argc, char **argv){

    // draw loading screen
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1000, 800, "Image maker for angry programmers");
    SetExitKey(KEY_NULL); // disable exit on KEY_ESCAPE;
    SetTargetFPS(60);

    BeginDrawing();
    char *loading_text = "loading...";
    int loadingFontSize = 20;
    DrawText(loading_text, (GetScreenWidth()-MeasureText(loading_text, loadingFontSize))/2, GetScreenHeight()/2, loadingFontSize, WHITE);
    EndDrawing();

    // -- initialize application --

    canvas_t canvas = {0};
    bool hasImage = false;

    char name_field[MAX_NAME_FIELD_SIZE];
    char name_field_old[MAX_NAME_FIELD_SIZE];

    // load image from provided path
    if (argc == 2){
        Image img = LoadImage(argv[1]);
        if (!IsImageReady(img)){
            printf("Error: failed to load image from '%s'\n", argv[1]);
            UnloadImage(img);
            return 1;
        }
        canvas_setToImage(&canvas, img);
        if (strnlen(argv[1], MAX_NAME_FIELD_SIZE) == MAX_NAME_FIELD_SIZE){
            name_field[MAX_NAME_FIELD_SIZE-1] = 0; // brutal approach to make string fit.
            // TODO: more graceful solution, that doesn't rip out the postfix.
        }
        sprintf(name_field, "%s", argv[1]);
        setWindowTitleToPath(name_field);

        hasImage = true;
        UnloadImage(img);
    }

    // generate standard 8x8 image
    if (!hasImage){
        Image img = GenImageColor(8, 8, STD_COLOR);
        canvas_setToImage(&canvas, img);
        sprintf(name_field, "out.png");

        hasImage = true;
        UnloadImage(img);
    }
    sprintf(name_field_old, "%s", name_field);

    // strings for image resizing textBoxes
    const size_t DIM_STRLEN = 4; // TODO: support 4 digit sizes
    char x_field[DIM_STRLEN];
    char y_field[DIM_STRLEN];
    sprintf(x_field, "%d", (int)canvas.size.x);
    sprintf(y_field, "%d", (int)canvas.size.y);

    char hex_field[11];

    // track if any text box is being edited.
    bool isEditingHexField = false;
    bool isEditingXField = false;
    bool isEditingYField = false;
    bool isEditingNameField = false;

    // menu parameters
    int menu_font_size = 30;
    int menu_width = 200;
    int menu_padding = 20;
    int menu_content_width = menu_width - 2*menu_padding;

    int huebar_padding = GuiGetStyle(COLORPICKER, HUEBAR_PADDING);
    int huebar_width = GuiGetStyle(COLORPICKER, HUEBAR_WIDTH);

    // raygui style settings
    GuiSetStyle(DEFAULT, TEXT_SIZE, menu_font_size);
    GuiSetStyle(DEFAULT, TEXT_SPACING, 3);

    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, 0xFFFFFFFF);
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED, 0xDDDDDDFF);
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED, 0xFFFFFFFF);

    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, 0x202020FF);
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED, 0x181818FF);
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED, 0x300020FF);

    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, 0xFF0000FF);
    GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED, 0xFF0000FF);

    GuiSetStyle(DEFAULT, BACKGROUND_COLOR, 0x181818FF);


    bool forceWindowResize = true;
    bool showGrid = true;

    bool mousePressStartedInDefaultMode = false;

    // this could possibly be encapsulated. Any change in floating scale requires scale to be set as well.
    float _floating_scale = 0; // used to affect scale. To enable small changes a float is needed.
    int scale = 0; // floored floating scale that should be used everywhere.

    Rectangle drawingBounds = {0};
    Rectangle menu_rect = {0};
    Vector2 image_position = {0};

    // Track hsv + alpha instead of rgba,
    // because rgba can only store lossy hue values which leads to color-picker jitters when color approaches white or black.
    color_t active_color = {0};
    setFromRGBA(&active_color, FAV_COLOR); // important to initialize from rgba, because HSV does not supply alpha information.

    enum CURSOR_MODE cursor = CURSOR_DEFAULT;

    while(!WindowShouldClose()){
        if (IsWindowResized() || forceWindowResize){
            forceWindowResize = false;
            drawingBounds = (Rectangle){menu_width, 0, GetScreenWidth()-menu_width, GetScreenHeight()};
            menu_rect = (Rectangle){0, 0, menu_width, GetScreenHeight()};
            if (hasImage){
                // TODO support images bigger than drawingBounds
                // Not sure if this code should stay here. On one hand it is convenient to be able to find the image easily. On the other hand it is jarring to resize and move the image.
                _floating_scale = MAX(1.0f, floor(MIN(drawingBounds.width / canvas.size.x, drawingBounds.height / canvas.size.y)));
                scale = (int)_floating_scale;
                image_position = (Vector2){drawingBounds.width - canvas.size.x*scale, drawingBounds.height - canvas.size.y*scale};
                image_position = Vector2Scale(image_position, 0.5);
                image_position = Vector2Add((Vector2){drawingBounds.x, drawingBounds.y}, image_position);
            }
        }
        // handle mouse and keyboard input
        if (hasImage && !isEditingNameField){ // name field can overlap with the canvas
            // Disable set pixel on button down, if a tool was pressed. (to avoid set pixel on tool clicks)
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) mousePressStartedInDefaultMode = cursor == CURSOR_DEFAULT;

            // detect canvas click
            Rectangle image_bounds = {image_position.x, image_position.y, canvas.size.x*scale, canvas.size.y*scale};
            bool isHoveringMenu = CheckCollisionPointRec(GetMousePosition(), menu_rect);
            if (!isHoveringMenu && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), image_bounds)){
                Vector2 pixel = Vector2Scale(Vector2Subtract(GetMousePosition(), (Vector2){image_bounds.x, image_bounds.y}), 1.0f/(float)scale);
                // set pixel
                if (cursor == CURSOR_DEFAULT && mousePressStartedInDefaultMode){
                    canvas_setPixel(&canvas, pixel, active_color.rgba);
                }
                // pick color with pipette (only on press, not continuously)
                if (cursor == CURSOR_PIPETTE && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
                    cursor = CURSOR_DEFAULT;
                    Color pixel_color = canvas_getPixel(&canvas, pixel);
                    setFromRGBA(&active_color, pixel_color);
                }
                // color fill
                else if (cursor == CURSOR_COLOR_FILL && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
                    cursor = CURSOR_DEFAULT;
                    canvas_colorFlood(&canvas, pixel, active_color.rgba);
                }
            }
            // shortcuts
            if (!isEditingHexField){
                bool isCtrlDown = (IsKeyDown(KEY_LEFT_CONTROL)||IsKeyDown(KEY_RIGHT_CONTROL));
                if(GetKeyPressed()){ // a key was pressed this frame (this gets the next key in the queue, but we don't use that.)
                    if(IsKeyPressed(KEY_G)) showGrid = !showGrid;
                    if(IsKeyPressed(KEY_P)) toggleTool(&cursor, CURSOR_PIPETTE);
                    if(IsKeyPressed(KEY_F)) toggleTool(&cursor, CURSOR_COLOR_FILL);
                    if(IsKeyPressed(KEY_C) && isCtrlDown) toggleTool(&cursor, CURSOR_PIPETTE); // still toggle, to conveniently escape the mode without reaching for KEY_ESCAPE.
                    if(IsKeyPressed(KEY_S) && isCtrlDown) canvas_save_as_image(&canvas, name_field);
                    if(IsKeyPressed(KEY_ESCAPE)) cursor = CURSOR_DEFAULT;
                    if(IsKeyPressed(KEY_HOME)) forceWindowResize = true; // this causes the canvas to be centered again.
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
            Vector2 image_size = Vector2Scale(canvas.size, (int)_floating_scale);
            if (image_position.x + image_size.x < drawingBounds.x + scale) image_position.x = drawingBounds.x - image_size.x + scale;
            if (image_position.y + image_size.y < drawingBounds.y + scale) image_position.y = drawingBounds.y - image_size.y + scale;
            if (drawingBounds.x + drawingBounds.width  < image_position.x + scale) image_position.x = drawingBounds.x + drawingBounds.width - scale;
            if (drawingBounds.y + drawingBounds.height < image_position.y + scale) image_position.y = drawingBounds.y + drawingBounds.height - scale;
        }

        BeginDrawing();
        ClearBackground(FAV_COLOR);

        // draw image
        Vector2 floored_image_position = {(int)image_position.x, (int)image_position.y}; // image_position is not an integer value at this point, which can cause slight distortions when drawing. outright flooring it degrades zoom precision.
        DrawTextureEx(canvas.tex, floored_image_position, 0, scale, WHITE); // use int scale, so that every pixel of the texture is drawn as the same multiple. This is important for drawing the grid.

        // draw grid
        const Color GRID_COLOR = DARKGRAY;
        if(showGrid && scale >= 3){
            for (int i = 1; i < canvas.size.x; i++){
                DrawLineEx((Vector2){floored_image_position.x + i*scale, floored_image_position.y}, (Vector2){floored_image_position.x + i*scale, floored_image_position.y + canvas.size.y*scale}, 1, GRID_COLOR);
            }
            for (int j = 1; j < canvas.size.y; j++){
                DrawLineEx((Vector2){floored_image_position.x, floored_image_position.y + j*scale}, (Vector2){floored_image_position.x + canvas.size.x*scale, floored_image_position.y + j*scale}, 1, GRID_COLOR);
            }
            DrawRectangleLines(floored_image_position.x-1, floored_image_position.y-1, canvas.size.x*scale+2, canvas.size.y*scale+2, GRID_COLOR);
        }

        // draw menu
        DrawLine(menu_width, 0, menu_width, GetScreenHeight(), GRAY);

        // color picker
        int color_picker_y = menu_padding;
        int color_picker_size = menu_content_width - huebar_width - huebar_padding;

        Vector3 color_picker_hsv = active_color.hsv;
        float bar_hue = color_picker_hsv.x; // The color panel should only affect saturation & value. Thus hue is buffered.
        GuiColorPanelHSV((Rectangle){menu_padding, color_picker_y, color_picker_size, color_picker_size}, "heyyyy", &color_picker_hsv);
        GuiColorBarHue((Rectangle){menu_padding + color_picker_size + huebar_padding, color_picker_y, huebar_width, color_picker_size}, "oiii", &bar_hue);
        color_picker_hsv.x = bar_hue;

        // check if the color picker changed the color
        if (memcmp(&color_picker_hsv, &active_color.hsv, sizeof(color_picker_hsv)) != 0){
            setFromHSV(&active_color, color_picker_hsv);
        }


        // hex code text box
        if (!isEditingHexField){
            Color color = active_color.rgba;
            sprintf(hex_field, "%02X%02X%02X%02X", color.r, color.g, color.b, color.a);
        }

        Rectangle hex_box = {menu_padding, color_picker_size + color_picker_y + huebar_padding, menu_content_width, menu_font_size};
        if(GuiTextBox(hex_box, hex_field, 9, isEditingHexField)){
            isEditingHexField = !isEditingHexField;
            if (!isEditingHexField){
                bool valid_hex = true;
                for(int i = 0; i < 8; i++){
                    bool is_hex = (hex_field[i] <= '9' && hex_field [i] >= '0') || (hex_field[i] >= 'A' && hex_field[i] <= 'F');
                    if (!is_hex){
                        printf("digit %d: disallowed char '%c' (%d) \n",i,  hex_field[i], hex_field[i]);
                        valid_hex = false;
                        break;
                    }
                }
                if (!valid_hex){
                    printf("invalid hex color entered!\n");
                    // TODO show error
                } else {
                    Color new_color = {0};
                    sscanf(hex_field, "%02X%02X%02X%02X", (unsigned int*)&new_color.r, (unsigned int*)&new_color.g, (unsigned int*)&new_color.b, (unsigned int*)&new_color.a);
                    setFromRGBA(&active_color, new_color);
                }
            }
        }

        int options_y = color_picker_size + color_picker_y + 2*huebar_padding + menu_font_size;
        int item = 0;

        toolToggleButton("pipette", &cursor, CURSOR_PIPETTE, 27, options_y + (item++)*(huebar_padding+menu_font_size), menu_padding, menu_content_width, menu_font_size);

        item++;
        toolToggleButton("color fill", &cursor, CURSOR_COLOR_FILL, 29, options_y + (item++)*(huebar_padding+menu_font_size), menu_padding, menu_content_width, menu_font_size);

        item++;
        // grid checkbox
        GuiCheckBox((Rectangle){menu_padding, options_y + (item++)*(huebar_padding+menu_font_size), menu_font_size, menu_font_size}, "grid", &showGrid);

        // x resize textbox
        Rectangle x_box = {menu_padding, options_y + item*(huebar_padding+menu_font_size), menu_content_width - menu_font_size, menu_font_size};
        long res = dimensionTextBox(x_box, x_field, DIM_STRLEN, canvas.size.x, &isEditingXField);
        if (res > 0){
            // TODO: fix flicker on resize? (caused by unloading the currently drawn texture)
            canvas.size.x = res;
            forceWindowResize = true;
            canvas_resize(&canvas, canvas.size, STD_COLOR); // FIXME: see if it is really a good idea to modify canvas.size. possibly introduce a different variable. Could play nicely with a resize confirmation button.
        }
        DrawText("x", menu_padding + menu_content_width + huebar_padding - menu_font_size ,options_y + (item++)*(huebar_padding+menu_font_size), menu_font_size, WHITE);

        // y resize textbox
        Rectangle y_box = {menu_padding, options_y + item*(huebar_padding+menu_font_size), menu_content_width - menu_font_size, menu_font_size};
        res = dimensionTextBox(y_box, y_field, DIM_STRLEN, canvas.size.y, &isEditingYField);
        if (res > 0){
            canvas.size.y = res;
            forceWindowResize = true;
            canvas_resize(&canvas, canvas.size, STD_COLOR); // FIXME: see if it is really a good idea to modify canvas.size. possibly introduce a different variable. Could play nicely with a resize confirmation button.
        }
        DrawText("y", menu_padding + menu_content_width + huebar_padding - menu_font_size ,options_y + (item++)*(huebar_padding+menu_font_size), menu_font_size, WHITE);

        // lower menu
        int min_y = options_y + item*(huebar_padding+menu_font_size);
        int save_button_height = menu_font_size/6*7; // slightly higher, so that parentheses fit in the box.

        // file name
        int desired_text_box_y = GetScreenHeight() - menu_padding - huebar_padding - menu_font_size - save_button_height;
        int name_width = menu_content_width;
        if (isEditingNameField){
            int needed_width = MeasureText(name_field, menu_font_size) + menu_font_size;
            name_width = MAX(name_width, needed_width);
            name_width = MIN(name_width, GetScreenWidth() - 2*menu_padding);

            DrawText(".png .bmp .qoi .raw", menu_padding, MAX(min_y, desired_text_box_y) - 0.6*menu_font_size, 0.6*menu_font_size, STD_COLOR);
        }
        if(GuiTextBox((Rectangle){menu_padding, MAX(min_y, desired_text_box_y), name_width, menu_font_size}, name_field, MAX_NAME_FIELD_SIZE, isEditingNameField)){
            isEditingNameField = !isEditingNameField;
            // validate
            if (!isEditingNameField){
                if (isSupportedImageFormat(name_field) 
                    && DirectoryExists(GetDirectoryPath(name_field))){
                    // accept new name
                        memcpy(name_field_old, name_field, strlen(name_field)+1);
                        setWindowTitleToPath(name_field);
                } else {
                    // reject name: reset to old name
                    memcpy(name_field, name_field_old, strlen(name_field_old)+1);
                }
            }
        }

        // save button
        int desired_save_button_y = GetScreenHeight() - menu_padding - save_button_height;
        Rectangle save_rect = {menu_padding, MAX(min_y + menu_font_size + menu_padding, desired_save_button_y), menu_content_width, save_button_height};
        if (CheckCollisionPointRec(GetMousePosition(), save_rect)) DrawText("ctrl+s", menu_width, save_rect.y + (save_rect.height - menu_font_size)/2, menu_font_size, WHITE);
        if(GuiButton(save_rect, "save")){
            canvas_save_as_image(&canvas, name_field);
        }

        EndDrawing();
    }

    canvas_free(&canvas);
    CloseWindow();
}

