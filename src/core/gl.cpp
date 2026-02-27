#include <cmath>
#include <limits>
#include <cstdlib>
#include "core/gl.h"
#include "platform/tgaimage.h"

// 新增：矩阵变换工具箱 (Matrix Utils)
mat<4,4> ModelView;
mat<4,4> Projection;
mat<4,4> Viewport;

// viewport transformation
void viewport (int x , int y, int w, int h) {
    Viewport = mat<4, 4>::identity();
    // 把原点从 (0,0) 移到屏幕中心 (x + w/2, y + h/2)
    Viewport[0][3]= x + w/2.f;
    Viewport[1][3]= y + h/2.f;
    Viewport[2][3]=255.f/2.f; // 深度映射
    // 线性变换
    Viewport[0][0]= w/2.f;
    Viewport[1][1]= - h/2.f; //这里可能涉及 Y 轴翻转，先按标准写，后面如果倒了再改负号
    Viewport[2][2]= 255.f/2.f;
}
//projection transformation
void projection(float coeff) {
    Projection = mat<4,4>::identity();
    // make w -> (1 + coeff * z)
    Projection[3][2] = coeff;
}
//view transformation
void lookat(vec3 eye, vec3 center, vec3 up) {
    vec3 z = (eye - center).normalize();
    // x (u): up 叉乘 z (右手定则，指向右侧)
    vec3 x = cross(up ,z).normalize();
    // y (v): z 叉乘 x (指向真正头顶上方)
    vec3 y = cross(z,x).normalize();

    mat<4, 4> Minv = mat<4, 4>::identity();
    mat<4, 4>Tr = mat<4, 4>::identity();

    for (int i=0; i<3; i++) {
        // 旋转（基向量投影）
        Minv[0][i]=x[i];
        Minv[1][i]=y[i];
        Minv[2][i]=z[i];
        // 平移（移动世界坐标）
        Tr[i][3]= -eye[i];
    }

    ModelView = Minv * Tr;
}

// 重心坐标计算 
vec3 barycentric(vec2 A, vec2 B, vec2 C, vec2 P) {
    vec3 s[2];
    for (int i=2; i--; ) {
        s[i][0] = C[i]-A[i];
        s[i][1] = B[i]-A[i];
        s[i][2] = A[i]-P[i];
    }
    vec3 u = cross(s[0], s[1]);
    if (std::abs(u[2])>1e-2) 
        return vec3(1.f-(u.x+u.y)/u.z, u.y/u.z, u.x/u.z);
    return vec3(-1,1,1);
}

