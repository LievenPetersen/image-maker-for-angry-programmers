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
    // TODO: icon should scale with font size.
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

static Font loadMenuFont(menu_state_t *ms){
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
    ms->font = font;
#else
    font = GetFontDefault();
#endif // DISABLE_CUSTOM_FONT
    return font;
}

void initMenu(menu_state_t *ms){
    setMenuStyle();
    loadMenuFont(ms);
}

void drawMenu(shared_state_t *s, menu_state_t *ms, int menu_width, int menu_font_size){

    // TODO: this only needs to happen if the font size was changed
    #ifndef DISABLE_CUSTOM_FONT
        // chose most fitting font resolution, avoid upscaling.
        if (2*menu_font_size <= DEFAULT_FONT_SIZE) { ms->font = ms->fonts[0]; }
        else if (menu_font_size > DEFAULT_FONT_SIZE) { ms->font = ms->fonts[2]; }
        else { ms->font = ms->fonts[1]; }
    #endif //DISABLE_CUSTOM_FONT

    GuiSetFont(ms->font);
    GuiSetStyle(DEFAULT, TEXT_SIZE, menu_font_size);

    // utilities for menu layout
    int menu_padding = menu_width / 10;
    int menu_content_width = menu_width - 2*menu_padding;
    int huebar_width = menu_width*2/25;
    int huebar_padding = huebar_width/2;

    // draw menu
    DrawRectangle(0, 0, menu_width, GetScreenHeight(), ColorAlpha(FAV_COLOR, 0.2));
    DrawLine(menu_width, 0, menu_width, GetScreenHeight(), GRAY);

    // color picker
    int color_picker_y = menu_padding;
    int color_picker_size = menu_content_width - huebar_width - huebar_padding;

    Vector3 color_picker_hsv = s->active_color.hsv;
    float bar_hue = color_picker_hsv.x; // The color panel should only affect saturation & value. Thus hue is buffered.
    GuiColorPanelHSV((Rectangle){menu_padding, color_picker_y, color_picker_size, color_picker_size}, "heyyyy", &color_picker_hsv);
    GuiColorBarHue((Rectangle){menu_padding + color_picker_size + huebar_padding, color_picker_y, huebar_width, color_picker_size}, "oiii", &bar_hue);
    color_picker_hsv.x = bar_hue;

    // check if the color picker changed the color
    if (memcmp(&color_picker_hsv, &s->active_color.hsv, sizeof(color_picker_hsv)) != 0){
        setFromHSV(&s->active_color, color_picker_hsv);
    }


    // hex code text box
    if (!ms->isEditingHexField){
        Color color = s->active_color.rgba;
        sprintf(ms->hex_field, "%02X%02X%02X%02X", color.r, color.g, color.b, color.a);
    }

    Rectangle hex_box = {menu_padding, color_picker_size + color_picker_y + huebar_padding, menu_content_width, menu_font_size};
    if(GuiTextBox(hex_box, ms->hex_field, 9, ms->isEditingHexField)){
        ms->isEditingHexField = !ms->isEditingHexField;
        if (!ms->isEditingHexField){
            bool valid_hex = true;
            for(int i = 0; i < 8; i++){
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

    int options_y = color_picker_size + color_picker_y + 2*huebar_padding + menu_font_size;
    int item = 0;

    // undo / redo buttons
    Rectangle undo_box = {menu_padding, options_y + item*(huebar_padding+menu_font_size), 0.5*menu_content_width, menu_font_size};
    Rectangle redo_box = {menu_padding + 0.5*menu_content_width, options_y + (item++)*(huebar_padding+menu_font_size), 0.5*menu_content_width, menu_font_size};
    if(GuiButton(undo_box, "#130#")){
        if(canvas_undo(s->canvas)) s->forceImageResize = true;
    }
    if(GuiButton(redo_box, "#131#")){
        if(canvas_redo(s->canvas)) s->forceImageResize = true;
    }

    toolToggleButton("pipette", &s->cursor, CURSOR_PIPETTE, 27, options_y + (item++)*(huebar_padding+menu_font_size), menu_padding, menu_content_width, menu_font_size);

    toolToggleButton("fill", &s->cursor, CURSOR_COLOR_FILL, 29, options_y + (item++)*(huebar_padding+menu_font_size), menu_padding, menu_content_width, menu_font_size);

    item++;
    // grid checkbox
    GuiCheckBox((Rectangle){menu_padding, options_y + (item++)*(huebar_padding+menu_font_size), menu_font_size, menu_font_size}, "grid", &s->showGrid);

    // x resize textbox
    if (!ms->isEditingXField) sprintf(ms->x_field, "%d", (int)canvas_getSize(s->canvas).x); // TODO: maybe only reprint this if canvas size changes.
    Rectangle x_box = {menu_padding, options_y + item*(huebar_padding+menu_font_size), menu_content_width - menu_font_size, menu_font_size};
    long res = dimensionTextBox(x_box, ms->x_field, DIM_STRLEN, canvas_getSize(s->canvas).x, &ms->isEditingXField);
    if (res > 0){
        Vector2 new_size = canvas_getSize(s->canvas);
        new_size.x = res;
        s->forceWindowResize = true;
        canvas_resize(s->canvas, new_size, STD_COLOR);
    }
    DrawTextEx(ms->font, "W", (Vector2){menu_padding + menu_content_width + huebar_padding - menu_font_size, options_y + (item++)*(huebar_padding+menu_font_size)}, menu_font_size, 1, WHITE);

    // y resize textbox
    if (!ms->isEditingYField) sprintf(ms->y_field, "%d", (int)canvas_getSize(s->canvas).y); // TODO: maybe only reprint this if canvas size changes.
    Rectangle y_box = {menu_padding, options_y + item*(huebar_padding+menu_font_size), menu_content_width - menu_font_size, menu_font_size};
    res = dimensionTextBox(y_box, ms->y_field, DIM_STRLEN, canvas_getSize(s->canvas).y, &ms->isEditingYField);
    if (res > 0){
        Vector2 new_size = canvas_getSize(s->canvas);
        new_size.y = res;
        s->forceWindowResize = true;
        canvas_resize(s->canvas, new_size, STD_COLOR);
    }
    DrawTextEx(ms->font, "H", (Vector2){menu_padding + menu_content_width + huebar_padding - menu_font_size, options_y + (item++)*(huebar_padding+menu_font_size)}, menu_font_size, 1, WHITE);

    // change resolution buttons
    Rectangle first_box = {menu_padding, options_y + item*(huebar_padding+menu_font_size), 0.5*(menu_content_width - menu_font_size), menu_font_size};
    Rectangle second_box = {menu_padding + 0.5*(menu_content_width - menu_font_size), options_y + item*(huebar_padding+menu_font_size), 0.5*(menu_content_width - menu_font_size), menu_font_size};
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
    int min_y = options_y + item*(huebar_padding+menu_font_size);
    int save_button_height = menu_font_size/6*7; // slightly higher, so that parentheses fit in the box.

    // file name
    int desired_text_box_y = GetScreenHeight() - menu_padding - huebar_padding - menu_font_size - save_button_height;
    int name_width = menu_content_width;
    if (ms->isEditingNameField){
        int needed_width = MeasureText(ms->name_field, menu_font_size) + menu_font_size;
        name_width = MAX(name_width, needed_width);
        name_width = MIN(name_width, GetScreenWidth() - 2*menu_padding);

        DrawTextEx(ms->font, ".png .bmp .qoi .raw", (Vector2){menu_padding, MAX(min_y, desired_text_box_y) - 0.6*menu_font_size}, 0.6*menu_font_size, 1, STD_COLOR);
    }
    if(GuiTextBox((Rectangle){menu_padding, MAX(min_y, desired_text_box_y), name_width, menu_font_size}, ms->name_field, MAX_NAME_FIELD_SIZE, ms->isEditingNameField)){
        ms->isEditingNameField = !ms->isEditingNameField;
        // validate
        if (!ms->isEditingNameField){
            if (isSupportedImageFormat(ms->name_field)
                && DirectoryExists(GetDirectoryPath(ms->name_field))){
                // accept new name
                    memcpy(ms->name_field_old, ms->name_field, strlen(ms->name_field)+1);
                    setWindowTitleToPath(ms->name_field);
            } else {
                // reject name: reset to old name
                memcpy(ms->name_field, ms->name_field_old, strlen(ms->name_field_old)+1);
            }
        }
    }

    // save button
    int desired_save_button_y = GetScreenHeight() - menu_padding - save_button_height;
    Rectangle save_rect = {menu_padding, MAX(min_y + menu_font_size + menu_padding, desired_save_button_y), menu_content_width, save_button_height};
    if (CheckCollisionPointRec(GetMousePosition(), save_rect)) DrawTextEx(ms->font, "ctrl+s", (Vector2){menu_width, save_rect.y + (save_rect.height - menu_font_size)/2}, menu_font_size, 1, WHITE);
    if(GuiButton(save_rect, "save")){
        canvas_saveAsImage(s->canvas, ms->name_field);
    }
}

void unloadMenu(menu_state_t *ms){
    #ifndef DISABLE_CUSTOM_FONT
        for (int i = 0; i < FONT_LEVELS; i++){
            UnloadFont(ms->fonts[i]);
        }
        RL_FREE(ms->fonts);
    #endif //DISABLE_CUSTOM_FONT
}
