#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#define MAX_REGIONS 1000
#define MAX_COLORS 4
#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 800
#define MENU_HEIGHT 100

typedef enum{
    MAIN_MENU, GAME_SCREEN, SETTINGS_SCREEN
}Status_menu;

typedef struct NeighborNode {
    int region_id;
    struct NeighborNode* next;
} NeighborNode;

typedef struct {
    int id;
    int color;
    int pixel_count;
    NeighborNode* neighbors;
    bool is_colored;
} Region;

typedef struct StackNode {
    int x, y;
    struct StackNode* next;
} StackNode;

typedef struct {
    SDL_Surface* surface;
    SDL_Surface* original_surface; 
    Region* regions;
    int reg_count;
    int width;
    int height;
    unsigned int* pixels;
    int* reg_map; 
} Map;

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
TTF_Font* font = NULL;
SDL_Texture* m_m_bg = NULL;
SDL_Texture* set_bg = NULL;
Map current_map;
bool is_coloring = false;
bool realtime_coloring = false;
int curr_map_index = 0;
char map_files[51][256];
int total_maps = 0;
int curr_color_region = 0;
unsigned int start_total_time = 0, start_alg_time = 0, end_alg_time = 0;
int color_used_final = 0;
Status_menu screen = MAIN_MENU;

SDL_Color colors[MAX_COLORS] = {
    {255, 0, 0, 255},
    {0, 255, 0, 255},
    {0, 0, 255, 255},
    {255, 255, 0, 255} 
};

bool init_SDL();
bool load_backgrounds();
void timing_results(char* map_name);
void log_message(char* message);
void log_format(char* format, ...);
void cleanup();
bool load_map_files();
bool load_map(const char* filename);
void find_regions();
void build_adjacency_graph();
int greedy_coloring();
void render_map();
void render_settings();
void show_main_menu();
void render_menu();
void render_text(const char* text, int x, int y, SDL_Color color);
bool is_black_pixel(unsigned int pixel);
void color_region_pixels(int region_id, SDL_Color color);
void save_colored_map(const char* filename);
void reset_map_colors();
bool step_coloring();
void push(StackNode** top, int x, int y);
bool pop(StackNode** top, int* x, int* y);
void flood_fill_iterative(int start_x, int start_y, int region_id, unsigned int* visited);
bool add_neighbor(Region* region, int neighbor_id);

