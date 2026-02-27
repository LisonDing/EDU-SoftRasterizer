#pragma once
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <iostream>
#include <vector>
#include <cstring>

class AppWindow {
public:
    Display* display;
    ::Window window; // X11 定义了 Window 宏，需要用 :: 区分
    GC gc;
    int width, height;
    bool keys[256]; // 我们的按键状态数组

    // XImage 结构用于在客户端和 X 服务器之间传输图像
    XImage* ximage = nullptr; 

    AppWindow(int w, int h, const char* title) : width(w), height(h) {
        // 1. 打开连接
        display = XOpenDisplay(NULL);
        if (!display) {
            std::cerr << "Cannot open X display. (Did you install XQuartz?)" << std::endl;
            exit(1);
        }

        int screen = DefaultScreen(display);
        ::Window root = RootWindow(display, screen); // 获取根窗口 ID

        // 【新增】打印调试信息
        std::cout << "Debug X11:" << std::endl;
        std::cout << "  Display: " << display << std::endl;
        std::cout << "  Screen: " << screen << std::endl;
        std::cout << "  Root Window ID: " << root << std::endl;
        std::cout << "  Width: " << width << ", Height: " << height << std::endl;
        int black = BlackPixel(display, screen);
        int white = WhitePixel(display, screen);

        // 2. 创建窗口
        window = XCreateSimpleWindow(display, RootWindow(display, screen),
                                     0, 0, width, height, 0,
                                     black, white);

        XSetStandardProperties(display, window, title, title, None, NULL, 0, NULL);

        // 3. 选择我们要监听的输入事件 (按键按下、松开、窗口结构变化)
        XSelectInput(display, window, KeyPressMask | KeyReleaseMask | StructureNotifyMask);

        // 4. 创建绘图上下文 (GC)
        gc = XCreateGC(display, window, 0, 0);

        // 5. 显示窗口
        XMapWindow(display, window);

        // 6. 等待窗口映射完成 (防止还没准备好就画图)
        while (1) {
            XEvent e;
            XNextEvent(display, &e);
            if (e.type == MapNotify) break;
        }

        // 初始化按键数组
        memset(keys, 0, sizeof(keys));
    }

    ~AppWindow() {
        if (ximage) {
            // 注意：XDestroyImage 会释放 data 指向的内存
            // 因为 data 是外部传进来的 buffer，我们不能让 X11 释放它
            ximage->data = NULL; 
            XDestroyImage(ximage);
        }
        XFreeGC(display, gc);
        XDestroyWindow(display, window);
        XCloseDisplay(display);
    }

    // 核心功能：把 buffer 画到 X11 窗口上
    // buffer 必须是 BGRA (Little Endian) 或 ARGB (Big Endian)
    // TGAImage 默认是 BGRA，正好符合大多数 X11 的要求
    void draw_buffer(unsigned char* buffer) {
        // 如果 ximage 还没创建，或者只创建一次 (为了简单，这里每次重新封装)
        // 在 X11 中，最好创建一个长期存在的 XImage，这里为了逻辑简单做适配
        
        int depth = DefaultDepth(display, DefaultScreen(display));
        Visual* visual = DefaultVisual(display, DefaultScreen(display));

        // 创建一个指向我们 buffer 的 XImage
        // 注意：这里不会拷贝数据，只是引用
        if (!ximage) {
            ximage = XCreateImage(display, visual, depth, ZPixmap, 0, 
                                  (char*)buffer, width, height, 32, 0);
        } else {
            ximage->data = (char*)buffer; // 更新指针
        }

        // 将图像推送到窗口
        XPutImage(display, window, gc, ximage, 0, 0, 0, 0, width, height);
        XFlush(display); // 强制刷新命令队列
    }

    // 处理消息队列
    bool is_running() {
        // 检查是否有挂起的事件
        while (XPending(display) > 0) {
            XEvent event;
            XNextEvent(display, &event);
            // 【新增】处理 X11 的自动按键重复 (Auto-Repeat)
            // 如果收到 Release 事件，且队列里紧接着一个同一时间的 Press 事件，说明是长按
            if (event.type == KeyRelease && XPending(display)) {
                XEvent next_event;
                XPeekEvent(display, &next_event); // 偷看下一个事件
                if (next_event.type == KeyPress &&
                    next_event.xkey.time == event.xkey.time &&
                    next_event.xkey.keycode == event.xkey.keycode) {
                    // 这是一个自动重复的 Release，忽略它（不要把 keys 设为 false）
                    // 并且我们可以选择跳过处理，或者直接继续下一轮
                    continue; 
                }
            }

            if (event.type == KeyPress || event.type == KeyRelease) {
                KeySym key = XLookupKeysym(&event.xkey, 0);
                bool is_pressed = (event.type == KeyPress);

                // 简单的键位映射
                if (key == XK_Escape) return false; // ESC 退出
                if (key == XK_a || key == XK_A) keys['A'] = is_pressed;
                if (key == XK_d || key == XK_D) keys['D'] = is_pressed;
                if (key == XK_w || key == XK_W) keys['W'] = is_pressed;
                if (key == XK_s || key == XK_S) keys['S'] = is_pressed;
            }
            // 这里没有专门处理 WM_DELETE_WINDOW，通常需要 Atom 设置
            // 简单起见，按 ESC 退出即可
        }
        return true;
    }
};