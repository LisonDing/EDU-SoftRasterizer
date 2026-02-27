#pragma once
#include <vector>
#include "math/geometry.h"
#include "platform/tgaimage.h"
#include "platform/mac_window.h"
#include "core/model.h"
#include "core/gl.h"
#include "shaders/shader.h"

class Application {
public:
    Application(int w, int h, std::vector<Model*>& model);

    ~Application();

    //start app
    void run();
private:
    //system
    int width, height;
    AppWindow* window;
    TGAImage* framebuffer;
    float* zbuffer;
    std::vector<Model*> models;

    //scene data
    vec3 eye;
    vec3 center;
    vec3 up;
    float angle;
    vec3* normal_buffer;

    // Shadow Map
    float* shadow_zbuffer;
    TGAImage* shadow_buffer;

    mat<4, 4> M_shadow; // shadow matrix

    //render data
    Uniforms uniforms;

    // GL method
    void process_input(); // Hardware Interaction
    void update_matrix(); // update MVP
    void clear_buffer();
    void draw_scene(); // render object

    void draw_shadow_map();

    void ssao_pass();
    void outline_pass();
};