int main(int argc, char* argv[]) {
    FILE* log_file = fopen("log.txt", "w");
    if (log_file) {
        fclose(log_file);
    }

    if(argc == 3){
        printf("=== The map coloring program ===\n");
        printf("INPUT_FN  - name of the BMP input file\n");
        printf("OUTPUT_FN - the name of the output file for the colored map\n");

        const char* input_filename = argv[1];
        const char* output_filename = argv[2];

        printf("Input file: %s\n", input_filename);
        printf("Output file: %s\n", output_filename);

        if (!init_SDL()) {
            log_message("Initialization error SDL!\n");
            return 1;
        }

        log_message("Uploading a map...\n");
        if (!load_map(input_filename)) {
            log_format("File upload error %s!\n", input_filename);
            cleanup();
            return 1;
        }
        log_format("The map has been uploaded successfully (%dx%d pixels)\n\n", current_map.width, current_map.height);

        log_message("Search for areas...\n");
        find_regions();
        log_format("Areas found: %d\n\n", current_map.reg_count);

        build_adjacency_graph();

        bool quit = false;

        start_total_time = SDL_GetTicks();
        start_alg_time = SDL_GetTicks();

        SDL_Event event;
        while (!quit) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
                    quit = true;
                }
            }

            
            if (!step_coloring()) {
                log_message("Coloring progress: 100%%\n");
                color_used_final = greedy_coloring();
                end_alg_time = SDL_GetTicks();
            }

            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, current_map.surface);
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, NULL, NULL);
            SDL_RenderPresent(renderer);
            SDL_DestroyTexture(texture);

            SDL_Delay(50);
        }

        save_colored_map(output_filename);

        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);

        double total_time = (SDL_GetTicks() - start_total_time) / 1000.0;
        double coloring_time = (end_alg_time - start_alg_time) / 1000.0;

        printf("\n=== RESULTS ===\n");
        printf("Number of colors: %d\n", color_used_final);
        printf("Total working time: %.3f сек\n", total_time);
        printf("Operating time of the coloring algorithm: %.3f сек\n", coloring_time);
        printf("The result is saved to a file: %s\n", output_filename);

        cleanup();

    }else{
        if (!init_SDL()) {
            log_message("ERROR: Failed to initialize SDL!\n");
            return 1;
        }

        if (!load_backgrounds()) {
            log_message("WARNING: Failed to load background images!\n");
        }

        if (!load_map_files()) {
            log_message("ERROR: Failed to load map files!\n");
            cleanup();
            return 1;
        }

        if (total_maps > 0) {
            log_format("[LOG] Attempting to load map: %s\n", map_files[0]);
            if (!load_map(map_files[0])) {
                log_message("WARNING: Failed to load map\n");
            }
        }
        bool quit = false;
        unsigned int last_step_time = 0;
        unsigned int STEP_DELAY = 100;

        while (!quit) {

            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) {
                    quit = true;
                }

                switch(screen) {
                    case MAIN_MENU:
                        if (e.type == SDL_MOUSEBUTTONDOWN) {
                            int mouse_X, mouse_Y;
                            SDL_GetMouseState(&mouse_X, &mouse_Y);
                        
                            if (mouse_X >= WINDOW_WIDTH/2 - 100 && mouse_X <= WINDOW_WIDTH/2 + 100) {
                                if (mouse_Y >= 220 && mouse_Y < 270) {
                                    screen = GAME_SCREEN;
                                    if (total_maps > 0 && current_map.reg_count == 0) {
                                        load_map(map_files[0]);
                                    }
                                }
                                else if (mouse_Y >= 290 && mouse_Y < 340) {
                                    screen = SETTINGS_SCREEN;
                                }
                                else if (mouse_Y > 360 && mouse_Y < 410) {
                                    quit = true;
                                }
                            }
                        }
                        break;

                    case SETTINGS_SCREEN:
                        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
                            screen = MAIN_MENU;
                        }
                        
                        break;

                    case GAME_SCREEN:
                        if (e.type == SDL_KEYDOWN) {
                            switch (e.key.keysym.sym) {
                                case SDLK_ESCAPE:
                                    screen = MAIN_MENU;
                                    break;
                                case SDLK_LEFT:
                                    if (curr_map_index > 0 && !is_coloring) {
                                        curr_map_index--;
                                        load_map(map_files[curr_map_index]);
                                    }
                                    break;
                                case SDLK_RIGHT:
                                    if (curr_map_index < total_maps - 1 && !is_coloring) {
                                        curr_map_index++;
                                        load_map(map_files[curr_map_index]);
                                    }
                                    break;
                                case SDLK_SPACE:
                                    if (!is_coloring && current_map.reg_count > 0) {
                                        is_coloring = true;
                                        realtime_coloring = true;
                                        curr_color_region = 0;
                                    
                                        start_total_time = SDL_GetTicks();
                                        reset_map_colors();
                                        start_alg_time = SDL_GetTicks();
                                    }
                                    break;
                                case SDLK_i:
                                    if (!is_coloring && current_map.reg_count > 0) {
                                        is_coloring = true;
                                        realtime_coloring = false;
                                        curr_color_region = 0;

                                        start_total_time = SDL_GetTicks();
                                        reset_map_colors();
                                        start_alg_time = SDL_GetTicks();

                                        color_used_final = greedy_coloring();
                                        end_alg_time = SDL_GetTicks();
                                        is_coloring = false;

                                        timing_results(map_files[curr_map_index]);
                                    }
                                    break;
                                case SDLK_r:
                                    if (!is_coloring) {
                                        reset_map_colors();
                                        curr_color_region = 0;
                                    }
                                    break;
                                case SDLK_s:
                                    if (current_map.reg_count > 0 && !is_coloring) {
                                        char output_filename[256];
                                        sprintf(output_filename, "output_maps/colored_map_%d.bmp", curr_map_index + 1);
                                        save_colored_map(output_filename);
                                    }
                                    break;
                            }
                        }
                        break;
                }
            }

            if (screen == GAME_SCREEN && realtime_coloring && is_coloring) {
                unsigned int current_time = SDL_GetTicks();
                if (current_time - last_step_time >= STEP_DELAY) {
                    if (!step_coloring()) {
                        log_message("Coloring progress: 100%%\n");
                        end_alg_time = SDL_GetTicks();

                        is_coloring = false;
                        realtime_coloring = false;

                        timing_results(map_files[curr_map_index]);
                    }
                    last_step_time = current_time;
                }
            }
        
            SDL_SetRenderDrawColor(renderer, 230, 230, 230, 255); 
            SDL_RenderClear(renderer);

            switch(screen) {
            case MAIN_MENU:
                show_main_menu();
                break;
            case GAME_SCREEN:
                render_map();
                render_menu();
                break;
            case SETTINGS_SCREEN:
                render_settings();
                break;
            }

            SDL_RenderPresent(renderer);
            SDL_Delay(16); 
            
        }
        double total_time = (SDL_GetTicks() - start_total_time) / 1000.0;
        log_format("Total working time: %.3f сек\n", total_time);

        cleanup();

    }
    
    return 0;
}

