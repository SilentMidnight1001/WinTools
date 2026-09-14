#include "winToolsH.hpp"   // mouse.hpp + wtl::sleep + autoClear + _cleanupCheck
#include <interception.h>
#include <cstdlib>

using namespace std;

/********************** 鼠标实现内部状态 **********************/
namespace
{
    // 键名 -> 鼠标键码映射（原 MouseEvent::mouseCode 静态成员）
    std::unordered_map<std::string, int> mouseCode = {
            {"left",0x0001},{"right",0x0002},{"X1",0x0005},{"X2",0x0006},
            {"mouseRoll",0x0004},{"forward",0x0006},{"backspace",0x0005}
    };

    // 驱动级鼠标按下记录（用于程序退出时清理）
    std::unordered_map<std::string, bool> mouseHistoryDown;

    // 鼠标监听状态标记
    std::atomic<bool> mouseEventProcessed(true);  // 原子操作保证线程安全

    /************** 侧边按钮按下抬起特殊记忆 ****************/
    /*
    自定义鼠标侧边按钮，侧边按下为523，抬起524，以这个数字加上按钮编号
    按钮1按下为5231，抬起为5241，按钮2按下5232， 抬起5242
    */
    // 侧边按下 //
    const WPARAM XBUTTON1DOWN = 5231;
    const WPARAM XBUTTON2DOWN = 5232;

    // 侧边抬起 //
    const WPARAM XBUTTON1UP = 5241;
    const WPARAM XBUTTON2UP = 5242;

    // 监听鼠标按下的虚键码 //
    WPARAM __MOUSE_CODE__ = 0;

    /************* press up flag **************/
    // 鼠标按下的标志，后期程序结束如果程序员忘记释放则自动释放按键
    struct MouseFlag
    {
        bool mouse_left_down;  // 左键标志
        bool mouse_right_down;  // 右键标志

        bool mouse_forward_down;
        bool mouse_backspace_down;

        bool mouse_roll_down;
    };

    MouseFlag __mouse_flag__;  // 实例化结构体

    // 鼠标钩子句柄
    HHOOK hMouseHook = NULL;

    // 钩子回调函数，用于处理鼠标事件 //
    LRESULT CALLBACK _mouseProc(int nCode, WPARAM wParam, LPARAM lParam)
    {
        // 获取鼠标事件的详细信息
        MSLLHOOKSTRUCT* pMouseStruct = (MSLLHOOKSTRUCT*)lParam;

        // 如果钩子代码有效（nCode >= 0），处理鼠标事件
        if (nCode >= 0)
        {
            if (mouseEventProcessed.load())
            {
                // 结尾需要把 mouseEventProcessed 改为false //
                // 把鼠标事件码赋值给全局变量 //
                __MOUSE_CODE__ = wParam;

                // 根据wParam判断鼠标事件类型
                switch (wParam)
                {
                    case WM_LBUTTONDOWN: // 左键按下
                        __MOUSE_CODE__ = wParam;
                        break;

                    case WM_LBUTTONUP: // 左键释放
                        __MOUSE_CODE__ = wParam;
                        break;

                    case WM_RBUTTONDOWN: // 右键按下
                        __MOUSE_CODE__ = wParam;
                        break;

                    case WM_RBUTTONUP: // 右键释放
                        __MOUSE_CODE__ = wParam;
                        break;

                    case WM_MBUTTONDOWN: // 中键按下
                        __MOUSE_CODE__ = wParam;
                        break;

                    case WM_MBUTTONUP: // 中键释放
                        __MOUSE_CODE__ = wParam;
                        break;

                }

                // 监听鼠标侧边按钮 //
                if (nCode == HC_ACTION)
                {
                    auto* pMouse = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);

                    // 侧边按钮按下 //
                    if (wParam == WM_XBUTTONDOWN)
                    {
                        // 检查侧键（高16位）
                        DWORD button = HIWORD(pMouse->mouseData);

                        // 后退按钮(按下) //
                        if (button == XBUTTON1)
                        {
                            __MOUSE_CODE__ = XBUTTON1DOWN;
                        }
                            // 前进按钮(按下) //
                        else if (button == XBUTTON2)
                        {
                            __MOUSE_CODE__ = XBUTTON2DOWN;
                        }
                    }

                    // 侧边按钮抬起 //
                    if (wParam == WM_XBUTTONUP)
                    {
                        // 检查侧键（高16位）
                        DWORD button = HIWORD(pMouse->mouseData);

                        // 后退按钮(抬起) //
                        if (button == XBUTTON1)
                        {
                            __MOUSE_CODE__ = XBUTTON1UP;
                        }
                            // 前进按钮(抬起) //
                        else if (button == XBUTTON2)
                        {
                            __MOUSE_CODE__ = XBUTTON2UP;
                        }
                    }
                }

                // 标记有新事件待处理 //
                mouseEventProcessed.store(false);
            }
        }

