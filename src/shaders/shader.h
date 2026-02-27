#pragma once
#include "core/gl.h"
#include "core/model.h"
#include "math/geometry.h"
#include "platform/tgaimage.h"
#include <algorithm>
#include <cstdlib>

// =========================================================
//  Uniforms (全局变量容器)
//  模拟 OpenGL 的 Uniform 机制，所有 Shader 共享或单独持有
// =========================================================
struct Uniforms {
    // =========================================================
    //  Uniforms (全局变量容器)
    //  模拟 OpenGL 的 Uniform 机制，所有 Shader 共享或单独持有
    // =========================================================
    int width;
    int height;
    
    
    mat<4,4> ModelView;
    mat<4,4> Projection;
    mat<4,4> Viewport;

    mat<4, 4> M_shadow; // shadow map 变量
    float* shadow_map; // 深度图指针


    vec3 eye;       // 【新增】相机位置，用于计算视线 V
    Model* model;

    // --- 光照系统升级 ---
    vec3 ambient;              // 环境光颜色/强度 (例如 0.1, 0.1, 0.1)
    vec3 light_dir;
    std::vector<vec3> light_color;
    std::vector<vec3> lights;  // 存储所有光源的方向 (指向光源)

    // 法线缓存
    vec3* normal_buffer;
};

    // =========================================================
    //  1. Flat Shader (平面着色)
    //  频率：Per Face (通常在 CPU 端算好面法线，传入 Shader)
    // =========================================================
struct FlatShader : public IShader {
    Uniforms* u;        // 引用 Uniforms
    float intensity;    // 【Uniform】由 CPU 计算后传入 (Per Face)

    // 构造函数注入 Uniforms
    FlatShader(Uniforms* _u) : u(_u), intensity(0) {}

    virtual vec4 vertex(int iface, int nthvert) {
        // 仅处理坐标变换
        vec3 v = u->model->vert(iface, nthvert);
        return u->Viewport * u->Projection * u->ModelView * embed<4>(v);
    }

    virtual bool fragment(vec3 bar, TGAColor &color) {
        // 颜色在整个三角形内是常数 (Flat)
        float val = 255 * intensity;
        color = TGAColor(val, val, val, 255);
        return false;
    }
};

// =========================================================
//  2. Gouraud Shader (顶点光照)
//  频率：Per Vertex (光照计算在 Vertex Shader)
// =========================================================
struct GouraudShader : public IShader {
    Uniforms* u;
    vec3 varying_intensity; // 【Varying】透视插值的光照强度

    GouraudShader(Uniforms* _u) : u(_u) {}

    virtual vec4 vertex(int iface, int nthvert) {
        // A. 坐标变换
        vec3 v = u->model->vert(iface, nthvert);
        vec4 gl_Vertex = u->Viewport * u->Projection * u->ModelView * embed<4>(v);

        // B. 光照计算 (Per Vertex)
        // 注意：真正的 Gouraud 需要顶点法线 (vn)。
        // 由于当前 OBJ 加载器只支持 v，我们暂时用"假法线"或需要在 Model 中计算平均法线。
        // 这里为了演示架构，假设我们能取到法线 n
        // vec3 n = u->model->normal(iface, nthvert); <--- 未来需要实现
        vec3 n = u->model->normal(iface, nthvert);
    
        // 计算 N dot L
        float diff = std::max(0.f, n * u->light_dir);
        varying_intensity[nthvert] = diff; // 写入 varying，等待插值

        return gl_Vertex;
    }

    virtual bool fragment(vec3 bar, TGAColor &color) {
        // C. 颜色计算 (直接使用插值后的亮度)
        float intensity = varying_intensity * bar; // 重心坐标插值
        // 简单的防溢出处理
        intensity = std::max(0.f, std::min(1.f, intensity));
        float val = 255 * intensity;
        color = TGAColor(val, val, val, 255);
        return false;
    }
};