bool init_SDL() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        log_format("  ERROR: SDL could not initialize! SDL Error: %s\n", SDL_GetError());
        
        return false;
    }
    log_message("[LOG]   SDL Video initialized OK\n");

    
    int img_flags = IMG_INIT_PNG | IMG_INIT_JPG;
    if (!(IMG_Init(img_flags) & img_flags)) {
        log_format("  WARNING: SDL_image could not initialize! IMG Error: %s\n", IMG_GetError());
        log_message("  Program will work with BMP files only\n");
        
    } else {
        log_message("[LOG]   SDL_image initialized OK\n");
    }

    if (TTF_Init() == -1) {
        log_format("  WARNING: SDL_ttf could not initialize! TTF Error: %s\n", TTF_GetError());
    } else {
        log_message("[LOG]   SDL_ttf initialized OK\n");
    }
    
    window = SDL_CreateWindow("Map Coloring", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS);
    SDL_WarpMouseInWindow(window, 10, 10);
    if (window == NULL) {
        log_format("  ERROR: Window could not be created! SDL Error: %s\n", SDL_GetError());
        
        return false;
    }
    log_message("[LOG]   Window created OK\n");
    
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        log_message("  WARNING: Hardware accelerated renderer could not be created! Trying software...\n");
        
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        if (renderer == NULL) {
            log_format("  ERROR: Renderer could not be created! SDL Error: %s\n", SDL_GetError());
            
            return false;
        }
        log_message("[LOG]   Software renderer created OK\n");
        
    } else {
        log_message("[LOG]   Hardware accelerated renderer created OK\n");
        
    }
    
    char temp[256];
    char* basePath = SDL_GetBasePath();
    sprintf(temp, "%sfonts/ttf/arial.ttf", basePath);
    font = TTF_OpenFont(temp, 16);
    if (!font) {
        log_message("[LOG]   System font not found, trying local font...\n");
        
        font = TTF_OpenFont("arial.ttf", 16);
        if (font == NULL) {
            log_format("Error SDL : %s.\n", TTF_GetError());
            return false;
        } else {
            log_message("[LOG]   Font loaded OK (local)\n");
        }
    } else {
        log_message("[LOG]   Font loaded OK\n");
    }
    
    return true;
}

bool load_backgrounds() {
    SDL_Surface* bg_surface = IMG_Load("background/ds3.png");
    if (!bg_surface) {
        log_message("Failed to load main menu background!\n");
        return false;
    }
    m_m_bg = SDL_CreateTextureFromSurface(renderer, bg_surface);
    SDL_FreeSurface(bg_surface);

    bg_surface = IMG_Load("background/cat.png");
    if (!bg_surface) {
        log_message("Failed to load settings background!\n");
        return false;
    }
    set_bg = SDL_CreateTextureFromSurface(renderer, bg_surface);
    SDL_FreeSurface(bg_surface);

    return true;
}

void timing_results(char* map_name) {
    double coloring_time = (end_alg_time - start_alg_time) / 1000.0;

    log_format("=== RESULTS for %s ===\n", map_name);
    log_format("Number of colors: %d\n", color_used_final);
    log_format("Operating time of the coloring algorithm: %.3f сек\n", coloring_time);
}

void log_message(char* message) {
    if (!message) return;

    FILE* log_file = fopen("log.txt", "a");
    if (!log_file) {
        return;
    }

    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char time_tamp[20];
    strftime(time_tamp, sizeof(time_tamp), "%Y-%m-%d %H:%M:%S", tm_info);
    fprintf(log_file, "%s - %s\n", time_tamp, message);
    
    fclose(log_file);
}

void log_format(char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    log_message(buffer);
}