        // 调用下一个钩子，确保其他程序也能正常处理鼠标事件
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }
} // namespace

/************************* 鼠标事件实现 ****************************/
namespace wtl
{

MouseEvent::MouseEvent()
{
    // 设置进程 DPI 感知级别，确保在高 DPI 屏幕上正确获取坐标 //
    SetProcessDPIAware();

    // 检查是否注册末尾清理函数，False为没有注册 //
    if (!autoClear)
    {
        exitCheckWork();
    }
}

/************************获取光标坐标*******************************/
MousePosition getMousePosition(bool out_put)  // 获取光标目前所在的坐标
{
    // 设置进程 DPI 感知级别，确保在高 DPI 屏幕上正确获取坐标
    SetProcessDPIAware();

    POINT cursorPos;  // 实例化光标结构体，获取位置
    MousePosition mouse_position;  // 实力结构体取其中变量
    if (GetCursorPos(&cursorPos))
    {
        if (out_put)
        {
            printf("The current position of the cursor is (x: %ld, y: %ld)\n", cursorPos.x, cursorPos.y);
        }
        mouse_position.x = cursorPos.x;
        mouse_position.y = cursorPos.y;
        return mouse_position;  // 返回这个实例化的坐标，如果想要拿到坐标需要实例化
    }
    else
    {
        printf("Sorry, cursor coordinates not found\n");
        mouse_position.x = -1;
        mouse_position.y = -1;
        return mouse_position;
    }
}

void MouseEvent::mouseMoveTo(int x, int y)  // mouseMoveTo是void类型
{
    SetCursorPos(x, y);  // 移动鼠标到某处(设置光标位置)
}

// 驱动操作 //
void MouseEvent::mouseMoveTo(int x, int y, bool drivingStage, bool nothing)
{
    if (!drivingStage)
    {
        mouseMoveTo(x, y);
        return;
    }

    InterceptionContext context = interception_create_context();
    if (context == nullptr)
    {
        return;
    }

    // 假设设备标识符正确，或根据您的头文件调整
    // 例如，有时是 interception_is_mouse(device) 来筛选
    InterceptionDevice device = INTERCEPTION_MOUSE(0); // 常见写法，表示第一个鼠标设备

    InterceptionMouseStroke stroke = {0}; // 初始化所有字段为0
    // 关键修正：通常 state 表示按钮状态，移动时设为 0
    stroke.state = 0; // 或者可能是 INTERCEPTION_MOUSE_LEFT_BUTTON_UP 等，但移动时通常为0
    stroke.flags = INTERCEPTION_MOUSE_MOVE_ABSOLUTE; // 设置标志为绝对移动
    stroke.x = x * 65535 / GetSystemMetrics(SM_CXSCREEN);
    stroke.y = y * 65535 / GetSystemMetrics(SM_CYSCREEN);
    // rolling 和 information 通常保持为0
    stroke.rolling = 0;
    stroke.information = 0;

    interception_send(context, device, (const InterceptionStroke *)&stroke, 1);
    interception_destroy_context(context);
}

// 优化后的鼠标移动函数 //
void MouseEvent::mouseMoveTo(int targetX, int targetY, float delay)
{
    if (!delay)
    {
        mouseMoveTo(targetX, targetY);
        return;
    }
    int durationMs = (int)(delay * 1000);
    // 获取起始位置
    MousePosition startPosition = getMousePosition(false);
    // 转为对应的数控类型，这里是双精度浮点 //
    double startX = static_cast<double>(startPosition.x);
    double startY = static_cast<double>(startPosition.y);

    // 计算总位移 //
    double dx = targetX - startX;
    double dy = targetY - startY;

    // 记录开始时间
    auto startTime = chrono::high_resolution_clock::now();

    while (true)
    {
        // 计算已用时间比例 (0.0 - 1.0)
        auto currentTime = chrono::high_resolution_clock::now();
        double elapsed = chrono::duration_cast<chrono::milliseconds>(
                currentTime - startTime).count();
        double t = min(elapsed / durationMs, 1.0);

        // 线性插值计算当前位置
        int currentX = static_cast<int>(startX + dx * t);
        int currentY = static_cast<int>(startY + dy * t);

        // 移动鼠标
        mouseMoveTo(currentX, currentY);

        // 检查是否到达终点或收到停止信号
        if (t >= 1.0) break;

        // 控制刷新率（约60FPS）
        this_thread::sleep_for(chrono::milliseconds(16));
    }

    // 确保到达最终位置
    mouseMoveTo(targetX, targetY);
}

// 驱动级操作 //
void MouseEvent::mouseMoveTo(int targetX, int targetY, float delay, bool drivingStage)
{
    if (!drivingStage)
    {
        mouseMoveTo(targetX, targetY, delay);
        return;
    }

    // 如果延迟为0或非常小，直接跳转，不执行插值
    if (delay <= 0.001f)
    {
        mouseMoveTo(targetX, targetY, drivingStage, true);
        return;
    }

    int durationMs = (int)(delay * 1000);
    MousePosition startPosition = getMousePosition(false);
    double startX = static_cast<double>(startPosition.x);
    double startY = static_cast<double>(startPosition.y);
    double dx = targetX - startX;
    double dy = targetY - startY;

    auto startTime = chrono::high_resolution_clock::now();

    while (true)
    {
        auto currentTime = chrono::high_resolution_clock::now();
        double elapsed = chrono::duration_cast<chrono::milliseconds>(currentTime - startTime).count();
        double t = min(elapsed / durationMs, 1.0);

        int currentX = static_cast<int>(startX + dx * t);
        int currentY = static_cast<int>(startY + dy * t);

        mouseMoveTo(currentX, currentY, drivingStage, true);

        if (t >= 1.0) break;
        this_thread::sleep_for(chrono::milliseconds(16));
    }

    mouseMoveTo(targetX, targetY, drivingStage, true);
}

void MouseEvent::mouseClick(int x, int y, int clicks, const string& button, float delay)
{
    /* 当click为1时并且button不为None时执行单击一次的指令 */
    mouseMoveTo(x, y, delay);
    if (clicks != 0)
    {
        int click_time = 0;  // 初始化点击次数
        while (click_time < clicks && button == "left")  // 当初始点击次数小于目标点击次数增
        {
            mouseDown("left");
            mouseUp("left");
            click_time++;
        }

        while (click_time < clicks && button == "right")  // 当初始点击次数小于目标点击次数增加
        {
            mouseDown("right");
            mouseUp("right");
            click_time++;
        }

        while (click_time < clicks && button == "roll")  // 当初始点击次数小于目标点击次数增加
        {
            mouseDown("roll");
            mouseUp("roll");
            click_time++;  // 初始值增加
        }

    }
}

// 驱动级操作 //
void MouseEvent::mouseClickDriving(int x, int y, int clicks, const string& button, float delay)
{
    // 只有 delay > 0 时才移动，否则直接点击当前位置
    if (delay > 0.001f)
    {
        mouseMoveTo(x, y, delay, true);
    }

    if (clicks != 0)
    {
        int click_time = 0;
        while (click_time < clicks && button == "left")
        {
            mouseDown("left", true);
            mouseUp("left", true);
            click_time++;
        }

        while (click_time < clicks && button == "right")
        {
            mouseDown("right", true);
            mouseUp("right", true);
            click_time++;
        }

        while (click_time < clicks && button == "roll")
        {
            mouseDown("roll", true);
            mouseUp("roll", true);
            click_time++;
        }
    }
}

void MouseEvent::mouseClick(MousePosition& positions, int clicks, const string& button, float delay)
{
    mouseClick(positions.x, positions.y, clicks, button, delay);
}

void MouseEvent::mouseClickDriving(MousePosition& positions, int clicks, const string& button, float delay)
{
    mouseClickDriving(positions.x, positions.y, clicks, button, delay);
}

void MouseEvent::mouseClick(int x, int y, int clicks, float delay)
{
    mouseClick(x, y, clicks, "left", delay);
}

void MouseEvent::mouseClickDriving(int x, int y, int clicks, float delay)
{
    mouseClickDriving(x, y, clicks, "left", delay);
}

void MouseEvent::mouseDown(const string& button)
{
    if (button == "left")
    {
        // 如果没传参数默认左键
        INPUT input = { 0 };  // 定义INPUT结构体变量表示输入事件信息
        input.type = INPUT_MOUSE;  // 指定输入事件类型为鼠标事件
        input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;  // 设定鼠标事件为左键按下
        SendInput(1, &input, sizeof(INPUT));  // 使用SendInput函数发送鼠标事件
        // 设置鼠标被按下
        __mouse_flag__.mouse_left_down = true;  // 标志为true
    }
    else if (button == "right")
    {
        // 如果没传参数默认左键
        INPUT input = { 0 };  // 定义INPUT结构体变量表示输入事件信息
        input.type = INPUT_MOUSE;  // 指定输入事件类型为鼠标事件
        input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;  // 设定鼠标事件为左键按下
        SendInput(1, &input, sizeof(INPUT));  // 使用SendInput函数发送鼠标事件
        // 设置鼠标被按下
        __mouse_flag__.mouse_right_down = true;
    }
    else if (button == "roll")
    {
        INPUT input = {0};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
        SendInput(1, &input, sizeof(INPUT));
        // 设置鼠标被按下
        __mouse_flag__.mouse_roll_down = true;
    }
}

/************** 驱动级鼠标模拟 **************/
void MouseEvent::mouseDown(const string& button, bool drivingStage)
{
    if (!drivingStage)
    {
        mouseDown(button);
        return;
    }

    // 初始化 interception 上下文
    InterceptionContext context = interception_create_context();
    if (!context)
    {
        // 上下文创建失败
        return;
    }

    // 设置要处理的设备类型（这里是鼠标）
    interception_set_filter(context, interception_is_mouse, INTERCEPTION_FILTER_MOUSE_ALL);

    // 根据传入的按钮参数确定按下哪个键
    unsigned short state = 0;

    if (button == "left")
    {
        state = INTERCEPTION_MOUSE_LEFT_BUTTON_DOWN;
        mouseHistoryDown["left"] = true;
    } else if (button == "right")
    {
        state = INTERCEPTION_MOUSE_RIGHT_BUTTON_DOWN;
        mouseHistoryDown["right"] = true;
    }
    else if (button == "roll")
    {
        // ===== 驱动级 鼠标中键按下 =====
        state = INTERCEPTION_MOUSE_MIDDLE_BUTTON_DOWN;
        mouseHistoryDown["roll"] = true;
    }

    // 获取第一个鼠标设备
    for (InterceptionDevice device = INTERCEPTION_MOUSE(0); device <= INTERCEPTION_MOUSE(9); ++device) {
        if (interception_is_mouse(device))
        {
            // 创建鼠标按下事件
            InterceptionMouseStroke stroke_down;
            stroke_down.state = state;  // 按钮按下状态
            stroke_down.flags = INTERCEPTION_MOUSE_MOVE_RELATIVE;  // 相对移动模式
            stroke_down.rolling = 0;
            stroke_down.x = 0;
            stroke_down.y = 0;
            stroke_down.information = 0;

            // 发送按下事件（仅发送按下，不发送释放）
            interception_send(context, device, (InterceptionStroke *)&stroke_down, 1);
            break;  // 只发送到第一个鼠标设备
        }
    }

    // 销毁上下文
    interception_destroy_context(context);
}

void MouseEvent::mouseUp(const string& button)
{
    if (button == "left")
    {
        INPUT input = { 0 };
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_LEFTUP; // 假设是左键，根据需要调整
        SendInput(1, &input, sizeof(INPUT));
        // 松开标志为False
        __mouse_flag__.mouse_left_down = false;
    }
    else if (button == "right")
    {
        INPUT input = { 0 };
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_RIGHTUP; // 假设是左键，根据需要调整
        SendInput(1, &input, sizeof(INPUT));
        // 松开标志为False
        __mouse_flag__.mouse_right_down = false;
    }
    else if (button == "roll")
    {
        INPUT input = {0};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
        SendInput(1, &input, sizeof(INPUT));
        __mouse_flag__.mouse_roll_down = false;
    }
}

// 驱动级 //
void MouseEvent::mouseUp(const string& button, bool drivingStage)
{
    if (!drivingStage)
    {
        mouseUp(button);
        return;
    }

    // 初始化 interception 上下文
    InterceptionContext context = interception_create_context();
    if (!context)
    {
        // 上下文创建失败 //
        return;
    }

    // 设置要处理的设备类型（这里是鼠标） //
    interception_set_filter(context, interception_is_mouse, INTERCEPTION_FILTER_MOUSE_ALL);

    // 根据传入的按钮参数确定释放哪个键
    unsigned short state = 0;

    if (button == "left")
    {
        state = INTERCEPTION_MOUSE_LEFT_BUTTON_UP;
        // 更新按键状态记录
        mouseHistoryDown["left"] = false;
    }
    else if (button == "right")
    {
        state = INTERCEPTION_MOUSE_RIGHT_BUTTON_UP;
        // 更新按键状态记录
        mouseHistoryDown["right"] = false;
    }
    else if (button == "roll")
    {
        state = INTERCEPTION_MOUSE_MIDDLE_BUTTON_UP;
        // 更新按键状态记录
        mouseHistoryDown["roll"] = false;
    }
    else
    {
        // 默认左键释放
        state = INTERCEPTION_MOUSE_LEFT_BUTTON_UP;
        if (mouseHistoryDown.find("left") != mouseHistoryDown.end()) {
            mouseHistoryDown["left"] = false;
        }
    }

    // 获取第一个鼠标设备
    for (InterceptionDevice device = INTERCEPTION_MOUSE(0); device <= INTERCEPTION_MOUSE(9); ++device) {
        if (interception_is_mouse(device))
        {
            // 创建鼠标释放事件
            InterceptionMouseStroke stroke_up;
            stroke_up.state = state;  // 按钮释放状态
            stroke_up.flags = INTERCEPTION_MOUSE_MOVE_RELATIVE;  // 相对移动模式
            stroke_up.rolling = 0;
            stroke_up.x = 0;
            stroke_up.y = 0;
            stroke_up.information = 0;

            // 发送释放事件
            interception_send(context, device, (InterceptionStroke *)&stroke_up, 1);
            break;  // 只发送到第一个鼠标设备
        }
    }

    // 销毁上下文
    interception_destroy_context(context);
}

void MouseEvent::mouseRoll(int move)
{
    INPUT input = { 0 };
    input.type = INPUT_MOUSE;
    input.mi.dx = 0;       // x坐标
    input.mi.dy = 0;       // y坐标
    input.mi.mouseData = move; // 滚动方向和距离
    input.mi.dwFlags = MOUSEEVENTF_WHEEL; // 发送滚轮事件
    input.mi.time = 0;     // 当前系统时间
    input.mi.dwExtraInfo = 0; // 额外信息

    SendInput(1, &input, sizeof(INPUT));
}

void MouseEvent::mouseRoll(int move, bool drivingStage)
{
    if (!drivingStage)
    {
        mouseRoll(move);
        return;
    }

    // 驱动模式实现
    // 初始化 interception 上下文
    InterceptionContext context = interception_create_context();
    if (!context) {
        // 上下文创建失败
        return;
    }

    // 设置要处理的设备类型（这里是鼠标）
    interception_set_filter(context, interception_is_mouse, INTERCEPTION_FILTER_MOUSE_ALL);

    // 获取第一个鼠标设备
    for (InterceptionDevice device = INTERCEPTION_MOUSE(0); device <= INTERCEPTION_MOUSE(9); ++device) {
        if (interception_is_mouse(device))
        {
            // 创建鼠标滚轮事件
            InterceptionMouseStroke stroke;
            stroke.state = 0;  // 滚轮事件不需要按钮状态
            stroke.flags = INTERCEPTION_MOUSE_MOVE_RELATIVE;  // 相对移动模式

            // 滚轮滚动值
            // Windows系统中，正数表示向上滚动，负数表示向下滚动
            // 通常120表示一个"齿"的距离
            stroke.rolling = move;

            stroke.x = 0;
            stroke.y = 0;
            stroke.information = 0;

            // 发送滚轮事件
            interception_send(context, device, (InterceptionStroke *)&stroke, 1);

            break;  // 只发送到第一个鼠标设备
        }
    }

    // 销毁上下文
    interception_destroy_context(context);
}

// 共有函数，获取鼠标事件码 //
int MouseEvent::getMouseCode()
{
    // 检查是否有未处理事件 //
    if (!mouseEventProcessed.load())  // 检查是否有未处理事件
    {
        // 标记事件已处理 //
        mouseEventProcessed.store(true);

        switch (__MOUSE_CODE__)
        {
            case WM_LBUTTONDOWN: // 左键按下
                return mouseCode::WM_LEFTDOWN;

            case WM_LBUTTONUP: // 左键释放
                return mouseCode::WM_LEFTUP;

            case WM_RBUTTONDOWN: // 右键按下
                return mouseCode::WM_RIGHTDOWN;

            case WM_RBUTTONUP: // 右键释放
                return mouseCode::WM_RIGHTUP;

            case WM_MBUTTONDOWN: // 中键按下
                return mouseCode::WM_ROLLDOWN;

            case WM_MBUTTONUP: // 中键释放
                return mouseCode::WM_ROLLUP;

                // 鼠标侧边按钮 //
            case XBUTTON2DOWN:  // 前进按下(按钮1):  // 前进按下(按钮2)
                return mouseCode::WM_FORWARDOWN;
                //
            case XBUTTON1DOWN:  // 后退按下(按钮1)
                return mouseCode::WM_BACKSPACEDOWN;

                // 侧边抬起 //
            case XBUTTON2UP:  // 前进抬起(按钮2)
                return mouseCode::WM_FORWARDUP;
            case XBUTTON1UP:  // 后退抬起(按钮1)
                return mouseCode::WM_BACKSPACEUP;
        }
    }
    return 0;
}

// 使用SendInput模拟侧键按下 //
void MouseEvent::mouseSidekeyDown(const string& button, int clicks, double delay)
{
    auto downIt = [=](){
        INPUT input = {0};
        input.type = INPUT_MOUSE;
        if (button == "X1" || button == "XBUTTON1" || button == "1" || button == "forward")
        {
            input.mi.dwFlags = MOUSEEVENTF_XDOWN;
            input.mi.mouseData = XBUTTON1;
            __mouse_flag__.mouse_forward_down = true;
        }
        else if (button == "X2" || button == "XBUTTON2" || button == "2" || button == "backspace")
        {
            input.mi.dwFlags = MOUSEEVENTF_XDOWN;
            input.mi.mouseData = XBUTTON2;
            __mouse_flag__.mouse_backspace_down = true;
        }
        else
        {
            cerr << "错误: 无效的侧键参数 '" << button
                 << "', 使用 'X1', 'X2', 'XBUTTON1', 'XBUTTON2', '1' 或 '2'" << endl;
            return;
        }

        SendInput(1, &input, sizeof(INPUT));
    };

    if (clicks>1)
    {
        for (int i=0; i<clicks; i++)
        {
            downIt();
            if (delay>0)
            {
                wtl::sleep(delay);
            }
        }
    }
    else
    {
        downIt();
    }
}

// 使用SendInput模拟侧键按下 //
void MouseEvent::mouseSidekeyDownDriving(const string& button, int clicks, double delay)
{
    // 初始化 interception 上下文
    InterceptionContext context = interception_create_context();
    if (!context)
    {
        // 上下文创建失败
        return;
    }

    // 设置要处理的设备类型（这里是鼠标）
    interception_set_filter(context, interception_is_mouse, INTERCEPTION_FILTER_MOUSE_ALL);

    // 根据传入的按钮参数确定按下哪个键
    unsigned short state = 0;

    if (button == "X1" || button == "XBUTTON1" || button == "1" || button == "forward")
    {
        state = INTERCEPTION_MOUSE_BUTTON_4_DOWN;  // 通常是侧键前进
        mouseHistoryDown["X1"] = true;
    } else if (button == "X2" || button == "XBUTTON2" || button == "2" || button == "backspace")
    {
        state = INTERCEPTION_MOUSE_BUTTON_5_DOWN;  // 通常是侧键后退
        mouseHistoryDown["X2"] = true;
    }

    // 获取第一个鼠标设备
    for (InterceptionDevice device = INTERCEPTION_MOUSE(0); device <= INTERCEPTION_MOUSE(9); ++device) {
        if (interception_is_mouse(device))
        {
            // 创建鼠标按下事件
            InterceptionMouseStroke stroke_down;
            stroke_down.state = state;  // 按钮按下状态
            stroke_down.flags = INTERCEPTION_MOUSE_MOVE_RELATIVE;  // 相对移动模式
            stroke_down.rolling = 0;
            stroke_down.x = 0;
            stroke_down.y = 0;
            stroke_down.information = 0;

            // 发送按下事件（仅发送按下，不发送释放）
            interception_send(context, device, (InterceptionStroke *)&stroke_down, 1);
            break;  // 只发送到第一个鼠标设备
        }
    }

    // 销毁上下文
    interception_destroy_context(context);
}

// 使用SendInput模拟侧键抬起 //
void MouseEvent::mouseSidekeyUp(const string& button, int clicks, double delay)
{
    auto upIt = [=]()
    {
        INPUT input = {0};
        input.type = INPUT_MOUSE;

        if (button == "X1" || button == "XBUTTON1" || button == "1" || button == "forward")
        {
            input.mi.dwFlags = MOUSEEVENTF_XUP;
            input.mi.mouseData = XBUTTON1;
            __mouse_flag__.mouse_forward_down = false;
        }
        else if (button == "X2" || button == "XBUTTON2" || button == "2" || button == "backspace")
        {
            input.mi.dwFlags = MOUSEEVENTF_XUP;
            input.mi.mouseData = XBUTTON2;
            __mouse_flag__.mouse_backspace_down = false;
        }
        else {
            cerr << "错误: 无效的侧键参数 '" << button
                 << "', 使用 'X1', 'X2', 'XBUTTON1', 'XBUTTON2', '1' 或 '2'" << endl;
            return;
        }

        SendInput(1, &input, sizeof(INPUT));
    };

    if (clicks>1)
    {
        for (int i=0; i<clicks; i++)
        {
            upIt();
            if (delay>0)
            {
                wtl::sleep(delay);
            }
        }
    }
    else
    {
        upIt();
    }
}

// 使用SendInput模拟侧键抬起 //
void MouseEvent::mouseSidekeyUpDriving(const string& button, int clicks, double delay)
{
    // 初始化 interception 上下文
    InterceptionContext context = interception_create_context();
    if (!context)
    {
        // 上下文创建失败 //
        return;
    }

    // 设置要处理的设备类型（这里是鼠标） //
    interception_set_filter(context, interception_is_mouse, INTERCEPTION_FILTER_MOUSE_ALL);

    // 根据传入的按钮参数确定释放哪个键
    unsigned short state = 0;

    if (button == "X1" || button == "XBUTTON1" || button == "1" || button == "forward")
    {
        state = INTERCEPTION_MOUSE_BUTTON_4_UP;
        mouseHistoryDown["X1"] = false;
    }
    else if (button == "X2" || button == "XBUTTON2" || button == "2" || button == "backspace")
    {
        state = INTERCEPTION_MOUSE_BUTTON_5_UP;
        mouseHistoryDown["X2"] = false;
    }

    // 获取第一个鼠标设备
    for (InterceptionDevice device = INTERCEPTION_MOUSE(0); device <= INTERCEPTION_MOUSE(9); ++device) {
        if (interception_is_mouse(device))
        {
            // 创建鼠标释放事件
            InterceptionMouseStroke stroke_up;
            stroke_up.state = state;  // 按钮释放状态
            stroke_up.flags = INTERCEPTION_MOUSE_MOVE_RELATIVE;  // 相对移动模式
            stroke_up.rolling = 0;
            stroke_up.x = 0;
            stroke_up.y = 0;
            stroke_up.information = 0;

            // 发送释放事件
            interception_send(context, device, (InterceptionStroke *)&stroke_up, 1);
            break;  // 只发送到第一个鼠标设备
        }
    }

    // 销毁上下文
    interception_destroy_context(context);
}

// 侧键按下 //
void MouseEvent::mouseSideKeyPress(const string& button, int clicks, double delay)
{
    if (clicks>1)
    {
        for (int i=0; i<clicks; i++)
        {
            if (button == "X1" || button == "XBUTTON1" || button == "1" || button == "forward" ||
                button == "X2" || button == "XBUTTON2" || button == "2" || button == "backspace")
            {
                mouseSidekeyDown(button);
                mouseSidekeyUp(button);
            }
            if (delay>0)
            {
                wtl::sleep(delay);
            }
        }
    }
    else
    {
        if (button == "X1" || button == "XBUTTON1" || button == "1" || button == "forward" ||
            button == "X2" || button == "XBUTTON2" || button == "2" || button == "backspace")
        {
            mouseSidekeyDown(button);
            mouseSidekeyUp(button);
        }
    }
}

// 侧键按下 //
void MouseEvent::mouseSideKeyPressDriving(const string& button, int clicks, double delay)
{
    if (clicks>1)
    {
        for (int i=0; i<clicks; i++)
        {
            if (button == "X1" || button == "XBUTTON1" || button == "1" || button == "forward" ||
                button == "X2" || button == "XBUTTON2" || button == "2" || button == "backspace")
            {
                mouseSidekeyDownDriving(button);
                mouseSidekeyUpDriving(button);
            }
            if (delay>0)
            {
                wtl::sleep(delay);
            }
        }
    }
    else
    {
        if (button == "X1" || button == "XBUTTON1" || button == "1" || button == "forward" ||
            button == "X2" || button == "XBUTTON2" || button == "2" || button == "backspace")
        {
            mouseSidekeyDownDriving(button);
            mouseSidekeyUpDriving(button);
        }
    }
}

// 监听鼠标函数 //
void MouseEvent::listenMouseEvent()
{
    // 安装鼠标钩子，并将句柄存储到全局变量中
    hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, _mouseProc, GetModuleHandle(nullptr), 0);

