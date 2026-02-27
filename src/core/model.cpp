#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include "core/model.h" // 确保这里路径正确，或者用 "model.h"
#include "math/geometry.h"
#include "platform/tgaimage.h"


// 辅助函数：替换文件后缀加载纹理
void Model::load_texture(std::string filename, const char* suffix, TGAImage &img) {
    std::string texfile(filename);
    size_t dot = texfile.find_last_of(".");
    if (dot != std::string::npos) {
        texfile = texfile.substr(0, dot) + std::string(suffix);
        bool ok = img.read_tga_file(texfile.c_str());

        std::cerr << "texture file " << texfile << " loading " << (img.read_tga_file(texfile.c_str()) ? "ok" : "failed") << std::endl;
        if (ok) img.flip_vertically(); //以此引擎逻辑，贴图通常需要上下翻转
    }
}

Model::Model(const char* filename):verts_(), uvs_(), norms_(), faces_(), face_uvs_(), face_norms_(), diffusemap_(), normalmap_(), specularmap_() {
    std::ifstream in;
    in.open(filename, std::ifstream::in);
    if (in.fail()) {
        std::cerr << "Cannot open " << filename << std::endl;
        return;
    }

    std::string line;
    while(!in.eof()) {
        std::getline(in, line);
        std::istringstream iss(line);
        char trash;
        if (!line.compare(0, 2, "v ")) {
            iss >> trash;
            vec3 v;
            for (int i = 0; i < 3; i++) iss >> v[i];
            verts_.push_back(v);
        } else if (!line.compare(0, 3, "vn ")) { // 【新增】读取法线
            iss >> trash >> trash;
            vec3 n;
            for (int i = 0; i < 3; i++) iss >> n[i];
            norms_.push_back(n);
        } else if (!line.compare(0, 3, "vt ")) { // 解析vt
            iss >> trash >> trash;
            vec2 uv;
            for (int i = 0; i < 2; i++) iss >> uv[i];
            uvs_.push_back(uv);
    
        } else if (!line.compare(0, 2, "f ")) {
            std::vector<int> f;
            std::vector<int> ft; // uv索引
            std::vector<int> fn; // 【新增】法线索引
            int idx, idx_t, idx_n;
            iss >> trash;
            while (iss >> idx) {
                idx--; // 顶点索引
                f.push_back(idx);

                // 处理 f v/vt/vn 格式
                char c = iss.peek();
                if (c == '/') {
                    iss >> trash;
                    c = iss.peek();
                    if (c == '/') { // v//vn
                        iss >> trash;
                        iss >> idx_n;
                        idx_n--; 
                        fn.push_back(idx_n);
                        ft.push_back(-1);
                    } else { // v/vt/vn
                        iss >> idx_t; // vt
                        idx_t--;
                        ft.push_back(idx_t);
                        if (iss.peek() == '/') {
                                iss >> trash;
                                iss >> idx_n;
                                idx_n--;
                                fn.push_back(idx_n);
                        } 
                    }
                } else {
                    ft.push_back(-1);
                    fn.push_back(-1);
                }
            }
            faces_.push_back(f);
            face_uvs_.push_back(ft);
            face_norms_.push_back(fn); // 保存法线索引
        }
    }
    std::cerr << "# v# " << verts_.size() << " vn# " << norms_.size() << " f# "  << faces_.size() << std::endl;

    //自动加载同名贴图
    load_texture(filename, "_diffuse.tga", diffusemap_);
    // // load_texture(filename, "_nm.tga", normalmap_);
    load_texture(filename, "_nm_tangent.tga", normalmap_);
    load_texture(filename, "_spec.tga", specularmap_);
    load_texture(filename, "_glow.tga", glowmap_);
    load_texture(filename, "_gloss.tga", glossmap_);
}

Model::~Model() {}

int Model::nverts() const {
    return static_cast<int>(verts_.size());
}

int Model::nfaces() const {
    return static_cast<int>(faces_.size());
}

std::vector<int> Model::face(int idx) {
    return faces_[idx];
}