void cleanup() {
    if (current_map.surface) {
        SDL_FreeSurface(current_map.surface);
    }

    if (current_map.original_surface) {
        SDL_FreeSurface(current_map.original_surface);
    }

    for (int i = 0; i < current_map.reg_count; i++) {
        NeighborNode* current = current_map.regions[i].neighbors;
        while (current) {
            NeighborNode* temp = current;
            current = current->next;
            free(temp);
        }
    }

    if (current_map.reg_map) {
        free(current_map.reg_map);
    }

    if (font) {
        TTF_CloseFont(font);
    }

    if (m_m_bg) {
        SDL_DestroyTexture(m_m_bg);
    }

    if (set_bg) {
        SDL_DestroyTexture(set_bg);
    }

    if (renderer) {
        SDL_DestroyRenderer(renderer);
    }

    if (window) {
        SDL_DestroyWindow(window);
    }

    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}

bool load_map_files() {

    const char* test_files[] = {
        "maps/map1.bmp", "maps/map2.bmp", "maps/map3.bmp",
        "maps/map4.bmp", "maps/map5.bmp", "maps/map6.bmp",
        "maps/map7.bmp", "maps/map8.bmp", "maps/map9.bmp",
        "maps/map10.bmp", "maps/map11.bmp", "maps/map12.bmp",
        "maps/map13.bmp", "maps/map14.bmp", "maps/map15.bmp",
        "maps/map16.bmp", "maps/map17.bmp", "maps/map18.bmp",
        "maps/map19.bmp", "maps/map20.bmp", "maps/map21.bmp",
        "maps/map22.bmp", "maps/map23.bmp", "maps/map24.bmp",
        "maps/map25.bmp", "maps/map26.bmp", "maps/map27.bmp",
        "maps/map28.bmp", "maps/map29.bmp", "maps/map30.bmp"
    };

    total_maps = sizeof(test_files) / sizeof(test_files[0]);
    for (int i = 0; i < total_maps; i++) {
        strcpy(map_files[i], test_files[i]);
    }

    return true;
}

