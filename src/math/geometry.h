#pragma once
#include <cmath>
#include <vector>
#include <cassert>
#include <iostream>

// 统一浮点类型，方便未来切换 double/float
typedef float SFloat; 

// ==============================================================
//  通用向量模板 (Vector)
// ==============================================================
template <size_t Dim> struct vec {
    SFloat data[Dim];
    
    // 访问操作符重载
    SFloat& operator[](const size_t i)       { assert(i < Dim); return data[i]; }
    const SFloat& operator[](const size_t i) const { assert(i < Dim); return data[i]; }
};

// ==============================================================
//  Vector 2 特化 (二维向量)
// ==============================================================
template <> struct vec<2> {
    SFloat x, y;
    vec() : x(0), y(0) {}
    vec(SFloat _x, SFloat _y) : x(_x), y(_y) {}
    
    // 基础运算
    vec<2> operator+(const vec<2>& V) const { return vec<2>(x + V.x, y + V.y); }
    vec<2> operator-(const vec<2>& V) const { return vec<2>(x - V.x, y - V.y); }
    vec<2> operator*(float f)          const { return vec<2>(x * f, y * f); }
    
    SFloat& operator[](const size_t i)       { assert(i < 2); return i <= 0 ? x : y; }
    const SFloat& operator[](const size_t i) const { assert(i < 2); return i <= 0 ? x : y; }
};

// ==============================================================
//  Vector 3 特化 (三维向量)
// ==============================================================
template <> struct vec<3> {
    SFloat x, y, z;
    vec() : x(0), y(0), z(0) {}
    vec(SFloat _x, SFloat _y, SFloat _z) : x(_x), y(_y), z(_z) {}
    
    // 基础运算
    vec<3> operator+(const vec<3>& V) const { return vec<3>(x + V.x, y + V.y, z + V.z); }
    vec<3> operator-(const vec<3>& V) const { return vec<3>(x - V.x, y - V.y, z - V.z); }
    vec<3> operator*(float f)          const { return vec<3>(x * f, y * f, z * f); }
    
    // 点积 (Dot Product)
    SFloat operator*(const vec<3>& V) const { return x*V.x + y*V.y + z*V.z; }

    // 叉积 (Cross Product)
    vec<3> cross(const vec<3>& V) const { // ^ operator usually has wrong precedence
        return vec<3>(y*V.z - z*V.y, z*V.x - x*V.z, x*V.y - y*V.x);
    }

    // 模长与归一化
    SFloat norm() const { return std::sqrt(x*x + y*y + z*z); }
    vec<3> & normalize(SFloat l = 1) { *this = (*this) * (l / norm()); return *this; }

    SFloat& operator[](const size_t i)       { assert(i < 3); return i <= 0 ? x : (1 == i ? y : z); }
    const SFloat& operator[](const size_t i) const { assert(i < 3); return i <= 0 ? x : (1 == i ? y : z); }
};

// ==============================================================
//  Vector 4 特化 (四维向量 / 齐次坐标)
// ==============================================================
template <> struct vec<4> {
    SFloat x, y, z, w;
    vec() : x(0), y(0), z(0), w(1) {} // w 默认为 1
    vec(SFloat _x, SFloat _y, SFloat _z, SFloat _w) : x(_x), y(_y), z(_z), w(_w) {}
    
    // 基础运算
    vec<4> operator+(const vec<4>& V) const { return vec<4>(x + V.x, y + V.y, z + V.z, w + V.w); }
    vec<4> operator-(const vec<4>& V) const { return vec<4>(x - V.x, y - V.y, z - V.z, w - V.w); }
    vec<4> operator*(float f)          const { return vec<4>(x * f, y * f, z * f, w * f); }
    
    // 点积
    SFloat operator*(const vec<4>& V) const { return x*V.x + y*V.y + z*V.z + w*V.w; }

    SFloat& operator[](const size_t i)       { assert(i < 4); return i <= 0 ? x : (1 == i ? y : (2 == i ? z : w)); }
    const SFloat& operator[](const size_t i) const { assert(i < 4); return i <= 0 ? x : (1 == i ? y : (2 == i ? z : w)); }
};

// ==============================================================
//  辅助函数 (Helpers)
// ==============================================================
typedef vec<2> vec2;
typedef vec<3> vec3;
typedef vec<4> vec4;

inline vec3 cross(vec3 v1, vec3 v2) { return v1.cross(v2); }

// 【核心工具】将 3D 向量嵌入到 4D (补 1.0 或 fill)
template<size_t DimRows, size_t DimCols> vec<DimRows> embed(const vec<DimCols> &v, SFloat fill=1.f) {
    vec<DimRows> ret;
    for (size_t i=DimCols; i--; ret[i]=v[i]);
    ret[DimCols] = fill;
    return ret;
}

