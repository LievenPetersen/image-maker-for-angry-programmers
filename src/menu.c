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

#include "menu.h"

#define RAYGUI_IMPLEMENTATION
#include "external/raygui.h"

#include "external/tinyfiledialogs/tinyfiledialogs.h"

#ifndef DISABLE_CUSTOM_FONT
    #include "font.h"
    #define FONT_LEVELS 3
#endif //DISABLE_CUSTOM_FONT



static void toolToggleButton(const char *text, enum CURSOR_MODE *cursor, enum CURSOR_MODE tool, int iconId, int y_position, int h_padding, int total_width, int font_size){
    // pipette
    Rectangle pipette_box = {h_padding, y_position, total_width - font_size, font_size};
    if(GuiButton(pipette_box, text)){
        toggleTool(cursor , tool);
    }
    if (*cursor == tool){
        DrawRectangleRec(pipette_box, ColorAlpha(RED, 0.3));
    }
    const int icon_height = 16;
    GuiDrawIcon(iconId, h_padding + total_width - font_size, y_position, roundf(font_size/(float)icon_height), WHITE);
}

// returns > 0  on resize
static long dimensionTextBox(const Rectangle bounds, char *str_value, const size_t str_size, long reset_value, bool *isEditing){
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

bool isSupportedImageFormat(const char *filePath){
    const char *VALID_EXTENSIONS = ".png;.bmp;.qoi;.raw"; //.tga;.jpg;.jpeg"; // .tga and .jpg don't work for some reason
    return IsFileExtension(filePath, VALID_EXTENSIONS);
}


static void setMenuStyle() {
    // raygui style settings
    GuiSetStyle(DEFAULT, TEXT_SIZE, DEFAULT_FONT_SIZE);
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
}

static void loadMenuFont(menu_state_t *ms){
    Font font = {0};
    // load custom font
#ifndef DISABLE_CUSTOM_FONT
    // The font is compressed and encoded into an array.
    int ttf_font_data_size = 0;
    unsigned char *ttf_font_data = DecompressData((unsigned char*)COMPRESSED_TTF_FONT, COMPRESSED_TTF_FONT_SIZE, &ttf_font_data_size);

    Font *fonts = RL_MALLOC(FONT_LEVELS * sizeof(*fonts)); // 0.5x, 1.0x and 2.0x font size

    fonts[0] = LoadFontFromMemory(".ttf", ttf_font_data, ttf_font_data_size, 0.5*DEFAULT_FONT_SIZE, NULL, 0);
    fonts[1] = LoadFontFromMemory(".ttf", ttf_font_data, ttf_font_data_size, 1.0*DEFAULT_FONT_SIZE, NULL, 0);
    fonts[2] = LoadFontFromMemory(".ttf", ttf_font_data, ttf_font_data_size, 2.0*DEFAULT_FONT_SIZE, NULL, 0);
    RL_FREE(ttf_font_data);
    GuiSetStyle(DEFAULT, TEXT_SPACING, 0);
    font = fonts[1];
    ms->fonts = fonts;
#else
    font = GetFontDefault();
#endif // DISABLE_CUSTOM_FONT
    ms->font = font;
    ms->font_size = DEFAULT_FONT_SIZE;
}

menu_state_t initMenu(char *image_name){
    menu_state_t menu_state = {
        .x_field            = RL_MALLOC(DIM_STRLEN),
        .y_field            = RL_MALLOC(DIM_STRLEN),
        .hex_field          = RL_MALLOC(11),
        .filename          = RL_MALLOC(MAX_FILENAME_SIZE),
        .filename_old      = RL_MALLOC(MAX_FILENAME_SIZE),
        .isEditingFileName  = false,
        .isEditingHexField  = false,
        .isEditingXField    = false,
        .isEditingYField    = false
    };
    sprintf(menu_state.filename,     "%s", image_name);
    sprintf(menu_state.filename_old, "%s", image_name);

    setMenuStyle();
    loadMenuFont(&menu_state);

    return menu_state;
}

void drawMenuDragger(Rectangle rect, bool isDragging){
    GuiState state = STATE_NORMAL;
    // FIXME: frankenstein combination of raygui and own global state
    if (isDragging){
        state = STATE_PRESSED;
    } else if(!guiSliderDragging && CheckCollisionPointRec(GetMousePosition(), rect)){
        state = STATE_FOCUSED;
    }

    Color color = GRAY;
    if (state != STATE_NORMAL){
        color = RED;
    }
    DrawRectangleLinesEx(rect, 1, color);
    for (int x = 1; x <= 2; x++){
        for (int y = 1; y <= 4; y++){
            DrawCircle(rect.x + x * (rect.width/3), rect.y + y*(rect.height/5), 1, color);
        }
    }
}

void drawMenu(shared_state_t *s, menu_state_t *ms){
    // menu dragger
    int dragger_height = ms->font_size; // 30
    int dragger_width = 2 * ms->font_size / 5; // 12
    s->dragger = (Rectangle){s->menu_rect.width - dragger_width - 1, (s->menu_rect.height - dragger_height)/2, dragger_width, dragger_height};
    // if menu is collapsed, present dragger at screen edge.
    if (s->menu_rect.width == 0) s->dragger.x = 0;

    drawMenuDragger(s->dragger, ms->isDragging);

    int minimum_menu_width = ms->font_size * 180 / DEFAULT_FONT_SIZE;

    // drag menu
    // TODO: clean up dragging code
    if (!guiSliderDragging && !ms->isDragging && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), s->dragger)){
        ms->isDragging = true;
        ms->isClick = true;
        s->isUsingMouse = true;
        guiSliderDragging = true;
        ms->dragOrigin = GetMousePosition();
        ms->dragOffset = (Vector2){s->menu_rect.width - GetMousePosition().x, s->menu_rect.height - GetMousePosition().y};
        if (s->menu_rect.width >= minimum_menu_width){
            ms->originalMenuWidth = s->menu_rect.width;
        }
    }
    if (ms->isDragging){
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)){
            ms->isDragging = false;
            s->isUsingMouse = false;
            guiSliderDragging = false;
            if (ms->isClick){
                s->menu_rect.width = s->menu_rect.width == 0 ? MAX(ms->originalMenuWidth, minimum_menu_width) : 0;
                s->forceImageResize = true;
            }
        } else {
            int new_width = MIN(GetMousePosition().x + ms->dragOffset.x, GetScreenWidth());
            if (GetMousePosition().x != ms->dragOrigin.x || GetMousePosition().y != ms->dragOrigin.y){
                ms->isClick = false;
            }
            s->forceImageResize = true;
            if (new_width < minimum_menu_width){
                s->menu_rect.width = minimum_menu_width;
                if(new_width < 3*minimum_menu_width/4.0){
                    s->menu_rect.width = 0;
                }
            } else {
                s->menu_rect.width = new_width;
            }
        }
    }

    // don't draw if menu is hidden
    if (s->menu_rect.width == 0) return;
    // push out if menu size was increased, for example by increasing font_size
    if (s->menu_rect.width < minimum_menu_width){
        s->menu_rect.width = minimum_menu_width;
        s->forceImageResize = true;
    }

    // TODO: this only needs to happen if the font size was changed
    #ifndef DISABLE_CUSTOM_FONT
        // chose most fitting font resolution, avoid upscaling.
        if (2*ms->font_size <= DEFAULT_FONT_SIZE) { ms->font = ms->fonts[0]; }
        else if (ms->font_size > DEFAULT_FONT_SIZE) { ms->font = ms->fonts[2]; }
        else { ms->font = ms->fonts[1]; }
    #endif //DISABLE_CUSTOM_FONT

    GuiSetFont(ms->font);
    GuiSetStyle(DEFAULT, TEXT_SIZE, ms->font_size);

    // utilities for menu layout
    int menu_padding = s->menu_rect.width / 10;
    int menu_content_width = s->menu_rect.width - 2*menu_padding;
    int huebar_width = s->menu_rect.width*2/25;
    int huebar_padding = huebar_width/2;

    // draw menu
    DrawRectangleRec(s->menu_rect, ColorAlpha(FAV_COLOR, 0.2));
    DrawLine(s->menu_rect.width, 0, s->menu_rect.width, s->menu_rect.height, GRAY);

    // color picker
    int color_picker_y = menu_padding;
    int color_picker_size = menu_content_width - huebar_width - huebar_padding;

    Vector3 color_picker_hsv = s->active_color.hsv;
    float bar_alpha = s->active_color.rgba.a / 255.0;
    float bar_hue = color_picker_hsv.x; // The color panel should only affect saturation & value. Thus hue is buffered.
    GuiColorPanelHSV((Rectangle){menu_padding, color_picker_y, color_picker_size, color_picker_size}, "heyyyy", &color_picker_hsv);
    GuiColorBarHue(  (Rectangle){menu_padding + color_picker_size + huebar_padding, color_picker_y, huebar_width, color_picker_size}, "oiii", &bar_hue);
    GuiColorBarAlpha((Rectangle){menu_padding, color_picker_y + color_picker_size + huebar_padding, color_picker_size + huebar_padding + huebar_width, huebar_width}, "uiii", &bar_alpha);
    color_picker_hsv.x = bar_hue;

    // check if the color picker changed the color
    if (memcmp(&color_picker_hsv, &s->active_color.hsv, sizeof(color_picker_hsv)) != 0){
        setFromHSV(&s->active_color, color_picker_hsv);
    }
    // set alpha value. Since it is not represented in HSV, we modify rgba directly, to avoid overriding the hsv value with a lossy rgb -> hsv conversion.
    s->active_color.rgba.a = bar_alpha * 255;


    // hex code text box
    if (!ms->isEditingHexField){
        Color color = s->active_color.rgba;
        sprintf(ms->hex_field, "%02X%02X%02X%02X", color.r, color.g, color.b, color.a);
    }

    Rectangle hex_box = {menu_padding, color_picker_size + color_picker_y + 2*huebar_padding + huebar_width, menu_content_width, ms->font_size};
    if(GuiTextBox(hex_box, ms->hex_field, 9, ms->isEditingHexField)){
        ms->isEditingHexField = !ms->isEditingHexField;
        if (!ms->isEditingHexField){
            bool valid_hex = true;
            for(int i = 0; i < 8; i++){
                // to lower
                if (ms->hex_field[i] >= 'a' && ms->hex_field[i] <= 'f') ms->hex_field[i] &= 0xDF; // set the 0x20 bit to 0, thus turning to uppercase.
                bool is_hex = (ms->hex_field[i] <= '9' && ms->hex_field [i] >= '0') || (ms->hex_field[i] >= 'A' && ms->hex_field[i] <= 'F');
                if (!is_hex){
                    printf("digit %d: disallowed char '%c' (%d) \n",i,  ms->hex_field[i], ms->hex_field[i]);
                    valid_hex = false;
                    break;
                }
            }
            if (!valid_hex){
                printf("invalid hex color entered!\n");
                // TODO show error
            } else {
                Color new_color = {0};
                sscanf(ms->hex_field, "%02X%02X%02X%02X", (unsigned int*)&new_color.r, (unsigned int*)&new_color.g, (unsigned int*)&new_color.b, (unsigned int*)&new_color.a);
                setFromRGBA(&s->active_color, new_color);
            }
        }
    }

    int options_y = color_picker_size + color_picker_y + 3*huebar_padding + huebar_width + ms->font_size;
    int item = 0;

    // undo & redo buttons
    Rectangle undo_box = {menu_padding, options_y + item*(huebar_padding+ms->font_size), 0.5*menu_content_width, ms->font_size};
    Rectangle redo_box = {menu_padding + 0.5*menu_content_width, options_y + (item++)*(huebar_padding+ms->font_size), 0.5*menu_content_width, ms->font_size};
    if(GuiButton(undo_box, "#130#")){
        if(canvas_undo(s->canvas)) s->forceImageResize = true;
    }
    if(GuiButton(redo_box, "#131#")){
        if(canvas_redo(s->canvas)) s->forceImageResize = true;
    }

    toolToggleButton("pipette", &s->cursor, CURSOR_PIPETTE, 27, options_y + (item++)*(huebar_padding+ms->font_size), menu_padding, menu_content_width, ms->font_size);

    toolToggleButton("fill", &s->cursor, CURSOR_COLOR_FILL, 29, options_y + (item++)*(huebar_padding+ms->font_size), menu_padding, menu_content_width, ms->font_size);

    item++;
    // grid checkbox
    GuiCheckBox((Rectangle){menu_padding, options_y + (item++)*(huebar_padding+ms->font_size), ms->font_size, ms->font_size}, "grid", &s->showGrid);

    // x resize textbox
    if (!ms->isEditingXField) sprintf(ms->x_field, "%d", (int)canvas_getSize(s->canvas).x); // TODO: maybe only reprint this if canvas size changes.
    Rectangle x_box = {menu_padding, options_y + item*(huebar_padding+ms->font_size), menu_content_width - ms->font_size, ms->font_size};
    long res = dimensionTextBox(x_box, ms->x_field, DIM_STRLEN, canvas_getSize(s->canvas).x, &ms->isEditingXField);
    if (res > 0){
        Vector2 new_size = canvas_getSize(s->canvas);
        new_size.x = res;
        s->forceWindowResize = true;
        canvas_resize(s->canvas, new_size, STD_COLOR);
    }
    DrawTextEx(ms->font, "W", (Vector2){menu_padding + menu_content_width + huebar_padding - ms->font_size, options_y + (item++)*(huebar_padding+ms->font_size)}, ms->font_size, 1, WHITE);

    // y resize textbox
    if (!ms->isEditingYField) sprintf(ms->y_field, "%d", (int)canvas_getSize(s->canvas).y); // TODO: maybe only reprint this if canvas size changes.
    Rectangle y_box = {menu_padding, options_y + item*(huebar_padding+ms->font_size), menu_content_width - ms->font_size, ms->font_size};
    res = dimensionTextBox(y_box, ms->y_field, DIM_STRLEN, canvas_getSize(s->canvas).y, &ms->isEditingYField);
    if (res > 0){
        Vector2 new_size = canvas_getSize(s->canvas);
        new_size.y = res;
        s->forceWindowResize = true;
        canvas_resize(s->canvas, new_size, STD_COLOR);
    }
    DrawTextEx(ms->font, "H", (Vector2){menu_padding + menu_content_width + huebar_padding - ms->font_size, options_y + (item++)*(huebar_padding+ms->font_size)}, ms->font_size, 1, WHITE);

    // change resolution buttons
    Rectangle first_box = {menu_padding, options_y + item*(huebar_padding+ms->font_size), 0.5*(menu_content_width - ms->font_size), ms->font_size};
    Rectangle second_box = {menu_padding + 0.5*(menu_content_width - ms->font_size), options_y + item*(huebar_padding+ms->font_size), 0.5*(menu_content_width - ms->font_size), ms->font_size};
    if(GuiButton(first_box, "#96#*2")){
        canvas_changeResolution(s->canvas, 2);
        s->forceImageResize = true;
    }
    if(GuiButton(second_box, "#111#/2")){
        canvas_changeResolution(s->canvas, 0.5);
        s->forceImageResize = true;
    }

    item++;

    // lower menu
    const int lower_menu_items = 3;
    const int lower_menu_item_size = ms->font_size + huebar_padding;
    int min_lower_menu_y = options_y + item*(huebar_padding+ms->font_size);
    int desired_lower_menu_y = s->menu_rect.height - menu_padding - lower_menu_items*ms->font_size - (lower_menu_items-1)*huebar_padding;
    int lower_menu_y = MAX(min_lower_menu_y, desired_lower_menu_y);

    Rectangle load_rect = {menu_padding, lower_menu_y, menu_content_width, ms->font_size};
    if(GuiButton(load_rect, "load")){
        char * new_file = tinyfd_openFileDialog("load image", ms->filename, 0, NULL, NULL, false);
        if (new_file){
            if (FileExists(new_file)){
                // TODO: recording the old file name for undo would be handy
                // TODO: undo has unwanted implications when overwriting an existing different file.
                // TODO: new canvas would be cleaner here.
                // FIXME: add method to swap out the old canvas for a new one or reset it's state
                Image new_image = LoadImage(new_file);
                if (IsImageReady(new_image)){
                    canvas_setToImage(s->canvas, new_image);
                    s->forceImageResize = true;
                } else {
                    printf("Error: failed to load image from '%s'\n", new_file);
                }
                UnloadImage(new_image); // free image in both cases
            } else {
                printf("Error: file '%s' does not exist!\n", new_file);
            }
        }
    }

    // file name
    int name_width = menu_content_width;
    int text_box_y = lower_menu_y + lower_menu_item_size;
    if (ms->isEditingFileName){
        int needed_width = MeasureText(ms->filename, ms->font_size) + ms->font_size;
        name_width = MAX(name_width, needed_width);
        name_width = MIN(name_width, GetScreenWidth() - 2*menu_padding); // TODO: calculate actual space if menu is on left side

        DrawTextEx(ms->font, ".png .bmp .qoi .raw", (Vector2){menu_padding, text_box_y - 0.6*ms->font_size}, 0.6*ms->font_size, 1, STD_COLOR);
    }
    if(GuiTextBox((Rectangle){menu_padding, text_box_y, name_width, ms->font_size}, ms->filename, MAX_FILENAME_SIZE, ms->isEditingFileName)){
        ms->isEditingFileName = !ms->isEditingFileName;
        // validate
        if (!ms->isEditingFileName){
            if (isSupportedImageFormat(ms->filename)
                && DirectoryExists(GetDirectoryPath(ms->filename))){
                // accept new name
                    memcpy(ms->filename_old, ms->filename, strlen(ms->filename)+1);
                    setWindowTitleToPath(ms->filename);
            } else {
                // reject name: reset to old name
                memcpy(ms->filename, ms->filename_old, strlen(ms->filename_old)+1);
            }
        }
    }

    // save button
    int save_button_y = lower_menu_y + 2*lower_menu_item_size;
    Rectangle save_rect = {menu_padding, save_button_y, menu_content_width, ms->font_size};
    if (CheckCollisionPointRec(GetMousePosition(), save_rect)) DrawTextEx(ms->font, "ctrl+s", (Vector2){s->menu_rect.width, save_rect.y + (save_rect.height - ms->font_size)/2}, ms->font_size, 1, WHITE);
    if(GuiButton(save_rect, "save")){
        canvas_saveAsImage(s->canvas, ms->filename);
    }
}

void unloadMenu(menu_state_t *ms){
    RL_FREE(ms->x_field);
    RL_FREE(ms->y_field);
    RL_FREE(ms->hex_field);
    RL_FREE(ms->filename);
    RL_FREE(ms->filename_old);
    #ifndef DISABLE_CUSTOM_FONT
        for (int i = 0; i < FONT_LEVELS; i++){
            UnloadFont(ms->fonts[i]);
        }
        RL_FREE(ms->fonts);
    #endif //DISABLE_CUSTOM_FONT
}