bool load_map(const char* filename) {
    log_format("  Loading map: %s\n", filename);
    
    if (current_map.surface) {
        SDL_FreeSurface(current_map.surface);
        current_map.surface = NULL;
    }
    if (current_map.original_surface) {
        SDL_FreeSurface(current_map.original_surface);
        current_map.original_surface = NULL;
    }
    if (current_map.regions) {
        for (int i = 0; i < current_map.reg_count; i++) {
            if (current_map.regions[i].neighbors) {
                free(current_map.regions[i].neighbors);
                current_map.regions[i].neighbors = NULL;
            }
        }
        free(current_map.regions);
        current_map.regions = NULL;
    }
    if (current_map.reg_map) {
        free(current_map.reg_map);
        current_map.reg_map = NULL;
    }

    memset(&current_map, 0, sizeof(Map));

    SDL_Surface* loaded_surface = NULL;
    loaded_surface = IMG_Load(filename);
    
    if (!loaded_surface) {
        log_format("SDL_image failed: %s. Attempting to load as BMP via SDL_LoadBMP: %s\n", IMG_GetError(), filename);
        loaded_surface = SDL_LoadBMP(filename);
        if (loaded_surface) {
            log_message("Image loaded successfully as BMP\n");
        }
    }

    current_map.surface = SDL_ConvertSurfaceFormat(loaded_surface, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(loaded_surface); 
    if (!current_map.surface) {
        log_format("ERROR: Failed to convert loaded surface to ARGB8888! SDL Error: %s\n", SDL_GetError());
        return false;
    }
    

    if (!current_map.surface) {
        log_message("ERROR: Failed to create or load any surface!\n");
        return false;
    }

    current_map.original_surface = SDL_ConvertSurface(current_map.surface, current_map.surface->format, 0);
    if (!current_map.original_surface) {
        log_format("ERROR: Failed to create original_surface copy! SDL Error: %s\n", SDL_GetError());
        SDL_FreeSurface(current_map.surface);
        current_map.surface = NULL;
        return false;
    }

    current_map.width = current_map.surface->w;
    current_map.height = current_map.surface->h;
    current_map.pixels = (unsigned int*)current_map.surface->pixels;
    current_map.reg_map = malloc(current_map.width * current_map.height * sizeof(int));
    if (!current_map.reg_map) {
        log_format("    ERROR: Failed to allocate memory for reg_map! Map dimensions: %dx%d\n", current_map.width, current_map.height);
        if (current_map.surface) {
            SDL_FreeSurface(current_map.surface);
            current_map.surface = NULL;
        }
        if (current_map.original_surface) {
            SDL_FreeSurface(current_map.original_surface);
            current_map.original_surface = NULL;
        }
        return false;
    }
    
    for (int i = 0; i < current_map.width * current_map.height; i++) {
        current_map.reg_map[i] = -1;
    }

    current_map.regions = calloc(MAX_REGIONS, sizeof(Region));
    if (!current_map.regions) {
        log_message("ERROR: Failed to allocate memory for regions!\n");
        if (current_map.surface) {
            SDL_FreeSurface(current_map.surface);
            current_map.surface = NULL;
        }
        if (current_map.original_surface) {
            SDL_FreeSurface(current_map.original_surface);
            current_map.original_surface = NULL;
        }
        if (current_map.reg_map) {
            free(current_map.reg_map);
            current_map.reg_map = NULL;
        }
        return false;
    }

    find_regions();
    log_format("Found %d regions\n", current_map.reg_count);
    
    build_adjacency_graph();

    return true;
}

void reset_map_colors() {
    if (current_map.original_surface && current_map.surface) {
        SDL_BlitSurface(current_map.original_surface, NULL, current_map.surface, NULL);
        
        for (int i = 0; i < current_map.reg_count; i++) {
            current_map.regions[i].color = -1;
            current_map.regions[i].is_colored = false;
        }
    }

    color_used_final = 0;
}

bool step_coloring() {
    if (curr_color_region >= current_map.reg_count) {
        return false;
    }

    bool used_colors[MAX_COLORS] = {false};

    NeighborNode* neighbor = current_map.regions[curr_color_region].neighbors;
    while (neighbor) {
        int neighbor_id = neighbor->region_id;

        if (current_map.regions[neighbor_id].color != -1) {
            used_colors[current_map.regions[neighbor_id].color] = true;
        }

        neighbor = neighbor->next;
    }

    for (int color = 0; color < MAX_COLORS; color++) {
        if (!used_colors[color]) {
            current_map.regions[curr_color_region].color = color;
            current_map.regions[curr_color_region].is_colored = true;

            if (color > color_used_final) {
                color_used_final = color;
            }
            break;
        }
    }

    color_region_pixels(curr_color_region, colors[current_map.regions[curr_color_region].color]);

    curr_color_region++;

    if (curr_color_region >= current_map.reg_count) {
        color_used_final++; 
        return false; 
    }

    return true;
}

bool is_black_pixel(unsigned int pixel) {//--------new
    SDL_Color color;
    SDL_GetRGB(pixel, current_map.surface->format, &color.r, &color.g, &color.b);
    int brightness = (color.r + color.g + color.b) / 3;
    return brightness < 50;
}

void find_regions() {
    log_format("[LOG]   Map dimensions: %dx%d\n", current_map.width, current_map.height);

    unsigned int* visited = calloc(current_map.width * current_map.height, sizeof(unsigned int));
    if (!visited) {
        log_message("[LOG]   ERROR: Failed to allocate memory for visited array!\n");
        return;
    }

    current_map.reg_count = 0;

    for (int y = 0; y < current_map.height && current_map.reg_count < MAX_REGIONS; y++) {
        for (int x = 0; x < current_map.width && current_map.reg_count < MAX_REGIONS; x++) {
            int index = y * current_map.width + x;

            if (!visited[index] && !is_black_pixel(current_map.pixels[index])) {

                int region_id = current_map.reg_count;

                current_map.regions[region_id].id = region_id;
                current_map.regions[region_id].color = -1;
                current_map.regions[region_id].is_colored = false;

                current_map.regions[region_id].neighbors = NULL;

                current_map.regions[region_id].pixel_count = 0;

                flood_fill_iterative(x, y, region_id, visited);

                current_map.reg_count++;
            }
        }
    }

    free(visited);
}

void push(StackNode** top, int x, int y) {
    StackNode* new_node = malloc(sizeof(StackNode));
    if (!new_node) {
        return;
    }
    new_node->x = x;
    new_node->y = y;
    new_node->next = *top;
    *top = new_node;
}

bool pop(StackNode** top, int* x, int* y) {
    if (*top == NULL) return false;
    StackNode* temp = *top;
    *x = temp->x;
    *y = temp->y;
    *top = temp->next;
    free(temp);
    return true;
}

void flood_fill_iterative(int start_x, int start_y, int region_id, unsigned int* visited) {//--------new
    StackNode* stack = NULL;
    push(&stack, start_x, start_y);

    int pixels_processed = 0;

    while (stack) {
        int x, y;
        if (!pop(&stack, &x, &y)) break;

        if (x < 0 || x >= current_map.width || y < 0 || y >= current_map.height) continue;

        int index = y * current_map.width + x;
        if (visited[index] || is_black_pixel(current_map.pixels[index])) continue;

        int left = x;
        while (left >= 0 && !visited[y * current_map.width + left] && !is_black_pixel(current_map.pixels[y * current_map.width + left])) {
            visited[y * current_map.width + left] = 1;
            current_map.reg_map[y * current_map.width + left] = region_id;
            current_map.regions[region_id].pixel_count++;
            pixels_processed++;
            left--;
        }
        left++;

        int right = x + 1;
        while (right < current_map.width && !visited[y * current_map.width + right] && !is_black_pixel(current_map.pixels[y * current_map.width + right])) {
            visited[y * current_map.width + right] = 1;
            current_map.reg_map[y * current_map.width + right] = region_id;
            current_map.regions[region_id].pixel_count++;
            pixels_processed++;
            right++;
        }

        for (int scan_x = left; scan_x < right; scan_x++) {
            if (y - 1 >= 0 && !visited[(y - 1) * current_map.width + scan_x] && !is_black_pixel(current_map.pixels[(y - 1) * current_map.width + scan_x])) {
                push(&stack, scan_x, y - 1);
            }
            if (y + 1 < current_map.height && !visited[(y + 1) * current_map.width + scan_x] && !is_black_pixel(current_map.pixels[(y + 1) * current_map.width + scan_x])) {
                push(&stack, scan_x, y + 1);
            }
        }

        if (pixels_processed > 100000) break;
    }

    while (stack) {
        StackNode* temp = stack;
        stack = stack->next;
        free(temp);
    }
}

void build_adjacency_graph() {//--------new
    if (current_map.reg_count == 0) {
        log_message("WARNING: No regions found, skipping graph building\n");
        return;
    }

    long long added_number_regions = 0;

    const int width  = current_map.width;
    const int height = current_map.height;
    const int* map   = current_map.reg_map;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int r1 = map[y * width + x];
            if (r1 == -1) continue;

            int scan_x = x + 1;
            while (scan_x < width) {
                int r2 = map[y * width + scan_x];
                if (r2 == r1) break;
                if (r2 != -1) {
                    if (r1 < current_map.reg_count && r2 < current_map.reg_count) {
                        if (add_neighbor(&current_map.regions[r1], r2)) added_number_regions++;
                        if (add_neighbor(&current_map.regions[r2], r1)) added_number_regions++;
                    }
                    break;
                }
                scan_x++;
            }

            int scan_y = y + 1;
            while (scan_y < height) {
                int r2 = map[scan_y * width + x];
                if (r2 == r1) break;
                if (r2 != -1) {
                    if (r1 < current_map.reg_count && r2 < current_map.reg_count) {
                        if (add_neighbor(&current_map.regions[r1], r2)) added_number_regions++;
                        if (add_neighbor(&current_map.regions[r2], r1)) added_number_regions++;
                    }
                    break;
                }
                scan_y++;
            }
        }
    }

    log_format("Added adjacencies: %lld\n", added_number_regions);
}


