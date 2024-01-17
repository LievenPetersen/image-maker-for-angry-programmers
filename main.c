#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "external/raylib/src/raylib.h"
#include "external/raylib/src/raymath.h"

#define RAYGUI_IMPLEMENTATION
#include "external/raygui.h"

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

bool save_texture_as_image(Texture2D tex, const char *path){
    Image result = LoadImageFromTexture(tex);

    bool success = IsImageReady(result)
        && ExportImage(result, path);

    UnloadImage(result);

    if (!success){
        printf("Error while saving image!\n");
    }
    return success;
}

bool IsSupportedImageFormat(const char *filePath){
    const char *VALID_EXTENSIONS = ".png;.bmp;.qoi;.raw"; //.tga;.jpg;.jpeg"; // .tga and .jpg don't work for some reason
    return IsFileExtension(filePath, VALID_EXTENSIONS);
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

// replaces the existing texture and unloads it. Returns true on success.
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

Color HSVToColor(Vector3 hsv, unsigned char alpha){
    Color color = ColorFromHSV(hsv.x, hsv.y, hsv.z);
    color.a = alpha;
    return color;
}

void SetWindowTitleImage(const char *image_path){
    char title[MAX_NAME_FIELD_SIZE + 100];
    sprintf(title, "%s - Image maker for angry programmers", image_path);
    SetWindowTitle(title);
}

void toggleTool(enum CURSOR_MODE *cursor, enum CURSOR_MODE tool){
    if (*cursor != tool) *cursor = tool;
    else *cursor = CURSOR_DEFAULT;
}

void imageColorFlood(Image *image, Vector2 source_pixel, Color new_color){
    Color old_color = GetImageColor(*image, source_pixel.x, source_pixel.y);

    Color *pixels = (Color*)image->data;
    size_t pixel_count = image->height * image->width;
    size_t end_ptr = 0;
    size_t next_ptr = 0;
    size_t *queue = malloc(4 * pixel_count * sizeof(*queue));
    bool *visited = malloc(pixel_count * sizeof(*visited));
    if (queue == NULL || visited == NULL){
        perror("unable to allocate sufficient memory\n");
        free(queue);
        free(visited);
        return;
    }

    memset(visited, 0, pixel_count * sizeof(*visited));

    size_t source_pixel_idx = (int)source_pixel.x + (int)source_pixel.y * image->width;
    queue[end_ptr++] = source_pixel_idx;

    while(end_ptr != next_ptr){
        if (end_ptr == 4*pixel_count - 1){
            free(queue);
            free(visited);
            return;
        }

        size_t pixel_idx = queue[next_ptr++];
        pixels[pixel_idx] = new_color;

        if (pixel_idx >= 1){
            size_t west = pixel_idx - 1;
            if (memcmp(&pixels[west], &old_color, sizeof(old_color)) == 0){
                if (!visited[west]){
                    queue[end_ptr++] = west;
                    visited[west] = true;
                }
            }
        }
        if (pixel_idx < pixel_count - 1){
            size_t east = pixel_idx + 1;
            if (memcmp(&pixels[east], &old_color, sizeof(old_color)) == 0){
                if (!visited[east]){
                    queue[end_ptr++] = east;
                    visited[east] = true;
                }
            }
        }
        if (pixel_idx < pixel_count - image->width){
            size_t south = pixel_idx + image->width;
            if (memcmp(&pixels[south], &old_color, sizeof(old_color)) == 0){
                if (!visited[south]){
                    queue[end_ptr++] = south;
                    visited[south] = true;
                }
            }
        }
        if (pixel_idx >= (size_t)image->width){
            size_t north = pixel_idx - image->width;
            if (memcmp(&pixels[north], &old_color, sizeof(old_color)) == 0){
                if (!visited[north]){
                    queue[end_ptr++] = north;
                    visited[north] = true;
                }
            }
        }
    }

    free(queue);
    free(visited);
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

    Texture2D buffer = {0};
    Vector2 image_size = {0};
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
        buffer = loadImageAsTexture(&img);
        image_size.x = img.width;
        image_size.y = img.height;
        if (strnlen(argv[1], MAX_NAME_FIELD_SIZE) == MAX_NAME_FIELD_SIZE){
            name_field[MAX_NAME_FIELD_SIZE-1] = 0; // brutal approach to make string fit.
            // TODO: more graceful solution, that doesn't rip out the postfix.
        }
        sprintf(name_field, "%s", argv[1]);
        SetWindowTitleImage(name_field);

        hasImage = true;
        UnloadImage(img);
    }

    // generate standard 8x8 image
    if (!hasImage){
        image_size = (Vector2){8, 8};
        Image img = GenImageColor(image_size.x, image_size.y, STD_COLOR);
        buffer = loadImageAsTexture(&img);
        sprintf(name_field, "out.png");

        hasImage = true;
        UnloadImage(img);
    }
    sprintf(name_field_old, "%s", name_field);

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

    float scale = 0;
    Vector2 position = {0};

    Rectangle drawingBounds = {0, 0, GetScreenWidth(), GetScreenHeight()};

    // Track hsv + alpha instead of rgba,
    // because rgba can only store lossy hue values which leads to jitters when color approaches white or black.
    Vector3 active_hsv = ColorToHSV(FAV_COLOR);
    unsigned char alpha = FAV_COLOR.a;
    enum CURSOR_MODE cursor = CURSOR_DEFAULT;

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
        // name field can overlap with the canvas
        if (hasImage && !isEditingNameField){
            // detect canvas click
            Rectangle image_bounds = {position.x, position.y, image_size.x*scale, image_size.y*scale};
            if(IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), image_bounds)){
                Vector2 pixel = Vector2Scale(Vector2Subtract(GetMousePosition(), (Vector2){image_bounds.x, image_bounds.y}), 1/scale);
                // set pixel
                if (cursor == CURSOR_DEFAULT){
                    Color active_color = HSVToColor(active_hsv, alpha);
                    UpdateTextureRec(buffer, (Rectangle){pixel.x, pixel.y, 1, 1}, &active_color);
                }
                // pick color with pipette (only on press, not continuously)
                if (cursor == CURSOR_PIPETTE && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
                    cursor = CURSOR_DEFAULT;
                    Image canvas_content = LoadImageFromTexture(buffer); // canvas content could be loaded as soon as pipette mode activates, if responsiveness is an issue.
                    Color pixel_color = GetImageColor(canvas_content, pixel.x, pixel.y);
                    active_hsv = ColorToHSV(pixel_color);
                    alpha = pixel_color.a;
                    UnloadImage(canvas_content);
                }
                // TODO: disable set pixel on button down, if a tool was pressed. (to avoid set pixel on tool clicks)
                // color fill
                else if (cursor == CURSOR_COLOR_FILL && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
                    cursor = CURSOR_DEFAULT;
                    Image canvas_content = LoadImageFromTexture(buffer); // canvas content could be loaded as soon as pipette mode activates, if responsiveness is an issue.
                    Color active_color = HSVToColor(active_hsv, alpha);
                    imageColorFlood(&canvas_content, pixel, active_color);
                    setTextureToImage(&buffer, &canvas_content);
                    UnloadImage(canvas_content);
                }
            }
            // shortcuts
            if (!isEditingHexField){
                if(IsKeyPressed(KEY_S) && (IsKeyDown(KEY_LEFT_CONTROL)||IsKeyDown(KEY_RIGHT_CONTROL))) save_texture_as_image(buffer, name_field);
                if(IsKeyPressed(KEY_G)) showGrid = !showGrid;
                if(IsKeyPressed(KEY_P)) toggleTool(&cursor, CURSOR_PIPETTE);
                if(IsKeyPressed(KEY_F)) toggleTool(&cursor, CURSOR_COLOR_FILL);
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

        float bar_hue = active_hsv.x; // The color panel should only affect saturation & value. Thus hue is buffered.
        GuiColorPanelHSV((Rectangle){menu_padding, color_picker_y, color_picker_size, color_picker_size}, "heyyyy", &active_hsv);
        GuiColorBarHue((Rectangle){menu_padding + color_picker_size + huebar_padding, color_picker_y, huebar_width, color_picker_size}, "oiii", &bar_hue);
        active_hsv.x = bar_hue;

        // hex code text box
        if (!isEditingHexField){
            Color color = HSVToColor(active_hsv, alpha);
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
                    active_hsv = ColorToHSV(new_color);
                    alpha = new_color.a;
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
        DrawText("x", menu_padding + menu_content_width + huebar_padding - menu_font_size ,options_y + (item++)*(huebar_padding+menu_font_size), menu_font_size, WHITE);

        // y resize textbox
        Rectangle y_box = {menu_padding, options_y + item*(huebar_padding+menu_font_size), menu_content_width - menu_font_size, menu_font_size};
        res = dimension_text_box(y_box, y_field, DIM_STRLEN, image_size.y, &isEditingYField);
        if (res > 0){
            image_size.y = res;
            forceWindowResize = true;
            Image img = LoadImageFromTexture(buffer);
            ImageResizeCanvasOwn(&img, image_size.x, image_size.y, 0, 0, STD_COLOR);
            setTextureToImage(&buffer, &img);
            UnloadImage(img);
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
                if (IsSupportedImageFormat(name_field) 
                    && DirectoryExists(GetDirectoryPath(name_field))){
                    // accept new name
                        memcpy(name_field_old, name_field, strlen(name_field)+1);
                        SetWindowTitleImage(name_field);
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
            save_texture_as_image(buffer, name_field);
        }

        EndDrawing();
    }

    UnloadTexture(buffer);
    CloseWindow();
}