// =========================================================
//  3. Phong Shader (像素光照)
//  频率：Per Fragment (光照计算在 Fragment Shader)
// =========================================================
struct PhongShader : public IShader {
    Uniforms* u;
    mat<3,3> varying_n; // 【Varying】插值法线 (将三个顶点的法线传给片元)
    mat<3,3> varying_tri; // 【新增】三角形顶点坐标(世界空间)，用于插值当前像素位置
    mat<2,3> varying_uv; // 储存三个顶点的uv
    PhongShader(Uniforms* _u) : u(_u) {}

    virtual vec4 vertex(int iface, int nthvert) {
        vec3 v = u->model->vert(iface, nthvert);
        vec4 gl_Vertex = u->Viewport * u->Projection * u->ModelView * embed<4>(v);

        // A. 传递法线 (不计算光照，只传递)
        // vec3 n = u->model->normal(iface, nthvert); 
        vec3 n = u->model->normal(iface, nthvert);
    
        // 将法线变换到世界空间 (这里简化处理，通常需要逆转置矩阵)
        varying_n.set_col(nthvert, n); 
        varying_tri.set_col(nthvert, v);

        // 获取并传递uv坐标
        // vt 坐标插值
        varying_uv.set_col(nthvert, u->model->uv(iface, nthvert));

        return gl_Vertex;
    }