IShader::~IShader() {}
// ==========================================
// 核心光栅化逻辑
// ==========================================
void triangle(vec<4> *pts, IShader &shader, TGAImage &image, float *zbuffer) {
    vec2 pts2[3]; // 屏幕上的 2D 坐标 (x, y)
    for (int i=0; i<3; i++) {
        // 进行透视除法 (Perspective Division)
        pts2[i] = vec2(pts[i][0]/pts[i][3], pts[i][1]/pts[i][3]);
    }

    // 包围盒计算
    int bboxmin[2] = {image.width()-1,  image.height()-1};
    int bboxmax[2] = {0, 0};
    int clamp[2]   = {image.width()-1,  image.height()-1};

    for (int i=0; i<3; i++) {
        for (int j=0; j<2; j++) {
            bboxmin[j] = std::max(0,        std::min(bboxmin[j], (int)pts2[i][j]));
            bboxmax[j] = std::min(clamp[j], std::max(bboxmax[j], (int)pts2[i][j]));
        }
    }

    // 光栅化循环
    vec3 P;
    TGAColor color;
    for (P.x=bboxmin[0]; P.x<=bboxmax[0]; P.x++) {
        for (P.y=bboxmin[1]; P.y<=bboxmax[1]; P.y++) {
            // 1. 计算重心坐标
            vec3 bc_screen = barycentric(pts2[0], pts2[1], pts2[2], vec2(P.x, P.y));
            
            // 2. 检查是否在三角形内
            if (bc_screen.x<0 || bc_screen.y<0 || bc_screen.z<0) continue;

            // 3. 插值计算 Z 值 (以及 w 值，用于透视矫正插值)
            // 注意：严格的透视插值需要用 1/w，这里先用简单的线性插值
            float z = 0;
            for (int i=0; i<3; i++) z += pts[i][2]/pts[i][3] * bc_screen[i]; // z/w 插值

            // 4. Z-Buffer 测试
            int idx = int(P.x + P.y * image.width());
            if (zbuffer[idx] < z) {

                // perspective correct interpolation
                float w0 = pts [0][3];
                float w1 = pts [1][3];
                float w2 = pts [2][3];
                // new_weight = (sceen_weight / w) / sum(sceen_weight / w)
                float z_interp = 1.f / (bc_screen.x / w0 + bc_screen.y / w1 + bc_screen.z / w2);
                vec3 bc_persp;
                bc_persp.x = (bc_screen.x / w0) * z_interp;
                bc_persp.y = (bc_screen.y / w1) * z_interp;
                bc_persp.z = (bc_screen.z / w2) * z_interp;
        
                // 5. 调用片元着色器 (Fragment Shader)
                // Shader 决定这个像素是什么颜色，或者是否丢弃
                // bool discard = shader.fragment(bc_screen, color);
                bool discard = shader.fragment(bc_persp, color);
        
                if (!discard) {

                    // Alpha blending
                    float alpha = color.bgra[3] / 255.f ; // 归一化alpha -> [0.~0.1]
                    if (alpha > 0.99f) {
                        zbuffer[idx] = z;
                        image.set(P.x, P.y, color);
                    }
                    else {
                        TGAColor bg_color = image.get(P.x, P.y);
                        
                        for (int i = 0; i < 3; i++) {
                            color.bgra[i] = (unsigned char)(color.bgra[i] * alpha + bg_color.bgra[i] * (1.f - alpha));
                            color.bgra[3] = 255;
                        }
                        // zbuffer[idx] = z;
                        image.set(P.x, P.y, color);
                    }
                } 
            }
        }
    }
}

void line(vec3 p0, vec3 p1, vec3 c0, vec3 c1, TGAImage &image) {
    bool steep = std::abs(p0.x - p1.x) < std::abs(p0.y - p1.y);
    if (steep) { std::swap(p0.x, p0.y); std::swap(p1.x, p1.y); }
    if (p0.x > p1.x) { std::swap(p0.x, p1.x); std::swap(p0.y, p1.y);std::swap(c0, c1); }
    int dx = p1.x - p0.x;
    int dy = std::abs(p1.y - p0.y);

    int error = 0;
    int derror = 2 * dy;
    int y = p0.y;
    int ystep = (p1.y > p0.y ? 1 : -1);

    for (int x = p0.x; x <= p1.x; x++) {
        float t = (float)(x - p0.x)/ (float)(dx == 0 ? 1 : dx); // 防止除以零

        // 线性插值颜色
        float r = c0.x * (1.f - t) + c1.x * t;
        float g = c0.y * (1.f - t) + c1.y * t;
        float b = c0.z * (1.f - t) + c1.z * t;
        // 【核心修复】使用构造函数初始化 TGAColor (R, G, B, A)
        // 构造函数内部会自动把 R 放到 bgra[2]，B 放到 bgra[0]
        TGAColor color(
            static_cast<unsigned char>(r), 
            static_cast<unsigned char>(g), 
            static_cast<unsigned char>(b), 
            255
        );


        if (steep) {
            image.set(y, x, color); // 无分支，直接写
        } else {
            image.set(x, y, color); // 无分支，直接写
        }

        error += derror;
        if (error > dx) {
            y += ystep;
            error -= 2 * dx;
        }
    }
}

// Blinn-Phong 实现
void blinn_phong(vec3 n, vec3 l, vec3 v,float& out_diff, float& out_spec, float spec_power) {
    // 1. 漫反射 (Diffuse)
    out_diff = std::max(0.f, n * l);

    // 2. 高光 (Specular) - Blinn-Phong 使用半程向量 H
    vec3 h = (l + v).normalize(); // 半程向量
    out_spec = std::pow(std::max(0.f, n * h), spec_power);
    // 3. 组合 (这里简单的按 1.0 Diffuse + 0.6 Specular 混合，可根据材质调整)
    // return diff + spec * 0.6f;
    // 传入intensity 此处为 0～255
    // return diff + spec * intensity;
}