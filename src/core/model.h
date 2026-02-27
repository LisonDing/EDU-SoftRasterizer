#pragma once
#include <vector>
#include <string>
#include "math/geometry.h"
#include "platform/tgaimage.h"

class Model {
private:
    std::vector<vec3> verts_; // 顶点列表
    std::vector<vec2> uvs_; // uv纹理坐标
    std::vector<vec3> norms_; // 【新增】存储所有法线向量 (vn)
    std::vector<std::vector<int>> faces_; // 面列表，每个面由顶点索引组成
    std::vector<std::vector<int>> face_uvs_; // vt 索引
    std::vector<std::vector<int>> face_norms_; // 【新增】存储每个面对应的法线索引

    // 纹理图片数据
    TGAImage diffusemap_; // 漫反射
    TGAImage normalmap_; // 法线
    TGAImage specularmap_; // 高光
    TGAImage glowmap_; // 自发光
    TGAImage glossmap_; // 光泽度

public:
    Model(const char* filename); // 从文件加载模型
    ~Model() ;

    int nverts() const; // 返回顶点数量
    int nfaces() const; // 返回面数量

    vec3 vert(int i) ; // 返回第i个顶点
    std::vector<int> face(int idx) ; // 返回第idx个面的顶点

    // 【新增】直接获取第 iface 个面的第 nthvert 个顶点
    vec3 vert(int iface, int nthvert);

    // 【新增】获取第 iface 个面的第 nthvert 个顶点的法线
    vec3 normal(int iface, int nthvert);

    // 获取 uv 坐标
    vec2 uv(int iface, int nthvert);

    // 从纹理图片中读取颜色&法线
    TGAColor diffuse(vec2 uv);

    TGAColor glow(vec2 uv);
    float gloss(vec2 uv);
    vec3 normal(vec2 uv);

    float specular(vec2 uv);

    // 加载纹理文件的辅助函数
    void load_texture(std::string filename, const char* suffix, TGAImage &img);
};
