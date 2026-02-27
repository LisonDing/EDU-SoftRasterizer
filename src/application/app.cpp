#include "application/app.h"
#include "X11/X.h"
#include "X11/Xlib.h"
#include "core/gl.h"
#include "core/model.h"
#include "math/geometry.h"
#include "platform/mac_window.h"
#include "platform/tgaimage.h"
#include "shaders/shader.h"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <cmath>
#include <vector>

Application::Application(int w, int h, std::vector<Model*>& m):width(w),height(h),models(m),angle(0.0f) {
    // Initialize window & image memory
    window = new AppWindow (width , height, "SoftRenderer - Refactored");
    framebuffer = new TGAImage(width, height, TGAImage::RGBA);
    zbuffer = new float[width * height];

    // Initialize normal buffer
    normal_buffer = new vec3[width * height];

    // Initialize shadow buffer
    shadow_buffer = new TGAImage(width,height,TGAImage::RGBA);
    shadow_zbuffer = new float[width * height];

    // // Loading model
    // model = new Model(model_path);

    // Initialize camera position
    eye = vec3 (0,0,3); //相机对looking方向的距离
    center = vec3 (0,0,0); // 相机看向原点
    up = vec3 (0,1,0); // 相机头顶朝上
    
    // Initialize global state
    viewport(0, 0, width, height);
    projection(-1.f / (eye - center).norm());

    // Initialize Uniforms
    // uniforms.model = model;
    uniforms.eye = eye;

    // Setting lights
    // 给一个暗淡的基础亮度，让背光面不是死黑
    vec3 ambient_color = vec3(0.5f, 0.7f, 1.0f);
    float ambient_strength = 0.8f;
    uniforms.ambient = ambient_color * ambient_strength; 

    // B. 添加多光源 (Multi-Lights)
    // 光源 1: 主光 (从右前方射入)
    uniforms.lights.push_back(vec3(1, 1, 1).normalize());
    uniforms.light_color.push_back(vec3(1.0f, 0.8f, 0.6f)*1.0f);
    // // // // 光源 2: 补光 (从左侧射入，模拟边缘光)
    uniforms.lights.push_back(vec3(-1, 0, -1).normalize());
    uniforms.light_color.push_back(vec3(0.0f, 0.6f, 1.0f)*0.6f);

}

Application::~Application() {
    for (auto m : models) {
        delete m;
    }
    delete [] zbuffer;
    delete framebuffer;
    delete window;

    delete shadow_buffer;
    delete [] shadow_zbuffer;

    delete [] normal_buffer;
}

void Application::process_input() {
    // Handling Rotary Knobs
    if (window->keys['A'] || window->keys['a']) angle -= 2.0f; // left
    if (window->keys['D'] || window->keys['d']) angle += 2.0f; // right
}

void Application::update_matrix() {
    // Reset view matrix
    lookat(eye, center, up);

    // Calculate the rotation matrix
    mat<4,4> Rot = mat<4,4>::identity();
    float rad = angle * M_PI / 180.f;
    Rot[0][0] = cos(rad); Rot[0][2] = sin(rad);
    Rot[2][0] = -sin(rad); Rot[2][2] = cos(rad);

    // Apply rotation to ModelView
    ModelView = ModelView * Rot;

    // Synchronize to Uniforms
    uniforms.ModelView = ModelView;
    uniforms.Projection = Projection;
    uniforms.Viewport = Viewport;
    uniforms.eye = eye;
}

void Application::clear_buffer() {
    // Clea Z-buffer & framebuffer
    for (int i=0; i<width*height; i++) zbuffer[i] = -std::numeric_limits<float>::max();
    // framebuffer->clear(); 

    // background color
    TGAColor bg_color(20, 80, 210, 255);
    for (int x=0; x<width; x++) {
        for (int y=0; y<height; y++) {
        
            framebuffer->set(x, y, bg_color);
        }
    }

    // clear normal buffer
    for (int i=0; i<width*height; i++) {
        normal_buffer[i] = vec3(0,0,0); 
    }

}

