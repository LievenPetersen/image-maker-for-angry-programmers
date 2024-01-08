#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include <raylib.h>
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include <raymath.h>

#define MIN(a, b) (a<b? (a) : (b))

#define FAV_COLOR ((Color){0x18, 0x18, 0x18, 0xFF}) // sorry, but AA is a bit impractical

bool save_texture_as_image(Texture2D tex, const char *path){
    Image result = LoadImageFromTexture(tex);

    bool success = 
        IsImageReady(result)
        && ExportImage(result, path);

    UnloadImage(result);

    if (!success){
        printf("Error while saving image!\n");
    }
    return success;
}

int main(){
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1000, 800, "Image maker for angry programmers");
    SetTargetFPS(60);
    BeginDrawing();
    char *loading_text = "loading...";
    int loadingFontSize = 20;
    DrawText(loading_text, (GetScreenWidth()-MeasureText(loading_text, loadingFontSize))/2, GetScreenHeight()/2, loadingFontSize, WHITE);
    EndDrawing();

    const size_t DIM_SIZE = 10;
    char x_field[DIM_SIZE];
    char y_field[DIM_SIZE];
    char hex_field[11];
    sprintf(x_field, "10");
    sprintf(y_field, "10");

    int menu_font_size = 30;
    int menu_width = 200;
    int menu_padding = 20;
    int menu_content_width = menu_width - 2*menu_padding;

    int huebar_padding = GuiGetStyle(COLORPICKER, HUEBAR_PADDING);
    int huebar_width = GuiGetStyle(COLORPICKER, HUEBAR_WIDTH);

    char *file_path = "out.png";

    GuiSetStyle(DEFAULT, TEXT_SIZE, menu_font_size);

    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, 0xFFFFFFFF);
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED, 0xDDDDDDFF);
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED, 0xFFFFFFFF);

    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, 0x202020FF);
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED, 0x181818FF);
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED, 0x300020FF);

    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, 0xFF0000FF);
    GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED, 0xFF0000FF);

    GuiSetStyle(DEFAULT, BACKGROUND_COLOR, 0x181818FF);


    bool hasImage = false;
    bool forceResize = true;

    bool showGrid = true;
    bool isEditingHexField = false;

    Image img;
    Texture2D buffer;
    float scale = 20;
    Rectangle drawingBounds = {0, 0, GetScreenWidth(), GetScreenHeight()};
    Vector2 image_size = {8, 8};
    Vector2 position;
    Color active_color = FAV_COLOR;

    sprintf(hex_field, "%02X%02X%02X%02X", active_color.r, active_color.g, active_color.b, active_color.a);

    while(!WindowShouldClose()){
        if (IsWindowResized() || forceResize){
            forceResize = false;
            drawingBounds = (Rectangle){menu_width, 0, GetScreenWidth()-menu_width, GetScreenHeight()};
            if (hasImage){
                scale = floor(MIN(drawingBounds.width / image_size.x, drawingBounds.height / image_size.y));
                position = (Vector2){drawingBounds.width - image_size.x*scale, drawingBounds.height - image_size.y*scale};
                position = Vector2Scale(position, 0.5);
                position = Vector2Add((Vector2){drawingBounds.x, drawingBounds.y}, position);
            }
        }
        if (hasImage){
            // set pixel
            Rectangle image_bounds = {position.x, position.y, image_size.x*scale, image_size.y*scale};
            if(IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), image_bounds)){
                Vector2 pixel = Vector2Scale(Vector2Subtract(GetMousePosition(), (Vector2){image_bounds.x, image_bounds.y}), 1/scale);
                UpdateTextureRec(buffer, (Rectangle){pixel.x, pixel.y, 1, 1}, &active_color);
            }
            // shortcuts
            if (!isEditingHexField){
                if(IsKeyPressed(KEY_S)) save_texture_as_image(buffer, file_path);
                if(IsKeyPressed(KEY_G)) showGrid = !showGrid;
            }
        }

        BeginDrawing();
        ClearBackground(FAV_COLOR);
    
        if (!hasImage){
            Color background_color = (Color){0xFF, 0, 0, 0xFF};
            img = GenImageColor(image_size.x, image_size.y, background_color);
            scale = floor(MIN(drawingBounds.width / image_size.x, drawingBounds.height / image_size.y));
            ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
            buffer = LoadTextureFromImage(img);
            assert(buffer.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
            hasImage = true;
            forceResize = true;
        }
        DrawTextureEx(buffer, position, 0, scale, WHITE);

        // draw grid
        if(showGrid && scale >= 3){
            for (int i = 1; i < image_size.x; i++){
                DrawLineEx((Vector2){position.x + i*scale, position.y}, (Vector2){position.x + i*scale, position.y + image_size.y*scale}, 1, BLACK);
            }
            for (int j = 1; j < image_size.y; j++){
                DrawLineEx((Vector2){position.x, position.y + j*scale}, (Vector2){position.x + image_size.x*scale, position.y + j*scale}, 1, BLACK);
            }
        }

        // draw menu
        DrawLine(menu_width, 0, menu_width, GetScreenHeight(), GRAY);

        int color_picker_y = 50;
        int color_picker_size = menu_content_width - huebar_width - huebar_padding;
        GuiColorPicker((Rectangle){menu_padding, color_picker_y, color_picker_size, color_picker_size}, "heyyyy", &active_color);

        if (!isEditingHexField){
            sprintf(hex_field, "%02X%02X%02X%02X", active_color.r, active_color.g, active_color.b, active_color.a);
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
                    sscanf(hex_field, "%02X%02X%02X%02X", (unsigned int*)&active_color.r, (unsigned int*)&active_color.g, (unsigned int*)&active_color.b, (unsigned int*)&active_color.a);
                }
            }
        }

        int options_y = color_picker_size + color_picker_y + 2*huebar_padding + menu_font_size + 20;

        GuiCheckBox((Rectangle){menu_padding, options_y + 0*(huebar_padding+menu_font_size), menu_font_size, menu_font_size}, "grid", &showGrid);

        if(GuiButton((Rectangle){menu_padding, GetScreenHeight() - menu_padding - menu_font_size-5, menu_content_width, menu_font_size+5}, "save (s)")){
            save_texture_as_image(buffer, file_path);
        }
        EndDrawing();
    }

    UnloadImage(img);
    UnloadTexture(buffer);
    CloseWindow();
}