    virtual bool fragment(vec3 bar, TGAColor &color) {
        // B. 法线插值与重归一化
        vec3 bn = (varying_n * bar).normalize(); // 对插值后的法线再次归一化
        vec3 p  = varying_tri * bar;             // 像素位置 P
        // uv 插值
        vec2 uv = varying_uv * bar;

        // TBN tangent space matrix
        // 求纹理坐标变化时 空间坐标的变化方向： 切线T

        // 获取三角形的原始数据
        vec3 p0 = varying_tri.col(0);
        vec3 p1 = varying_tri.col(1);
        vec3 p2 = varying_tri.col(2);
        vec2 uv0 = varying_uv.col(0);
        vec2 uv1 = varying_uv.col(1);
        vec2 uv2 = varying_uv.col(2);
        // 计算边缘增量： 变化量
        vec3 e1 = p1 - p0;
        vec3 e2 = p2 - p0;
        vec2 duv1 = uv1 - uv0;
        vec2 duv2 = uv2 - uv0;
        // 求切线Tangent 和 副切线 Bitangent
        float f = 1.0f / (duv1.x * duv2.y - duv2.x * duv1.y + 1e-6f); // 防除0
        
        vec3 T, B;
        T.x = f * (duv2.y * e1.x - duv1.y * e2.x);
        T.y = f * (duv2.y * e1.y - duv1.y * e2.y);
        T.z = f * (duv2.y * e1.z - duv1.y * e2.z);

        B.x = f * (-duv2.x * e1.x + duv1.y * e2.x);
        B.y = f * (-duv2.x * e1.y + duv1.y * e2.y);
        B.z = f * (-duv2.x * e1.z + duv1.y * e2.z);

        // 正交化 Gram-Schmidt process
        // 确保 T垂直于N（bn） 并归一化
        vec3 N = bn;
        T = (T - N * (N * T)).normalize();
        B = cross(N, T).normalize();

        // 采样法线贴图
        // 此时的n_map 为切线空间（0，0，1） -> (128,128,255) 近似蓝色
        vec3 n_map = u->model->normal(uv);

        // 变换到世界空间
        vec3 n_final = (T * n_map.x + B * n_map.y + N * n_map.z).normalize();

        // 采样纹理&透明度
        TGAColor tex_color = u->model->diffuse(uv);
        float alpha = tex_color.bgra[3] / 255.0f;
         // Alpha test
        if (tex_color.bgra[3] < 5) {
            return true;
        }

        // 采样光泽度
        float gloss_val = u->model->gloss(uv);
        float spec_power = std::pow(2.0f, gloss_val / 10.f) + 100.0f; // 指数映射
        // float spec_power = 150.f;
        // 采样高光强度
        float spec_intensity = u->model->specular(uv) / 1.0f; // 除常量调整亮度
        // 2. 准备光照向量
        // vec3 l = u->light_dir;                   // 光照方向 L
        vec3 v = (u->eye - p).normalize();       // 视线方向 V = Camera - Pixel
        
        // 分离光照累加
        vec3 diff_light(0,0,0);
        vec3 spec_light(0,0,0);
        // vec3 total_intensity = u ->ambient;
        vec3 ambient_light = u ->ambient;
    

        // // shadow map 阴影计算
        // float shadow = 1.0f; // 默认不在阴影中
        // // 计算当前像素在光源空间的坐标
        // vec4 p_shadow = u->M_shadow * embed<4>(p); // 由 varying_tri * bar 计算得到的世界坐标 p
        // p_shadow = p_shadow * (1.0f / p_shadow.w); // 齐次除法
        // // 转换到纹理坐标系
        // int width = std::abs(u->Viewport[0][0] * 2);
        // int height = std::abs(u->Viewport[1][1] * 2);
        // int x = int (p_shadow.x);
        // int y = int (p_shadow.y);
        // int idx = x + y * width; // 计算索引
        // // 深度比较
        // if (x >=0 && x < width && y >= 0 && y < height && idx >=0 && idx < width * height) {
        //     float z_shadow = u->shadow_map[idx];
        //     // // 调试 深度图投影
        //     // color = TGAColor(z_shadow,z_shadow,z_shadow,255);
        //     // return false;
        //     // 调试 可视化深度
        //     // float diff_depth = p_shadow.z - z_shadow;
        //     // if (std::abs(diff_depth) < 5.0f) {
        //     //     color = TGAColor(0,255,0,255);
        //     // } else if (diff_depth < 0) {
        //     //     color = TGAColor(0,0,255,255);
        //     // } else {
        //     //     color = TGAColor(255,0,0,255);
        //     // }
        //     // return false;

        //     // 计算片元深度
        //     float current_depth = p_shadow.z;
        //     // shadow bias 防止自阴影
        //     float bias = 1.5f; // 偏移量 防止自阴影
        //     if (current_depth < z_shadow - bias) {
        //         shadow = 0.3f; // 在阴影中，减少光照
        //     }
        // }

        // soft shadow mapping 阴影计算
        float shadow = 1.0f; // 默认不在阴影中
        vec4 p_shadow = u->M_shadow * embed<4>(p);
        p_shadow = p_shadow * (1.0f / p_shadow.w); // 齐次除法
        int width = u->width;
        int height = u->height;
        // PCF参数
        float visibility = 0.0f; // 可见度累加器
        float bias = 1.5f; // 偏移量 防止自阴影
        int pcf_radius = 2; // 采样半径 1-> 3x3 核心;2->5x5 核心
        int samples = 0; // 采样计数器
        // 循环采样周围像素
        for (int y_offset = -pcf_radius; y_offset <= pcf_radius; y_offset++) {
            for (int x_offset = -pcf_radius; x_offset <= pcf_radius; x_offset++) {
                // 计算领坐标
                int current_x = int (p_shadow.x) + x_offset;
                int current_y = int (p_shadow.y) + y_offset;
                int idx = current_x + current_y * width;
                // 边界检查
                if (current_x >=0 && current_x < width && current_y >= 0 && current_y < height && idx >=0 && idx < width * height) {
                    float z_shadow = u->shadow_map[idx];
                    float current_depth = p_shadow.z;
                    if (current_depth < z_shadow - bias) {
                        visibility += 0.0f; // 在阴影中
                    } else {
                        visibility += 1.0f; // 可见
                    }
                    samples++;
                }
            }
        }
        // 计算平均可见度
        if (samples > 0) {
            visibility /= float(samples); // 归一化    
        } else {
            visibility = 1.0f;
        }
        // 混合光照和环境 保留默认环境亮度
        shadow = 0.3 + 0.7 * visibility; 


        // // 3. 【调用 GL 库】计算 Blinn-Phong 强度
        // float intensity = blinn_phong(bn, l, v);
        // 多光源循环
        for (size_t i = 0; i < u->lights.size(); i++){
            vec3 l = u->lights[i];
            vec3 c = u->light_color[i];

            float diff = 0; 
            float spec = 0;
            blinn_phong(n_final, l, v, diff, spec, spec_power);
            diff_light = diff_light + (c * diff * shadow); // 考虑阴影衰减
            spec_light = spec_light + (c * spec * spec_intensity * shadow);

            // // Diffuse
            // float diff = std::max(0.f, n_final * l);
            // diff_light = diff_light + (c * diff);

            // // Specular
            // vec3 h = (l + v).normalize();
            // float spec = std::pow(std::max(0.f, n_final * h), spec_power);
            // spec_light = spec_light + (c * spec * spec_intensity);

            // 合成颜色

            // 调用 gl.h 中的 blinn_phong 计算当前光源的 (Diffuse + Specular)
            // * 传入n_final
            // float bp = blinn_phong(n_final, l, v, spec_intensity, spec_power);

            // 累加到总亮度
            // 这里假设所有光源都是白光。如果要做彩色光，可以用 lights_color[i] * bp
            // total_intensity = total_intensity + c * bp;
        }

        // 4. Tone Mapping / Clamp (防止过曝)
        // 简单的将 RGB 限制在 [0, 1]
        // for (int i=0; i<3; i++) {
        //     total_intensity[i] = std::min(1.f, std::max(0.f, total_intensity[i]));
        // }

        // // 采样纹理颜色
        // TGAColor tex_color = u->model->diffuse(uv);

        // // Alpha test
        // if (tex_color.bgra[3] < 10) {
        //     return true;
        // }

        // 采样自发光
        TGAColor glow_color = u->model->glow(uv);
        float glow_intensity = 3.0f;
        
        // // C. 光照计算 (Per Pixel)
        // float diff = std::max(0.f, bn * u->light_dir); // 在每个像素点算 N dot L
        // // (可选) 加上高光 Specular (Phong 模型的核心优势)
        // vec3 r = (bn * (diff * 2.f) - u->light_dir).normalize(); // 反射向量
        // float spec = std::pow(std::max(r.z, 0.f), 5.f); // 简单的高光计算
        // float intensity = diff + spec * 0.9f;
        // float intensity = diff; 
        // 防溢出
        // intensity = std::max(0.f, std::min(1.f, intensity));
        // int val = 255 * intensity;

        // color = TGAColor(
        //     total_intensity.x * 255, // R
        //     total_intensity.y * 255, // G
        //     total_intensity.z * 255, // B
        //     255
        // );
        // float r = (ambient_light.x + diff_light.x) * alpha + spec_light.x;
        // float g = (ambient_light.y + diff_light.y) * alpha + spec_light.y;
        // float b = (ambient_light.z + diff_light.z) * alpha + spec_light.z;

        // vec3 diffuse_part = (ambient_light + diff_light) * alpha;
        // vec3 specular_part = spec_light * spec_intensity;
        color = TGAColor(
            std::min(255.f, tex_color[2] * (ambient_light.x + diff_light.x) * alpha + spec_light.x + glow_color[2] * glow_intensity), // B 通道（蓝）
            std::min(255.f, tex_color[1] * (ambient_light.y + diff_light.y) * alpha + spec_light.y + glow_color[1] * glow_intensity), // G 通道（绿）
            std::min(255.f, tex_color[0] * (ambient_light.z + diff_light.z) * alpha + spec_light.z + glow_color[0] * glow_intensity), // R 通道（红）
            tex_color.bgra[3]                                              // A 通道（透明度，不透明）
        );
        // color = TGAColor(
        //     std::min(255.f, (tex_color[2] * diffuse_part.x + specular_part.x * 255.f + spec_light.z + glow_color[2] * glow_intensity)), // B 通道（蓝）
        //     std::min(255.f, (tex_color[1] * diffuse_part.y + specular_part.x * 255.f + spec_light.y + glow_color[1] * glow_intensity)), // G 通道（绿）
        //     std::min(255.f, (tex_color[0] * diffuse_part.z + specular_part.x * 255.f + spec_light.z + glow_color[0] * glow_intensity)), // R 通道（红）
        //     tex_color.bgra[3]                                              // A 通道（透明度，不透明）
        // );

        // 调试 强制输出UV颜色
        // color = TGAColor(uv.x * 255, uv.y * 255, 0, 255);
        
        // // 调试 强制输出 diffuse强度
        // color = TGAColor (255* diff_light.x,255* diff_light.y,255* diff_light.z);
        return false;
    }
};

