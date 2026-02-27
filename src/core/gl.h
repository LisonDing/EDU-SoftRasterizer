#pragma once
#include "platform/tgaimage.h"
#include "math/geometry.h"

// 全局矩阵变量 (模拟 OpenGL 的状态机模式)
extern mat<4,4> ModelView;
extern mat<4,4> Projection;
extern mat<4,4> Viewport;

// 矩阵设置函数
void viewport(int x, int y, int w, int h);
void projection(float coeff=0.f); // coeff = -1/c
void lookat(vec3 eye, vec3 center, vec3 up);

// ==========================================
// 可编程着色器接口 (IShader)
// ==========================================
struct IShader {
    virtual ~IShader();
    
    // 顶点着色器：输入第 iface 个面的第 nthvert 个顶点
    // 返回：变换后的屏幕坐标 (4D)
    virtual vec<4> vertex(int iface, int nthvert) = 0;
    
    // 片元着色器：输入重心坐标 bar，输出颜色 color
    // 返回：true 表示要丢弃该像素 (discard)，false 表示保留
    virtual bool fragment(vec3 bar, TGAColor &color) = 0;
};

// 光栅化核心函数
// 注意：现在 triangle 不需要传具体颜色了，它会问 shader 要颜色
void triangle(vec4 *pts, IShader &shader, TGAImage &image, float *zbuffer);

void line(vec2 p0, vec2 p1, TGAColor color, TGAImage &image);

void blinn_phong(vec3 n, vec3 l, vec3 v, float& out_diff, float& out_spec ,float spec_power);
