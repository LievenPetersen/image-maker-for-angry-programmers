#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#include <raylib.h>
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include <raymath.h>

#define MIN(a, b) (a<b? (a) : (b))
#define MAX(a, b) (a>b? (a) : (b))

#define FAV_COLOR ((Color){0x18, 0x18, 0x18, 0xFF}) // sorry, but AA is a bit impractical
#define STD_COLOR ((Color){0xFF, 0x00, 0x00, 0xFF})

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

// returns > 0  on resize
long dimension_text_box(const Rectangle bounds, char *str_value, const size_t str_size, long reset_value, bool *isEditing){
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

// changes image and texture to PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
Texture2D loadImageAsTexture(Image *img){
    ImageFormat(img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    Texture2D buffer = LoadTextureFromImage(*img);
    assert(buffer.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    return buffer;
}

bool setTextureToImage(Texture2D *texture, Image *img){
    Texture2D new_buffer = loadImageAsTexture(img);
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
    // fill new pixels with color
    // filling order around old image (o):
    // 111
    // 3o4
    // 222
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

int main(int argc, char **argv){

    // draw loading screen
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1000, 800, "Image maker for angry programmers");
    SetTargetFPS(60);
    BeginDrawing();
    char *loading_text = "loading...";
    int loadingFontSize = 20;
    DrawText(loading_text, (GetScreenWidth()-MeasureText(loading_text, loadingFontSize))/2, GetScreenHeight()/2, loadingFontSize, WHITE);
    EndDrawing();

    // -- initialize application --

    Texture2D buffer = {0};
    Vector2 image_size = {0};
    bool hasImage = false;

    // load image from provided path
    if (argc == 2){
        Image img = LoadImage(argv[1]);
        if (!IsImageReady(img)){
            printf("Error: failed to load image from '%s'\n", argv[1]);
            UnloadImage(img);
            return 1;
        }
        buffer = loadImageAsTexture(&img);
        image_size.x = img.width;
        image_size.y = img.height;

        hasImage = true;
        UnloadImage(img);
    }

    // generate standard 8x8 image
    if (!hasImage){
        image_size = (Vector2){8, 8};
        Image img = GenImageColor(image_size.x, image_size.y, STD_COLOR);
        buffer = loadImageAsTexture(&img);
        UnloadImage(img);
        hasImage = true;
    }

    // strings for image resizing textBoxes
    const size_t DIM_STRLEN = 4; // TODO: support 4 digit sizes
    char x_field[DIM_STRLEN];
    char y_field[DIM_STRLEN];
    sprintf(x_field, "%d", (int)image_size.x);
    sprintf(y_field, "%d", (int)image_size.y);

    char hex_field[11];

    // track if any text box is being edited.
    bool isEditingHexField = false;
    bool isEditingXField = false;
    bool isEditingYField = false;

    // menu parameters
    int menu_font_size = 30;
    int menu_width = 200;
    int menu_padding = 20;
    int menu_content_width = menu_width - 2*menu_padding;

    int huebar_padding = GuiGetStyle(COLORPICKER, HUEBAR_PADDING);
    int huebar_width = GuiGetStyle(COLORPICKER, HUEBAR_WIDTH);

    char *file_path = "out.png";

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

    float scale = 0;
    Vector2 position = {0};

    Rectangle drawingBounds = {0, 0, GetScreenWidth(), GetScreenHeight()};

    Color active_color = FAV_COLOR;

    while(!WindowShouldClose()){
        if (IsWindowResized() || forceWindowResize){
            forceWindowResize = false;
            drawingBounds = (Rectangle){menu_width, 0, GetScreenWidth()-menu_width, GetScreenHeight()};
            if (hasImage){
                // TODO support images bigger than drawingBounds
                scale = MAX(1.0f, floor(MIN(drawingBounds.width / image_size.x, drawingBounds.height / image_size.y)));
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

        // draw image
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

        // color picker
        int color_picker_y = menu_padding;
        int color_picker_size = menu_content_width - huebar_width - huebar_padding;
        GuiColorPicker((Rectangle){menu_padding, color_picker_y, color_picker_size, color_picker_size}, "heyyyy", &active_color);
        if (!isEditingHexField){
            sprintf(hex_field, "%02X%02X%02X%02X", active_color.r, active_color.g, active_color.b, active_color.a);
        }

        // hex code text box
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

        // grid checkbox
        GuiCheckBox((Rectangle){menu_padding, options_y + 0*(huebar_padding+menu_font_size), menu_font_size, menu_font_size}, "grid", &showGrid);

        // x resize textbox
        Rectangle x_box = {menu_padding, options_y + 1*(huebar_padding+menu_font_size), menu_content_width - menu_font_size, menu_font_size};
        long res = dimension_text_box(x_box, x_field, DIM_STRLEN, image_size.x, &isEditingXField);
        if (res > 0){
            // TODO: fix flicker on resize? (caused by unloading the currently drawn texture)
            image_size.x = res;
            forceWindowResize = true;
            Image img = LoadImageFromTexture(buffer);
            ImageResizeCanvasOwn(&img, image_size.x, image_size.y, 0, 0, STD_COLOR);
            setTextureToImage(&buffer, &img);
            UnloadImage(img);
        }
        DrawText("x", menu_padding + menu_content_width + huebar_padding - menu_font_size ,options_y + 1*(huebar_padding+menu_font_size), menu_font_size, WHITE);

        // y resize textbox
        Rectangle y_box = {menu_padding, options_y + 2*(huebar_padding+menu_font_size), menu_content_width - menu_font_size, menu_font_size};
        res = dimension_text_box(y_box, y_field, DIM_STRLEN, image_size.y, &isEditingYField);
        if (res > 0){
            image_size.y = res;
            forceWindowResize = true;
            Image img = LoadImageFromTexture(buffer);
            ImageResizeCanvasOwn(&img, image_size.x, image_size.y, 0, 0, STD_COLOR);
            setTextureToImage(&buffer, &img);
            UnloadImage(img);
        }
        DrawText("y", menu_padding + menu_content_width + huebar_padding - menu_font_size ,options_y + 2*(huebar_padding+menu_font_size), menu_font_size, WHITE);

        // save button
        if(GuiButton((Rectangle){menu_padding, GetScreenHeight() - menu_padding - menu_font_size-5, menu_content_width, menu_font_size+5}, "save (s)")){
            save_texture_as_image(buffer, file_path);
        }
        EndDrawing();
    }

    UnloadTexture(buffer);
    CloseWindow();
}

