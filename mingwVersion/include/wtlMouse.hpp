#pragma once
/**
 * mouse.hpp — 鼠标事件库（C++17 / Windows）
 *
 * 仅暴露功能 API 接口，所有实现细节（Windows API、Interception 驱动、
 * 鼠标钩子等）均位于 mouse.cpp，避免使用方编译时引入过多头文件。
 *
 * 使用：
 *   wtl::MouseEvent mouse;
 *   mouse.mouseMoveTo(500, 300, 0.5);
 *   mouse.mouseClick(100, 200, 2, "right");
 */

#include <string>

namespace wtl
{

/*************** 鼠标事件码枚举 ***************/
// 按下为正数，抬起为负数 //
namespace mouseCode
{
    enum MOUSECODE {
        /************** 鼠标按下 ****************/
        WM_LEFTDOWN=1001,  WM_ROLLDOWN=1002, WM_RIGHTDOWN=1003, WM_FORWARDOWN=1004,
        WM_BACKSPACEDOWN=1005,

        /************** 鼠标抬起 ****************/
        WM_LEFTUP=-1001, WM_ROLLUP=-1002, WM_RIGHTUP=-1003, WM_FORWARDUP=-1004,
        WM_BACKSPACEUP=-1005,
    };
}

/*************** 鼠标坐标结构体 ***************/
struct MousePosition  // 获取光标所在当前位置
{
    long x;
    long y;
};

// 获取光标目前所在的坐标 //
MousePosition getMousePosition(bool out_put = true);

/*************** 鼠标事件类 ***************/
class MouseEvent
{
public:
    MouseEvent();

    void mouseMoveTo(int x, int y);  // x,y坐标
    // 驱动级操作 nothing是蹭标志，无作用 //
    void mouseMoveTo(int x, int y, bool drivingStage, bool nothing);

    void mouseMoveTo(int targetX, int targetY, float delay);
    // 驱动级操作 //
    void mouseMoveTo(int targetX, int targetY, float delay, bool drivingStage);

    void mouseDown(const std::string& button = "left");  // 鼠标按下键位
    void mouseDown(const std::string& button, bool drivingStage);  // 驱动级

    void mouseUp(const std::string& button = "left");  // 鼠标释放
    void mouseUp(const std::string& button, bool drivingStage);  // 驱动级

    // left,right,roll_down,roll_up //
    void mouseClick(int x, int y, int clicks = 1, const std::string& button = "left", float delay = 0);
    void mouseClickDriving(int x, int y, int clicks = 1, const std::string& button = "left", float delay = 0);

    void mouseClick(MousePosition& positions, int clicks = 1, const std::string& button = "left", float delay = 0);
    void mouseClickDriving(MousePosition& positions, int clicks = 1, const std::string& button = "left", float delay = 0);

    void mouseClick(int x, int y, int clicks = 1, float delay = 0);
    void mouseClickDriving(int x, int y, int clicks = 1, float delay = 0);

    void mouseRoll(int move);  // 鼠标滚轮,正数向上负数向下
    void mouseRoll(int move, bool drivingStage);

    void listenMouseEvent();  // 监听鼠标事件
    int getMouseCode();       // 获取鼠标虚键码

    // 鼠标侧键 //
    void mouseSidekeyDown(const std::string& button, int clicks = 1, double delay = 0.0);
    void mouseSidekeyDownDriving(const std::string& button, int clicks = 1, double delay = 0.0);

    void mouseSidekeyUp(const std::string& button, int clicks = 1, double delay = 0.0);
    void mouseSidekeyUpDriving(const std::string& button, int clicks = 1, double delay = 0.0);

    void mouseSideKeyPress(const std::string& button, int clicks = 1, double delay = 0.0);
    void mouseSideKeyPressDriving(const std::string& button, int clicks = 1, double delay = 0.0);

    static void exitMouseEvent();      // 退出鼠标监听事件
    static void uninstallMouseEvent(); // 注销所有鼠标事件，但不退出程序

    // 程序退出时释放鼠标资源（未松开的按键、驱动级按键、钩子等）
    static void cleanup();
};

} // namespace wtl