bool add_neighbor(Region* region, int neighbor_id) {
    if (!region) return false;

    NeighborNode* current = region->neighbors;
    while (current) {
        if (current->region_id == neighbor_id) {
            return false;
        }
        current = current->next;
    }

    NeighborNode* new_node = malloc(sizeof(NeighborNode));
    if (!new_node) {
        log_format("ERROR: Failed to allocate NeighborNode for region %d\n", region->id);
        return false;
    }

    new_node->region_id = neighbor_id;
    new_node->next = region->neighbors;
    region->neighbors = new_node;

    return true; 
}

int greedy_coloring() {
    int max_color = 0;
    color_used_final = 0;
    
    for (int i = 0; i < current_map.reg_count; i++) {
        bool used_colors[MAX_COLORS] = {false};
        
        NeighborNode* neighbor = current_map.regions[i].neighbors;
        while (neighbor) {
            int neighbor_id = neighbor->region_id;

            if (neighbor_id >= 0 && neighbor_id < current_map.reg_count && current_map.regions[neighbor_id].is_colored && current_map.regions[neighbor_id].color != -1) {
                used_colors[current_map.regions[neighbor_id].color] = true;
            }
            neighbor = neighbor->next;
        }
        
        int chosen_color = -1;
        for (int j = 0; j < MAX_COLORS; j++) {
            if (!used_colors[j]) {
                current_map.regions[i].color = j;
                current_map.regions[i].is_colored = true;
                chosen_color = j;
                if (j > max_color) {
                    max_color = j;
                }
                break;
            }
        }
        if(chosen_color == -1) {
            current_map.regions[i].color = 0;
            current_map.regions[i].is_colored = true;
        }
        
        color_region_pixels(i, colors[current_map.regions[i].color]);
    }
    
    color_used_final = max_color + 1;
    
    return color_used_final;
}