void Application::draw_scene() {


    for (Model* m : models) {
        uniforms.model = m;

        // Instantiate shader 此处实例化shader 可以对不同模型进行判断
        // FlatShader Shader(&uniforms);
        // GouraudShader Shader(&uniforms);
        PhongShader Shader(&uniforms);
        // DepthShader Shader(&uniforms);
        // ToonShader Shader(&uniforms);
        // Draw loop
        for (int i=0; i< m->nfaces(); i++) {
            // 裁剪空间坐标
            vec4 clip_coords[3];
            // // 【Flat Shading 特有步骤】: 在 CPU 计算面法线
            // vec3 n = cross(model->vert(i, 1)-model->vert(i, 0), 
            //                model->vert(i, 2)-model->vert(i, 0)).normalize();
            // Shader.intensity = std::max(0.f, n * uniforms.light_dir);   

            for (int j=0; j<3; j++) {
                // 调用顶点着色器 所有的几何变换逻辑都在 Shader.vertex 里发生
                clip_coords[j] = Shader.vertex(i, j);
                
            }
            // 调用光栅化器 (自动调用片元着色器)
            triangle(clip_coords, Shader, *framebuffer, zbuffer);
        }
    }
}

// shadow map pass
void Application::draw_shadow_map() {

    // 调试 灯光是否存在
    // if (uniforms.lights.empty()) {
    //     std::cerr << "No light source defined!" << std::endl;
    //     return;
    // }
    // clean  shadow buffer
    shadow_buffer->clear();
    for (int i= 0; i<width*height; i++) {
        shadow_zbuffer[i] = -std::numeric_limits<float>::max();
    }

    // setting light
    vec3 light_pos = uniforms.lights[0]; //main light position

    // 调试 打印光源位置
    // std::cerr << "Light position: " << light_pos.x << ", " << light_pos.y << ", " << light_pos.z << std::endl;

    vec3 light_eye = light_pos * 3.0f; 
    vec3 center(0,0,0);
    vec3 up(0,1,0);

    // updating martix
    lookat(light_eye, center, up);
    viewport(0, 0, width, height);
    // projection(0); // shadow 》 Orthographics projection
    projection(-1.f / (light_eye -center).norm());

    // Save transport matrix
    M_shadow = Viewport * Projection * ModelView;

    // Render loop
    uniforms.ModelView = ModelView;
    uniforms.Projection = Projection;
    uniforms.Viewport = Viewport;
    uniforms.eye = eye;
    for (Model* m : models) {
        uniforms.model = m;
        DepthShader Shader(&uniforms);
        for (int i= 0 ; i<m->nfaces(); i++) {
            vec4 clip_coords[3];
            for (int j=0; j<3; j++) {
                clip_coords[j]=Shader.vertex(i, j);
            }

            // 调试 输出顶点坐标
            // if (i== 0) {
            //     std::cerr << "Face 0 clip coords: " << std::endl;
            //     for (int k=0; k<3; k++) {
            //         std::cerr << "Vertex " << k << ": (" << clip_coords[k][0] << ", " << clip_coords[k][1] << ", " << clip_coords[k][2] << ", " << clip_coords[k][3] << ")" << std::endl;
            //     }
            // }
            triangle(clip_coords, Shader, *shadow_buffer, shadow_zbuffer);
        }
    
    }


}

// SSAO (screen space ambient occlusion) pass
void Application::ssao_pass() {
    std::vector<float> ao_map(width * height, 1.0f); //初始化为1.0f
   // 采样半径 阴影扩散量
   float radius = 10.0f;
   float bias = 15.0f; //偏移 防止自遮挡
   const int sample_count = 16; //采样数量 (cpu 瓶颈)
   float angle_limit = 20.0f; //角度限制 避免过度采样
   // 遍历每个像素
   for (int y = 0; y < height; y++) {
       for (int x = 0; x < width; x++) {
           int idx = x + y * width;
           int current_z = zbuffer[idx];
        //    if (current_z < -99999.0f) continue; //跳过无效像素

           float occlusion = 0.0f;
           int samples = 0;
           
           // 环绕采样
           for (int i= 0; i < sample_count; i++) {
                // 随机采样 
                
                float angle = (float)rand() / RAND_MAX * 2.0f * M_PI;
                // 采样坐标
                int sample_x = x + (int)(radius * cos(angle));
                int sample_y = y + (int)(radius * sin(angle));
                // 边界检查
                if (sample_x >= 0 && sample_x < width && sample_y >= 0 && sample_y < height) {
                    float sample_z = zbuffer[sample_x + sample_y * width];

                    // if ((zbuffer[x + y * width]) < -99999.0f) continue;
                    
                    // 深度差计算
                    float diff = sample_z - current_z;
                    if (diff > bias && diff < angle_limit) {
                        // 遮蔽率增加
                        // float intensity = 1.0f - (diff / angle_limit);
                        // occlusion += intensity;
                   
                        occlusion += 1.0f;
                    }
                   
                }
            }
            ao_map[idx] = occlusion / sample_count;
            // 计算遮蔽率
            // float ao = 1.0f - (occlusion / (float)samples);
            // ao = std::pow(ao, 2.0f); // 调整对比度
            // // 应用到颜色缓冲
            // TGAColor color = framebuffer->get(x, y);
            // color.bgra[0] = std::min(255.f, color.bgra[0] * ao); // B
            // color.bgra[1] = std::min(255.f, color.bgra[1] * ao); // G
            // color.bgra[2] = std::min(255.f, color.bgra[2] * ao); // R

            // framebuffer->set(x, y, color);  
        }
    }
    // 降噪 box blur
    int blur_size = 2; // 5x5 core
    for (int y = 0; y < height; y++) {
        for (int x = 0 ; x < width; x++) {
            float result = 0.0f;
            int count = 0;
            for (int dy = -blur_size; dy <= blur_size; dy++) {
                for (int dx = -blur_size; dx <= blur_size; dx++) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                        result += ao_map[nx + ny * width];
                        count++;
                    }
                }
            }
            float avg_ao = result / count;
            // 应用到颜色缓冲

            float visibility = 1.0f - avg_ao; // 可见度
            avg_ao = std::pow(avg_ao, 1.5f); // 调整对比度
            TGAColor color = framebuffer->get(x, y);
            color.bgra[0] = std::min(255.f, color.bgra[0] * visibility); // B
            color.bgra[1] = std::min(255.f, color.bgra[1] * visibility); // G
            color.bgra[2] = std::min(255.f, color.bgra[2] * visibility); // R
            framebuffer->set(x, y, color);
        
        }
    }
    
}