vec3 Model::vert(int i) {
    return verts_[i];
}

vec3 Model::vert(int iface, int nthvert) {
    return verts_[faces_[iface][nthvert]];
}

// 【新增】获取法线实现
vec3 Model::normal(int iface, int nthvert) {
    if (face_norms_[iface].empty() || face_norms_[iface][nthvert] == -1 ) 
    return vec3(0,0,1);
    int idx = face_norms_[iface][nthvert];
    return norms_[idx].normalize();
}

// 获取uv
vec2 Model::uv(int iface, int nthvert) {
    // 索引面越界检测
    if (iface < 0 || iface >= face_uvs_.size())
    return vec2(0.f,0.f);

    // 检测该面是否存在uv数据
    if (face_uvs_[iface].empty())
    return vec2(0.f,0.f);

    // 顶点索引越界检测
    if (nthvert < 0 || nthvert >= face_uvs_[iface].size())
    return vec2(0.f,0.f);

    // 获取uv索引
    int idx = face_uvs_[iface][nthvert];

    // 索引有效检测
    if (idx < 0 || idx >= uvs_.size())
    return vec2(0.f,0.f);

    return uvs_[idx];
}

// 从diffuse map 采样颜色
TGAColor Model::diffuse(vec2 uv) {
    //  纹理位加载返回白色 防止除零和越界
    if (diffusemap_.width() == 0 || diffusemap_.height() == 0)
    return TGAColor(255,255,255);

    // Repeat 模式处理 uv 越界
    float u = uv.x - std::floor(uv.x);
    float v = uv.y - std::floor(uv.y);

    vec2 uv_mapped(u * diffusemap_.width(),v * diffusemap_.height());
    return diffusemap_.get(uv_mapped.x,uv_mapped.y);
}

// 自发光
TGAColor Model::glow(vec2 uv) {
    if (glowmap_.width() == 0 || glowmap_.height() == 0) return TGAColor(0,0,0); //默认无光

    // uv映射
    float u = uv.x - std::floor(uv.x);
    float v = uv.y - std::floor(uv.y);

    vec2 uv_mapped(u * glowmap_.width(),v * glowmap_.height());
    TGAColor c = glowmap_.get(uv_mapped.x, uv_mapped.y);
    return glowmap_.get(uv_mapped.x, uv_mapped.y);
}

// 光泽度
float Model::gloss(vec2 uv) {
    if (glossmap_.width() == 0 || glossmap_.height() == 0) return 0.0f;

    float u = uv.x - std::floor(uv.x);
    float v = uv.y - std::floor(uv.y);

    vec2 uv_mapped(u * glossmap_.width(),v * glossmap_.height());
    TGAColor c = glossmap_.get(uv_mapped.x,uv_mapped.y);
    return (float)c[0]; //灰度图 只需要取其中一个通道
}

// 新增从normal map 采样法线
vec3 Model::normal(vec2 uv) {
    if (normalmap_.width() == 0 || normalmap_.height() == 0)
    return vec3(0,0,1);

    float u = uv.x - std::floor(uv.x);
    float v = uv.y - std::floor(uv.y);


    vec2 uv_mapped(u * normalmap_.width(),v * normalmap_.height());
    TGAColor c = normalmap_.get(uv_mapped.x, uv_mapped.y);
    vec3 res;
    // color: 0~255 -> vector: -1~1
    for (int i=0 ; i<3; i++)
        res[2-i] = (float)c[i]/255.f*2.f - 1.f;
    return res;
}

// 高光采样
float Model::specular(vec2 uv) {
    if (specularmap_.width() == 0 || specularmap_.height() == 0) {
        return 10.0f; // 无贴图返回低高光强度
    }
    float u = uv.x - std::floor(uv.x);
    float v = uv.y - std::floor(uv.y);

    vec2 uv_mapped(u * specularmap_.width(),v * specularmap_.height());
    TGAColor c = specularmap_.get(uv_mapped.x,uv_mapped.y);
    return (float)c[0]; //灰度图 只需要取其中一个通道
}