void color_region_pixels(int region_id, SDL_Color color) {
    if (!current_map.surface) {
        log_message("[LOG]   ERROR: current_map.surface is NULL!\n");
        return;
    }
    
    if (!current_map.reg_map) {
        log_message("[LOG]   ERROR: current_map.reg_map is NULL!\n");
        return;
    }
    
    if (!current_map.pixels) {
        log_message("[LOG]   ERROR: current_map.pixels is NULL!\n");
        return;
    }
    
    unsigned int pixel_color = SDL_MapRGB(current_map.surface->format, color.r, color.g, color.b);
    
    int expected_pixels = current_map.regions[region_id].pixel_count;
    
    int pixels_colored = 0;
    int map_size = current_map.width * current_map.height;
    
    for (int i = 0; i < map_size && pixels_colored < expected_pixels; i++) {
        if (current_map.reg_map[i] == region_id) {
            current_map.pixels[i] = pixel_color;
            pixels_colored++;
        }
    }
    
}

void render_text(const char* text, int x, int y, SDL_Color color) {
    if (!font) return;
    
    SDL_Surface* text_surface = TTF_RenderText_Solid(font, text, color);
    if (text_surface) {
        SDL_Texture* text_texture = SDL_CreateTextureFromSurface(renderer, text_surface);
        if (text_texture) {
            SDL_Rect dest_rect = {x, y, text_surface->w, text_surface->h};
            SDL_RenderCopy(renderer, text_texture, NULL, &dest_rect);
            SDL_DestroyTexture(text_texture);
        }
        SDL_FreeSurface(text_surface);
    }
}

void render_map() {
    if (!current_map.surface) return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, current_map.surface);
    if (texture) {
        SDL_Rect dest_rect = {50, MENU_HEIGHT + 50, WINDOW_WIDTH - 100, WINDOW_HEIGHT - MENU_HEIGHT - 100};
        SDL_RenderCopy(renderer, texture, NULL, &dest_rect);
        SDL_DestroyTexture(texture);
    }

    if (current_map.reg_count > 0) {
        char info_text[256];
        sprintf(info_text, "Map %d/%d | Regions: %d", curr_map_index + 1, total_maps, current_map.reg_count);
        SDL_Color white = {0, 0, 0, 255};
        render_text(info_text, WINDOW_WIDTH - 1150, MENU_HEIGHT + 10, white);

        if (realtime_coloring && is_coloring) {
            sprintf(info_text, "Colored: %d/%d", curr_color_region, current_map.reg_count);
            render_text(info_text, WINDOW_WIDTH - 950, MENU_HEIGHT + 10, white);
            
            int progress = (curr_color_region * 100) / current_map.reg_count;
            sprintf(info_text, "Progress: %d%%", progress);
            render_text(info_text, WINDOW_WIDTH - 825, MENU_HEIGHT + 10, white);
        }

        if (color_used_final > 0 && !is_coloring) {
            sprintf(info_text, "Colors used: %d", color_used_final);
            SDL_Color green = {0, 0, 0, 255};
            render_text(info_text, WINDOW_WIDTH - 250, MENU_HEIGHT + 10, green);
        }
    }
}

void render_settings(){
    if (set_bg) {
        SDL_RenderCopy(renderer, set_bg, NULL, NULL);
    }

    SDL_Color text_color = {150, 0, 0, 255};
    render_text("Author 25", WINDOW_WIDTH/2 + 120, 15, text_color);
    render_text("Map Coloring Game Controls:", WINDOW_WIDTH/2 + 130, 40, text_color);
    render_text(" SPACE - Dynamic Coloring", WINDOW_WIDTH/2 + 130, 70, text_color);
    render_text(" I - Instant Coloring", WINDOW_WIDTH/2 + 150, 100, text_color);
    render_text(" ESC - Return to Main Menu", WINDOW_WIDTH/2 + 130, 130, text_color);
}