// =========================================================
//  4. Depth Shader (深度图/Z-Buffer 可视化)
// =========================================================
struct DepthShader : public IShader {
    Uniforms* u;
    vec3 varying_depth; // 【Varying】存储三个顶点的深度值

    DepthShader(Uniforms* _u) : u(_u) {}

    virtual vec4 vertex(int iface, int nthvert) {
        vec3 v = u->model->vert(iface, nthvert);
        // 变换到屏幕空间 (Screen Space)
        // 此时 gl_Vertex 的分量是: 
        // x, y: 屏幕坐标
        // z: 深度值 (0~255, 因为 Viewport 矩阵做了映射)
        // w: 1/z (用于透视校正)
        vec4 gl_Vertex = u->Viewport * u->Projection * u->ModelView * embed<4>(v);
    
        // 【核心】我们需要传递透视除法后的 Z 值
        // 在 gl.cpp 的 triangle 函数里，真正的 z 是 gl_Vertex[2]/gl_Vertex[3]
        // 所以我们在这里预计算一下传给 varying
        varying_depth[nthvert] = gl_Vertex[2] / gl_Vertex[3];

        return gl_Vertex;
    }

    virtual bool fragment(vec3 bar, TGAColor &color) {
        // 插值深度
        float z = varying_depth * bar; 

        // z 已经在 0~255 范围内了 (由 Viewport 决定)
        // 为了视觉效果，可以防止溢出
        // 注意：离相机越近 z 越大还是越小，取决于你的 Projection 矩阵系数 coeff
        // 通常：白色(255)代表近，黑色(0)代表远，或者反过来
        int val = (int)(std::abs(z)+0.0f)*2;
        val =std::max(0, std::min(255, val));
    
        // 输出灰度图
        color = TGAColor(val, val, val, 255);
        // 调试
        // color = TGAColor(255, 255, 255, 255);
        return false;
    }
};