void Application::outline_pass() {
    // Sobel 边缘检测 控制深度差异
    // float depth_threshold = 8.0f; // 深度差异阈值
    float normal_threshold = 2.0f; // 法线差异阈值
    // Sobel 核
    float Gx[3][3] = {
        {-1, 0, 1},
        {-2, 0, 2},
        {-1, 0, 1}
    };
    float Gy[3][3] = {
        {1, 2, 1},      
        {0, 0, 0},
        {-1, -2, -1}
    };
    TGAColor outline_color(0, 0, 0, 255); // 黑色边缘

    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            // float depth_center = zbuffer[x + y * width];
            // if (depth_center < -99999.0f) continue; //跳过无效像素
            

            // // 重建法线
            // float r_z = zbuffer[(x + 1) + y * width];
            // float d_z = zbuffer[x + (y + 1) * width];
            // vec3 v1(1.0f, 0.0f, r_z - depth_center);
            // vec3 v2(0.0f, 1.0f, d_z - depth_center);
            // vec3 n_normal = cross(v1, v2).normalize();
            // // 简化右下法线检测
            // float rr_z = zbuffer[(x - 2) + y * width]; // 右边第二个像素
            // float rd_z = zbuffer[x + (y - 2) * width]; // 下边第二个像素
            // vec3 v1_r(1.0f, 0.0f, rr_z - r_z);
            // vec3 v2_d(0.0f, 1.0f, rd_z - r_z);
            // vec3 n_normal_r = cross(v1_r, v2_d).normalize();
            // // 计算法线差异 dot
            // float normal_diff = n_normal * n_normal_r;
            // bool is_depth_edge = false;
            // 计算深度梯度
            // float gx =0.0f;
            // float gy =0.0f;

            // // 3x3 Sobel 核
            // for (int dy = -1 ; dy <=1; dy++) {
            //     for (int dx = -1; dx<=1; dx++) {
            //         // 获取邻域深度
            //         float neighbor_depth = zbuffer[(x + dx) + (y + dy) * width];
            //         // if (neighbor_depth < -99999.0f) neighbor_depth = depth_center; //无效深度使用中心深度
            //         // sobel x kernel
            //         int kx= (dx == -1) ? -1 : (dx == 1) ? 1 : 0;
            //         // sobel y kernel
            //         int ky= (dy == -1) ? -1 : (dy == 1) ? 1 : 0;
            //         gx += neighbor_depth * kx;
            //         gy += neighbor_depth * ky;
            //     }
            
            // }
            // // 计算梯度幅值
            // float gradient = std::sqrt(gx * gx + gy * gy);

            int idx = x + y * width;
            if (zbuffer[idx] < -99999.0f) continue; //跳过无效像素

            // vec3 n_center = normal_buffer[idx];
            // float z_center = zbuffer[idx];
            // bool is_edge = false;

            // 计算深度梯度
            vec3 grad_x(0.0f, 0.0f, 0.0f);
            vec3 grad_y(0.0f, 0.0f, 0.0f);
            

            // 3x3 Sobel 核
            for (int dy = -1 ; dy <=1; dy++) {
                for (int dx = -1; dx<=1; dx++) {
                   int nx = x + dx;;
                   int ny = y + dy;
                   vec3 n = normal_buffer[nx + ny * width];

                   grad_x = grad_x + n * Gx[dx+1][dy+1];
                   grad_y = grad_y + n * Gy[dx+1][dy+1];
                }
            
            }
            // 计算梯度幅值
            float gradient = std::sqrt(grad_x * grad_x + grad_y * grad_y);


            // bool is_normal_edge = (normal_diff < normal_threshold);
            // 判断是否为边缘
            // if (gradient > depth_threshold) {
            // if (is_depth_edge || is_normal_edge) {
            if (gradient > normal_threshold) {
                // 设置边缘颜色
                // framebuffer->set(x, y, outline_color);
                
                // // 宽度控制
                // float stroke_width = 0.5f; //边缘宽度
                // for (int oy = -stroke_width; oy <= stroke_width; oy++) {
                //     for (int ox = -stroke_width; ox <= stroke_width; ox++) {
                //         int nx = x + ox;;
                //         int ny = y + oy;
                //         if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                //             framebuffer->set(nx, ny, outline_color);
                //         }
                //     }
                // }
                // // 混合alpha
                // TGAColor original_color = framebuffer->get(x, y);
                // float alpha = 0.7f; //边缘透明度
                // for (int i = 0; i < 3; i++) {
                //     original_color.bgra[i] = (unsigned char)(outline_color.bgra[i] * alpha + original_color.bgra[i] * (1.f - alpha));
                //     original_color.bgra[3] = 255;
                // }
                // framebuffer->set(x, y, original_color);

                // stroke radius
                float stroke_radius = 1.0f; //边缘半径
                // stroke color
                // TGAColor line_color(20, 20, 20, 255); //黑色
                // 遍历包围盒
                int bound = (int)std::ceil(stroke_radius);
                for (int oy = -bound; oy <= bound; oy++) {
                    for (int ox = -bound; ox <= bound; ox++) {
                        int nx = x + ox;;
                        int ny = y + oy;
                        if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                            float dist = std::sqrt(ox * ox + oy * oy);
                            if (dist > stroke_radius) continue;
                            // 计算alpha
                            float d = dist / stroke_radius;
                            float alpha = 1.0f - (d * d * (3.0f - 2.0f * d)); // smoothstep
                            // edge blending
                            float edge_strength = std::min(1.0f, (gradient - normal_threshold) / 5.0f);
                            alpha *= edge_strength;
                            alpha = std::min(1.0f, std::max(0.0f, alpha)* 1.5f);  

                            // 混合alpha
                            TGAColor original_color = framebuffer->get(nx, ny);
                            for (int i = 0; i < 3; i++) {
                                original_color.bgra[i] = (unsigned char)(outline_color.bgra[i] * alpha + original_color.bgra[i] * (1.f - alpha));
                                original_color.bgra[3] = 255;
                            }
                            framebuffer->set(nx, ny, original_color);
                        }
                    }
                }
            }
        }
    }

    // 调试 输出
    // for (int y = 0; y < height; y++) {
    //     for (int x = 0; x < width; x++) {
    //        vec3 n = normal_buffer[x + y * width];
    //        int r = (n.x +1.0f) * 0.5f * 255.0f;
    //        int g = (n.y +1.0f) * 0.5f * 255.0f;
    //        int b = (n.z +1.0f) * 0.5f * 255.0f;
    //        framebuffer->set(x, y, TGAColor (r, g, b, 255));
    //     }
    // }
}
// void Application::run() {
//     // Main loop
//     while (window->is_running()) {
//         process_input();
//         clear_buffer();
//         update_matrix();
//         draw_scene();
//         window->draw_buffer(framebuffer->buffer());
//     }
// }

// new run loop
void Application::run() {
    // Main loop
    while (window->is_running()) {
        uniforms.width = width;
        uniforms.height = height;
        process_input();

        // // Pass 1 : shadow map
        
        draw_shadow_map();
        // window->draw_buffer(shadow_buffer->buffer());
        // continue;

        // Pass 2 : render
        clear_buffer();
        update_matrix();

        // uniforms.width = width;
        // uniforms.height = height;
        uniforms.shadow_map = shadow_zbuffer;
        uniforms.M_shadow = M_shadow;

        draw_scene();
        // window->draw_buffer(shadow_buffer->buffer());

        // Pass 3 : SSAO
        ssao_pass();
        uniforms.normal_buffer = normal_buffer;
        // Pass 4 : outline
        outline_pass();
        
        window->draw_buffer(framebuffer->buffer());
    }
}