// 【核心工具】将 4D 向量投影回 3D/2D (透视除法)
template<size_t DimRows,size_t DimCols> vec<DimRows> proj(const vec<DimCols> &v) {
    vec<DimRows> ret;
    for (size_t i=DimRows; i--; ret[i]=v[i]);
    return ret;
}

// ==============================================================
//  Matrix (矩阵)
// ==============================================================
template<size_t Dim> struct dt;

template<size_t rows, size_t cols> struct mat {
    vec<cols> rows_vec[rows];

    mat() {}
    
    // 访问
    vec<cols>& operator[] (const size_t idx)       { assert(idx < rows); return rows_vec[idx]; }
    const vec<cols>& operator[] (const size_t idx) const { assert(idx < rows); return rows_vec[idx]; }

    // 获取列
    vec<rows> col(const size_t idx) const {
        assert(idx < cols);
        vec<rows> ret;
        for(size_t i=rows; i--; ret[i]=rows_vec[i][idx]);
        return ret;
    }

    // 设置列
    void set_col(size_t idx, vec<rows> v) {
        assert(idx < cols);
        for(size_t i=rows; i--; rows_vec[i][idx]=v[i]);
    }

    // 单位矩阵
    static mat<rows,cols> identity() {
        mat<rows,cols> ret;
        for(size_t i=rows; i--; )
            for(size_t j=cols;j--; ret[i][j]=(i==j));
        return ret;
    }

    // 行列式相关 (Det, Cofactor, Adjugate, Invert)
    SFloat det() const { return dt<cols>::det(*this); }
    mat<rows-1,cols-1> get_minor(size_t row, size_t col) const {
        mat<rows-1,cols-1> ret;
        for(size_t i=rows-1; i--; )
            for(size_t j=cols-1;j--; ret[i][j]=rows_vec[i<row?i:i+1][j<col?j:j+1]);
        return ret;
    }
    SFloat cofactor(size_t row, size_t col) const { return get_minor(row,col).det()*((row+col)%2 ? -1 : 1); }
    mat<rows,cols> adjugate() const {
        mat<rows,cols> ret;
        for(size_t i=rows; i--; ) for(size_t j=cols; j--; ret[i][j]=cofactor(i,j));
        return ret;
    }
    mat<rows,cols> invert_transpose() {
        mat<rows,cols> ret = adjugate();
        SFloat tmp = ret[0]*rows_vec[0];
        return ret/tmp;
    }
    mat<rows,cols> invert() { return invert_transpose().transpose(); }
    mat<cols,rows> transpose() {
        mat<cols,rows> ret;
        for(size_t i=cols; i--; ret[i]=this->col(i));
        return ret;
    }
};

// ==============================================================
//  矩阵运算符重载
// ==============================================================

// Matrix * Vector
template<size_t nrows,size_t ncols> vec<nrows> operator*(const mat<nrows,ncols>& lhs, const vec<ncols>& rhs) {
    vec<nrows> ret;
    for(size_t i=nrows; i--; ret[i]=lhs[i]*rhs);
    return ret;
}

// Matrix * Matrix
template<size_t R1,size_t C1,size_t C2>mat<R1,C2> operator*(const mat<R1,C1>& lhs, const mat<C1,C2>& rhs) {
    mat<R1,C2> result;
    for(size_t i=R1; i--; )
        for(size_t j=C2; j--; ) {
            result[i][j] = 0; // 必须初始化为0
            for(size_t k=C1; k--; result[i][j]+=lhs[i][k]*rhs[k][j]);
        }
    return result;
}

// Matrix * float / float
template<size_t nrows,size_t ncols>mat<nrows,ncols> operator*(const mat<nrows,ncols>& lhs, const SFloat& val) {
    mat<nrows,ncols> result;
    for(size_t i=nrows; i--; result[i] = lhs[i]*val);
    return result;
}
template<size_t nrows,size_t ncols>mat<nrows,ncols> operator/(const mat<nrows,ncols>& lhs, const SFloat& val) {
    mat<nrows,ncols> result;
    for(size_t i=nrows; i--; result[i] = lhs[i]/val);
    return result;
}

// Determinant helper
template<size_t n> struct dt {
    static SFloat det(const mat<n,n>& src) {
        SFloat ret=0;
        for(size_t i=n; i--; ret += src[0][i]*src.cofactor(0,i));
        return ret;
    }
};
template<> struct dt<1> {
    static SFloat det(const mat<1,1>& src) { return src[0][0]; }
};