    // 检查安装钩子是否成功 //
    if (hMouseHook == nullptr)
    {
        std::cerr << "Failed to install mouse hook." << std::endl;
        return; // 如果安装钩子失败，退出程序
    }

    // 当 PostQuitMessage(0) 被调用时，GetMessage会返回 0, 从而退出循环。 //
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) // 使用GetMessage返回值判断是否收到退出消息
    {
        TranslateMessage(&msg); // 翻译消息
        DispatchMessage(&msg); // 派发消息
    }
}

// 退出函数，用于清理资源并退出程序
void MouseEvent::exitMouseEvent()
{
    // 检查全局变量hMouseHook是否有效
    if (hMouseHook != nullptr)
    {
        // 移除鼠标钩子
        UnhookWindowsHookEx(hMouseHook);
        hMouseHook = nullptr; // 将全局变量置为空
    }

    // 发送程序结束消息 //
    PostQuitMessage(0);

    exit(0); // 退出程序
}

void MouseEvent::uninstallMouseEvent()
{
    // 检查全局变量hMouseHook是否有效
    if (hMouseHook != nullptr)
    {
        // 移除鼠标钩子
        UnhookWindowsHookEx(hMouseHook);
        hMouseHook = nullptr; // 将全局变量置为空
    }
}

// 程序退出时的鼠标资源清理 //
void MouseEvent::cleanup()
{
    MouseEvent mouse;

    // 释放鼠标，如果鼠标按下 //
    if (__mouse_flag__.mouse_left_down)
    {
        // 释放左键
        mouse.mouseUp("left");
        __mouse_flag__.mouse_left_down = false;  // 更新标志
    }
    if (__mouse_flag__.mouse_right_down)
    {
        // 释放右键
        mouse.mouseUp("right");
        __mouse_flag__.mouse_right_down = false;  // 更新标志
    }

    if (__mouse_flag__.mouse_forward_down)
    {
        mouse.mouseSidekeyUp("X1");
        __mouse_flag__.mouse_forward_down = false;
    }
    if (__mouse_flag__.mouse_backspace_down)
    {
        mouse.mouseSidekeyUp("X2");
        __mouse_flag__.mouse_backspace_down = false;
    }

    if (__mouse_flag__.mouse_roll_down)
    {
        mouse.mouseUp("roll");
        __mouse_flag__.mouse_roll_down = false;
    }

    /********* 驱动级鼠标清理 *********/
    for (auto &[k, v] : mouseHistoryDown)
    {
        if (v)
        {
            mouse.mouseUp(k, true);
        }
    }

    // 注销鼠标钩子
    if (hMouseHook != NULL || hMouseHook != nullptr)
    {
        if (hMouseHook != NULL)
        {
            // 移除鼠标钩子
            UnhookWindowsHookEx(hMouseHook);
            hMouseHook = NULL; // 将全局变量置为空
        }
    }
}

} // namespace wtl