void show_main_menu() {
    if (m_m_bg) {
        SDL_RenderCopy(renderer, m_m_bg, NULL, NULL);
    }

    SDL_Color but_color = {70, 70, 70, 50};
    SDL_Color hover_color = {100, 100, 100, 100};
    SDL_Color text_color = {255, 255, 255, 255};

    int but_w = 200;
    int but_h = 50;
    int but_x = WINDOW_WIDTH / 2 - but_w / 2;
    int but_spacing = 70;

    render_text("MAP COLORING",  but_x + (but_w - 100) / 2, 25, (SDL_Color){64, 224, 208, 255});
    
    int mouse_x, mouse_y;
    SDL_GetMouseState(&mouse_x, &mouse_y);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    SDL_Rect start_but = {but_x, 220, but_w, but_h};
    bool active_start = (mouse_x >= start_but.x && mouse_x <= start_but.x + but_w && mouse_y >= start_but.y && mouse_y <= start_but.y + but_h);

    SDL_SetRenderDrawColor(renderer, active_start ? hover_color.r : but_color.r, active_start ? hover_color.g : but_color.g, active_start ? hover_color.b : but_color.b, active_start ? hover_color.a : but_color.a);

    SDL_RenderFillRect(renderer, &start_but);

    render_text("START GAME", but_x + (but_w - 100) / 2, 235, text_color);

    SDL_Rect settings_but = {but_x, 220 + but_spacing, but_w, but_h};
    bool active_settings = (mouse_x >= settings_but.x && mouse_x <= settings_but.x + but_w && mouse_y >= settings_but.y && mouse_y <= settings_but.y + but_h);

    SDL_SetRenderDrawColor(renderer, active_settings ? hover_color.r : but_color.r, active_settings ? hover_color.g : but_color.g, active_settings ? hover_color.b : but_color.b, active_settings ? hover_color.a : but_color.a);

    SDL_RenderFillRect(renderer, &settings_but);

    render_text("SETTINGS", but_x + (but_w - 80) / 2, 235 + but_spacing, text_color);

    SDL_Rect exit_but = {but_x, 220 + 2 * but_spacing, but_w, but_h};
    bool active_exit = (mouse_x >= exit_but.x && mouse_x <= exit_but.x + but_w && mouse_y >= exit_but.y && mouse_y <= exit_but.y + but_h);

    SDL_SetRenderDrawColor(renderer, active_exit ? hover_color.r : but_color.r, active_exit ? hover_color.g : but_color.g, active_exit ? hover_color.b : but_color.b, active_exit ? hover_color.a : but_color.a);

    SDL_RenderFillRect(renderer, &exit_but);

    render_text("EXIT", but_x + (but_w - 40) / 2, 235 + 2 * but_spacing, text_color);

}

void render_menu() {
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
    SDL_Rect menu_rect = {0, 0, WINDOW_WIDTH, MENU_HEIGHT};
    SDL_RenderFillRect(renderer, &menu_rect);

    SDL_Color button_color = {100, 100, 100, 255};
    SDL_Color text_color = {255, 255, 255, 255};

    SDL_SetRenderDrawColor(renderer, button_color.r, button_color.g, button_color.b, 255);
    SDL_Rect prev_button = {20, 20, 80, 30};
    SDL_RenderFillRect(renderer, &prev_button);
    render_text("<- Prev", 30, 27, text_color);

    SDL_Rect next_button = {110, 20, 80, 30};
    SDL_RenderFillRect(renderer, &next_button);
    render_text("Next ->", 120, 27, text_color);

    SDL_Rect color_button = {200, 20, 140, 30};
    SDL_RenderFillRect(renderer, &color_button);
    render_text("SPACE: Color", 210, 27, text_color);

    SDL_Rect instant_button = {350, 20, 120, 30};
    SDL_RenderFillRect(renderer, &instant_button);
    render_text("I: Instant", 360, 27, text_color);

    SDL_Rect reset_button = {480, 20, 80, 30};
    SDL_RenderFillRect(renderer, &reset_button);
    render_text("R: Reset", 490, 27, text_color);

    SDL_Rect save_button = {570, 20, 100, 30};
    SDL_RenderFillRect(renderer, &save_button);
    render_text("S: Save", 580, 27, text_color);

    if (is_coloring) {
        SDL_Color red = {255, 0, 0, 255};
        if (realtime_coloring) {
            render_text("DYNAMIC COLORING", 680, 25, red);
        } else {
            render_text("PROCESSING...", 680, 25, red);
        }
    } else {
        SDL_Color green = {0, 255, 0, 255};
        render_text("READY", 680, 25, green);
    }

    render_text("ESC: Exit  |  Left / Right: Maps  |  SPACE: Dynamic  |  I: Instant  |  R: Reset  |  S: Save", 10, 70, text_color);
}

void save_colored_map(const char* filename) {
    if (current_map.surface) {
        if (SDL_SaveBMP(current_map.surface, filename) != 0) {
            log_format("Ошибка сохранения файла %s: %s\n", filename, SDL_GetError());
        }
    }
} 