struct ToonShader : public IShader {
    Uniforms* u;
    mat<3,3> varying_n; // 【Varying】插值法线 (将三个顶点的法线传给片元)
    mat<3,3> varying_tri; // 【新增】三角形顶点坐标(世界空间)，用于插值当前像素位置
    mat<2,3> varying_uv; // 储存三个顶点的uv
    ToonShader(Uniforms* _u) : u(_u) {}
    virtual vec4 vertex(int iface, int nthvert) {
        vec3 v = u->model->vert(iface, nthvert);
        vec4 gl_Vertex = u->Viewport * u->Projection * u->ModelView * embed<4>(v);

        // A. 传递法线 (不计算光照，只传递)
        // vec3 n = u->model->normal(iface, nthvert); 
        vec3 n = u->model->normal(iface, nthvert);
    
        // 将法线变换到世界空间 (这里简化处理，通常需要逆转置矩阵)
        varying_n.set_col(nthvert, n); 
        varying_tri.set_col(nthvert, v);

        // 获取并传递uv坐标
        // vt 坐标插值
        varying_uv.set_col(nthvert, u->model->uv(iface, nthvert));

        return gl_Vertex;
    }
    virtual bool fragment(vec3 bar, TGAColor &color) {
        // B. 法线插值与重归一化
        vec3 bn = (varying_n * bar).normalize(); // 对插值后的法线再次归一化
        // 写入 normal buffer       
        vec3 p  = varying_tri * bar;             // 像素位置 P
        vec4 p_clip = u->Projection * u->ModelView * embed<4>(p);
        p_clip = p_clip * (1.0f / p_clip.w); // 齐次除法
        vec4 p_screen = u->Viewport * p_clip; // 屏幕空间坐标
        int x = int (p_screen.x + 0.5f);
        int y = int (p_screen.y + 0.5f);
        int idx = x + y * u->width;
        if (u->normal_buffer != nullptr) {
            if (x >= 0 && x< u->width && y >= 0 && y < u->height) {
                u->normal_buffer[idx] = bn;
            }
        }
        
        // uv 插值
        vec2 uv = varying_uv * bar;

        vec3 l = u->lights[0];                   // 假设单光源

        vec3 v = (u->eye - p).normalize();       // 视线方向 V = Camera - Pixel

        float intensity = std::max(0.f, bn * l);

        // 光照阶梯化 硬切分级
        if (intensity > 0.85f) 
            intensity = 1.0f;
        // else if (intensity > 0.60f) 
        //     intensity = 0.80f;
        else if (intensity > 0.45f) 
            intensity = 0.60f;
        // else if (intensity > 0.30f) 
        //     intensity = 0.40f;
        else 
            intensity = 0.30f;
        

        // shadow 计算
        float shadow = 1.0f; // 默认不在阴影中
        vec4 p_shadow = u->M_shadow * embed<4>(p);
        p_shadow = p_shadow * (1.0f / p_shadow.w); // 齐次除法
        int width = u->width;
        int height = u->height;
        // PCF参数
        float visibility = 0.0f; // 可见度累加器
        float bias = 1.5f; // 偏移量 防止自阴影
        int pcf_radius = 2; // 采样半径 1-> 3x3 核心;2->5x5 核心
        int samples = 0; // 采样计数器
        // 循环采样周围像素
        for (int y_offset = -pcf_radius; y_offset <= pcf_radius; y_offset++) {
            for (int x_offset = -pcf_radius; x_offset <= pcf_radius; x_offset++) {
                // 计算领坐标
                int current_x = int (p_shadow.x) + x_offset;
                int current_y = int (p_shadow.y) + y_offset;
                int idx = current_x + current_y * width;
                // 边界检查
                if (current_x >=0 && current_x < width && current_y >= 0 && current_y < height && idx >=0 && idx < width * height) {
                    float z_shadow = u->shadow_map[idx];
                    float current_depth = p_shadow.z;
                    if (current_depth < z_shadow - bias) {
                        visibility += 0.0f; // 在阴影中
                    } else {
                        visibility += 1.0f; // 可见
                    }
                    samples++;
                }
            }
        }
        // 计算平均可见度
        if (samples > 0) {
            visibility /= float(samples); // 归一化    
        } else {
            visibility = 1.0f;
        }
        // 混合光照和环境 保留默认环境亮度
        shadow = 0.3 + 0.7 * visibility; 

        float ambient_intensity = u->ambient.x; // 假设环境光均匀
        // rim light
        float rim = 1.0f - std::max(0.f, bn * v); // 视线与法线垂直即为边缘
        if (rim > 0.7f && intensity > 0.3f) intensity += 0.3f; // 增强边缘光照
        // 合成颜色
        TGAColor tex_color = u->model->diffuse(uv);
        // float final_light = ambient_intensity + intensity * (shadow > 0.5f ? 1.0f : 0.2f); // 阴影中减弱光照
        float final_light = ambient_intensity + intensity * shadow; 
        final_light = std::min(1.5f, final_light); // 可过曝
        color = TGAColor(
            std::min(255.f, tex_color[2] * final_light), // B       通道（蓝）
            std::min(255.f, tex_color[1] * final_light), // G       通道（绿）
            std::min(255.f, tex_color[0] * final_light), // R       通道（红）
            tex_color.bgra[3]                                              // A 通道（透明度，不透明）
        );
        return false;
    }
};