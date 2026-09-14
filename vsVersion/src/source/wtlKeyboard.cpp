#include "winToolsH.hpp"   // keyboard.hpp + wtl::StringCode + wtl::sleep + autoClear + _cleanupCheck
#include <interception.h>
#include <cctype>
#include <cstring>
#include <cstdlib>

using namespace std;

/********************** 键盘实现内部状态 **********************/
namespace
{
    // 键名 -> 扫描码映射（驱动级模拟使用）
    std::unordered_map<std::string, int> keyBoardCode = {
            // 字母键
            {"a", 0x1E}, {"b", 0x30}, {"c", 0x2E}, {"d", 0x20}, {"e", 0x12}, {"f", 0x21},
            {"g", 0x22}, {"h", 0x23}, {"i", 0x17}, {"j", 0x24}, {"k", 0x25}, {"l", 0x26},
            {"m", 0x32}, {"n", 0x31}, {"o", 0x18}, {"p", 0x19}, {"q", 0x10}, {"r", 0x13},
            {"s", 0x1F}, {"t", 0x14}, {"u", 0x16}, {"v", 0x2F}, {"w", 0x11}, {"x", 0x2D},
            {"y", 0x15}, {"z", 0x2C},

            // 数字键（主键盘区，非小键盘）
            {"0", 0x0B}, {"1", 0x02}, {"2", 0x03}, {"3", 0x04}, {"4", 0x05}, {"5", 0x06},
            {"6", 0x07}, {"7", 0x08}, {"8", 0x09}, {"9", 0x0A},

            // 功能键
            {"f1", 0x3B}, {"f2", 0x3C}, {"f3", 0x3D}, {"f4", 0x3E}, {"f5", 0x3F}, {"f6", 0x40},
            {"f7", 0x41}, {"f8", 0x42}, {"f9", 0x43}, {"f10", 0x44}, {"f11", 0x57}, {"f12", 0x58},

            // 导航键
            {"up", 0x48}, {"down", 0x50}, {"left", 0x4B}, {"right", 0x4D},
            {"home", 0x47}, {"end", 0x4F}, {"pageup", 0x49}, {"pagedown", 0x51},
            {"insert", 0x52}, {"delete", 0x53},

            // 控制键
            {"esc", 0x01},
            {"tab", 0x0F},
            {"caps", 0x3A},
            {"shift", 0x2A}, {"rshift", 0x36},
            {"ctrl", 0x1D},
            {"alt", 0x38},
            {"win", 0x5B}, {"rwin", 0x5C},
            {"apps", 0x5D}, // 应用程序键（右键菜单键）
            {"enter", 0x1C},
            {"space", 0x39},
            {"backspace", 0x0E},

            // 符号键
            {"`", 0x29}, {"-", 0x0C}, {"=", 0x0D}, {"[", 0x1A}, {"]", 0x1B},
            {"\\", 0x2B}, {";", 0x27}, {"'", 0x28}, {",", 0x33}, {".", 0x34}, {"/", 0x35},

            // 数字键盘
            {"num0", 0x52}, {"num1", 0x4F}, {"num2", 0x50}, {"num3", 0x51},
            {"num4", 0x4B}, {"num5", 0x4C}, {"num6", 0x4D}, {"num7", 0x47},
            {"num8", 0x48}, {"num9", 0x49},
            {"num*", 0x37}, {"num+", 0x4E}, {"num-", 0x4A}, {"num.", 0x53}, {"num/", 0x35},
            {"numenter", 0x1C},

            // 其他
            {"scrolllock", 0x46}, {"pause", 0x45}, {"printscreen", 0x37}
    };

    // 驱动级按键按下记录（用于程序退出时清理）
    std::unordered_map<std::string, bool> historykeyDown;

    // 单个按键监听状态标记
    std::atomic<bool> OneKeyEventProcessed(true);

    // 在全局变量区新增状态标记 单个按键监听 //
    std::atomic<bool> listenHotKey(true);  // 原子操作保证线程安全

    /*********** 虚键码对照表 Virtual key code mapping table **************/
    string alphabet_list[] = {"a", "b", "c", "d", "e", "f", "g",
                              "h", "i", "j", "k", "l", "m",
                              "n", "o", "p", "q", "r", "s",
                              "t", "u", "v", "w", "x", "y", "z"  // 26字母
            ,"0", "1", "2", "3", "4", "5", "6"
            , "7", "8", "9"};  // 按键列  // 字母列表

    string function_keys_list[] = {"ctrl", "alt", "shift", "f1", "f2", "f3", "f4", "f5",
                                   "f6", "f7", "f8", "f9", "f10", "f11", "f12", "esc",
                                   "space", "delete", "tab", "enter", "caps", "clear",
                                   "backspace", "win", "pause", "page_up", "page_down", "left_arrow",
                                   "right_arrow", "down_arrow", "up_arrow", "insert", "`", "[", "]",
                                   "\\", ";", "''", ",", ".", "/","-", "=", "，",
                                   "。", "’", "‘"
    };

    /**************************** 符合虚键码列表 ********************************/
    string symbol_list_not_shift_press[] = {"`", "[", "]", "\\", ";", "''", ",", ".", "/",
                                            "-", "=", "，", "。", "’", "‘"};

    string symbol_list_need_shift_press[] = {"~", "{", "}", "|", ":", "\"\"", "<", ">", "?",
                                             "_", "+","!", "@", "#", "$", "%", "^",
                                             "&", "*", "(",")"};

    string chinese_symbol_shift_press[] = {"：", "“”", "《", "》", "？",
                                           "——", "！" , "￥", "……","（","）"};

    /****************************虚键码列表********************************/
    int alphabet_code[] = {65, 66, 67, 68, 69, 70,
                           71, 72, 73, 74, 75, 76,
                           77, 78, 79, 80, 81, 82,
                           83, 84, 85, 86, 87, 88,
                           89, 90, 48, 49, 50, 51,  // 48 以后是数组
                           52, 53, 54, 55, 56, 57};  // 虚拟键码

    int function_code[] = {VK_CONTROL, VK_MENU, VK_SHIFT, VK_F1, VK_F2, VK_F3,
                           VK_F4, VK_F5, VK_F6, VK_F7, VK_F8, VK_F9, VK_F10,
                           VK_F11, VK_F12, VK_ESCAPE, VK_SPACE, VK_DELETE, VK_TAB,
                           VK_RETURN, VK_CAPITAL, VK_CLEAR, VK_BACK, VK_LWIN, VK_PAUSE,
                           VK_PRIOR, VK_NEXT, VK_LEFT, VK_RIGHT, VK_DOWN, VK_UP, VK_INSERT,
                           VK_OEM_3, VK_OEM_4, VK_OEM_6, VK_OEM_5, VK_OEM_1, VK_OEM_7, 188,
                           190, VK_OEM_2, VK_OEM_MINUS, VK_OEM_PLUS, 188, 190, VK_OEM_7,
                           VK_OEM_7};

    //虚键码列表//
    int symbol_not_shift_code[] = {VK_OEM_3, VK_OEM_4, VK_OEM_6, VK_OEM_5, VK_OEM_1, VK_OEM_7, 188,
                                   190, VK_OEM_2, VK_OEM_MINUS, VK_OEM_PLUS, 188, 190, VK_OEM_7,
                                   VK_OEM_7};

    int symbol_need_shift_code[] = {VK_OEM_3, VK_OEM_4, VK_OEM_6, VK_OEM_5, VK_OEM_1, VK_OEM_7, 188,
                                    190, VK_OEM_2, VK_OEM_MINUS, VK_OEM_PLUS, 49,
                                    50, 51,52, 53, 54, 55, 56, 57, 48 };

    int chinese_symbol_shift_code[] = {VK_OEM_1, VK_OEM_7, 188,190, VK_OEM_2,
                                       VK_OEM_MINUS,  49,52, 53, 57, 48 };

    /************* press up flag **************/
    // 检查键盘是否松开结构体 //
    struct pressHotKeyNameUp
    {
        int key_num;  // 按下的数量
        int key_code[300];
    };
    pressHotKeyNameUp free_keys = {0, 0}; // 释放按键

    // 字符串复制到剪切板 //
    struct copy_str_structs  // 字符串剪切的结构体(防止后续重复利用相同字符串)
    {
        const char* textToCopy;  // 一个常量指针字符串
        void (*_copyStrIn)(struct copy_str_structs);  // 复制到剪切板的函数，后续包裹到copyStr中
    };

    /*************** 快捷键注册表 ****************/
    // 修饰键位掩码 //
    enum ModMask : unsigned
    {
        WTL_MOD_CTRL  = 1u,
        WTL_MOD_ALT   = 2u,
        WTL_MOD_SHIFT = 4u,
        WTL_MOD_WIN   = 8u
    };

    // 一条快捷键注册项 //
    struct HotKey
    {
        unsigned mods;     // 需要按住的修饰键掩码
        int      trigger;  // 主键虚键码；为 0 表示"纯修饰键"热键（如 ctrl、ctrl+shift）
        std::function<void()> func;
    };

    // 快捷键注册表，注册顺序即回调顺序 //
    vector<HotKey> hotKeyTable;

    /*************** 钩子内维护的按键状态 ****************/
    unsigned curMods     = 0;      // 当前按住的修饰键掩码
    bool     modsConsumed = false; // 本轮修饰键按住期间是否已被组合键消费
    bool     keyIsDown[256] = {};  // 各虚键码是否处于按下态，用于过滤自动重复

    // 把虚键码归一化为修饰键掩码，非修饰键返回 0 //
    // 低级钩子送来的是 VK_LCONTROL/VK_RCONTROL 这类左右分明的码，需一并归一 //
    inline unsigned _modBitOf(DWORD vk)
    {
        switch (vk)
        {
            case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL: return WTL_MOD_CTRL;
            case VK_MENU:    case VK_LMENU:    case VK_RMENU:    return WTL_MOD_ALT;
            case VK_SHIFT:   case VK_LSHIFT:   case VK_RSHIFT:   return WTL_MOD_SHIFT;
            case VK_LWIN:    case VK_RWIN:                       return WTL_MOD_WIN;
            default:                                             return 0;
        }
    }

    // 直接向系统查询修饰键状态，用于校正钩子安装前就已按住的修饰键造成的状态漂移 //
    inline unsigned _queryMods()
    {
        unsigned m = 0;
        if (GetAsyncKeyState(VK_CONTROL) & 0x8000) m |= WTL_MOD_CTRL;
        if (GetAsyncKeyState(VK_MENU)    & 0x8000) m |= WTL_MOD_ALT;
        if (GetAsyncKeyState(VK_SHIFT)   & 0x8000) m |= WTL_MOD_SHIFT;
        if ((GetAsyncKeyState(VK_LWIN) & 0x8000) || (GetAsyncKeyState(VK_RWIN) & 0x8000)) m |= WTL_MOD_WIN;
        return m;
    }

    // 快捷键钩子句柄 //
    HHOOK hHook = nullptr;  // HHOOK 类型用于保存钩子句柄

    // 单个按键监听 //
    struct ONE_KEY_CODE_STRUCT
    {
        long ONE_KEY_CODE;
        bool real_time;
    };
    ONE_KEY_CODE_STRUCT ONE_KEY_CODES = {0, false};

    HHOOK hOneKeyEvent = nullptr;  // 钩子句柄
    std::vector<bool> keyStates(256, false); // 跟踪256个虚拟键的状态

    // 监听键盘按下与抬起的虚拟键码  暂时无用  //
    long long ListenOneKeyCodeArr[256] = {
            65, 66, 67, 68, 69, 70, 71, 72, 73, 74,
            75, 76, 77, 78, 79, 80, 81, 82,
            83, 84, 85, 86, 87, 88, 89, 90,
            48, 49, 50, 51, 52, 53, 54, 55, 56, 57,
            112, 113, 114, 115, 116, 117, 118, 119,
            120, 121, 122, 123, 27, 9, 13, 8, 32,
            127, 38, 40, 37, 39, 162, 164, 160, 20
    };

    // 剪贴板编码转换辅助 //
    static wtl::StringCode scd_cpp;

    /**************** 键盘按下记录 ****************/
    // 键盘按下记录方便后期查询是否释放 //
    void _keyDownRecord(int key_code)  // 记录键盘按下 (避免重复)
    {
        int len = sizeof(free_keys.key_code) / sizeof(free_keys.key_code[0]);  // 检查按键数组大小
        bool found = false;  // 标记是否找到按键码

        // 遍历数组，检查按键码是否已经存在
        for (int i = 0; i < len; i++)
        {
            if (free_keys.key_code[i] == key_code)
            {
                found = true;  // 找到按键码，设置标记为true
                break;
            }
        }

        // 如果没有找到按键码，添加到数组的第一个空位置
        if (!found)  // 如果没找到 就重新遍历数组 寻找值为0的位置 将虚键码赋值在上面
        {
            for (int i = 0; i < len; i++)
            {
                if (free_keys.key_code[i] == 0)  // 找到数组的第一个空位置 (找到数组为0的位置，将虚键码赋值到上面)
                {
                    free_keys.key_code[i] = key_code;
                    break;
                }
            }
        }
    }

    // 记录键盘松开，清空按键 //
    void _keyUpRecord(int key_code)
    {
        int len = sizeof(free_keys.key_code) / sizeof(free_keys.key_code[0]);  // 检查释放键盘的数组大小
        for (int i = 0; i < len; i++)
        {
            if (free_keys.key_code[i] == key_code)
            {
                free_keys.key_code[i] = 0;  // 清空按键码
                break;
            }
        }
    }

    /**************** 字符串输出辅助 ****************/
    // 辅助函数，主要用于输出 //
    void _sendUnicodeChar(wchar_t character)
    {
        INPUT inputs[2] = {};

        // 按下事件
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = 0;  // 虚拟键码设为0
        inputs[0].ki.wScan = character;  // Unicode字符编码
        inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;  // 关键标志

        // 释放事件
        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = 0;
        inputs[1].ki.wScan = character;
        inputs[1].ki.dwFlags = KEYEVENTF_KEYUP | KEYEVENTF_UNICODE;

        SendInput(2, inputs, sizeof(INPUT));
    }

    // 发送回车键（换行）
    void _sendEnterKey()
    {
        INPUT input = {0};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = VK_RETURN;  // 回车键
        input.ki.dwFlags = 0;
        SendInput(1, &input, sizeof(INPUT));

        input.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &input, sizeof(INPUT));
    }

    /*********** 剪切板 ******************/
    // 字符串拷贝辅助函数 //
    void _copyStrIn(copy_str_structs self)  // 复制到剪切板的函数，后续包裹在copyStr函数中
    {
        if (OpenClipboard(NULL))
        {
            // 清空剪切板内容
            EmptyClipboard();

            // 分配内存并将文本内容复制到全局内存块
            HGLOBAL hglbCopy = GlobalAlloc(GMEM_MOVEABLE, strlen(self.textToCopy) + 1); // +1是为了包含字符串的结尾 null 字符
            LPVOID lpCopy = NULL;
            if (hglbCopy != NULL)
            {
                lpCopy = GlobalLock(hglbCopy);
                strcpy((char*)lpCopy, self.textToCopy);
                GlobalUnlock(hglbCopy);

                // 将全局内存块设置为剪切板内容
                SetClipboardData(CF_TEXT, hglbCopy);
            }
            else
            {
                // 如果hglbCopy分配失败，检查lpCopy是否已经被获取并释放
                if (lpCopy != NULL)
                {
                    GlobalFree(lpCopy);
                }
            }

            // 关闭剪切板
            CloseClipboard();
        }
    }

    /*************** 键盘钩子回调 ****************/
    // 收集命中的回调后再统一执行，避免回调内部注册/清理快捷键导致遍历中的容器失效 //
    void _fireMatched(unsigned mods, int trigger)
    {
        vector<std::function<void()>> pending;
        for (const auto& hk : hotKeyTable)
        {
            // 修饰键掩码要求完全相等：注册 ctrl+e 时按下 ctrl+shift+e 不算命中 //
            if (hk.trigger == trigger && hk.mods == mods)
            {
                pending.push_back(hk.func);
            }
        }
        for (auto& f : pending)
        {
            if (f)
            {
                f();
            }
        }
    }

    // 键盘钩子的回调函数（快捷键监听）
    LRESULT CALLBACK _keyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
    {
        if (nCode < 0)
        {
            return CallNextHookEx(hHook, nCode, wParam, lParam);
        }

        PKBDLLHOOKSTRUCT pKey = (PKBDLLHOOKSTRUCT)lParam;
        const DWORD    vk     = pKey->vkCode;
        const unsigned bit    = _modBitOf(vk);
        const bool     isDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
        const bool     isUp   = (wParam == WM_KEYUP   || wParam == WM_SYSKEYUP);

        if (isDown)
        {
            if (bit)
            {
                // 修饰键按下：只更新状态，此刻无法判定它是独立热键还是组合键前缀，故不触发 //
                if (curMods == 0)
                {
                    modsConsumed = false;  // 新一轮修饰键组合开始
                }
                curMods |= bit;
            }
            else if (vk < 256 && !keyIsDown[vk])
            {
                // 普通键首次按下（keyIsDown 过滤掉按住不放产生的自动重复） //
                keyIsDown[vk] = true;
                curMods = _queryMods();  // 以系统状态为准，纠正可能的漂移
                modsConsumed = true;    // 修饰键已被组合键消费，松开时不再触发纯修饰键热键
                _fireMatched(curMods, (int)vk);
            }
        }
        else if (isUp)
        {
            if (bit)
            {
                const unsigned before = curMods;  // 记录松开前的掩码
                curMods &= ~bit;

                // 纯修饰键热键推迟到松开才判定：按住期间没被组合键消费才算"单独按了它" //
                if (!modsConsumed)
                {
                    _fireMatched(before, 0);
                    modsConsumed = true;  // 一轮组合只触发一次
                }
                if (curMods == 0)
                {
                    modsConsumed = false;  // 修饰键全部松开，重置本轮状态
                }
            }
            else if (vk < 256)
            {
                keyIsDown[vk] = false;
            }
        }

        return CallNextHookEx(hHook, nCode, wParam, lParam);
    }

    // 实时获取按键 //
    LRESULT CALLBACK _oneKeyEventRealTimeProc(int nCode, WPARAM wParam, LPARAM lParam)
    {
        if (nCode == HC_ACTION) {
            KBDLLHOOKSTRUCT* pKeyInfo = (KBDLLHOOKSTRUCT*)lParam;
            const int vkCode = pKeyInfo->vkCode;

            switch (wParam) {
                case WM_KEYDOWN:
                case WM_SYSKEYDOWN:
                    ONE_KEY_CODES.ONE_KEY_CODE = vkCode; // 按下时返回正数
                    break;
                case WM_KEYUP:
                case WM_SYSKEYUP:
                    ONE_KEY_CODES.ONE_KEY_CODE = -vkCode; // 抬起时返回负数
                    break;
            }
            OneKeyEventProcessed.store(false);
        }
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    // 不实时返回虚假码 //
    LRESULT CALLBACK _oneKeyEventProc(int nCode, WPARAM wParam, LPARAM lParam)
    {
        if (nCode == HC_ACTION)
        {
            auto* pKeyInfo = (KBDLLHOOKSTRUCT*)lParam;
            const int vkCode = pKeyInfo->vkCode;

            switch (wParam)
            {
                case WM_KEYDOWN:
                case WM_SYSKEYDOWN:
                    if (!keyStates[vkCode])
                    {
                        ONE_KEY_CODES.ONE_KEY_CODE = vkCode; // 按下时返回正数
                        keyStates[vkCode] = true;
                    }
                    break;
                case WM_KEYUP:
                case WM_SYSKEYUP:
                    if (keyStates[vkCode])
                    {
                        ONE_KEY_CODES.ONE_KEY_CODE = -vkCode; // 抬起时返回负数
                        keyStates[vkCode] = false;
                    }
                    break;
            }
            OneKeyEventProcessed.store(false);
        }
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }
} // namespace

/************************* 键盘事件实现 ****************************/
namespace wtl
{

KeyBoardEvent::KeyBoardEvent()
{
    // 检查是否注册末尾清理函数，False为没有注册 //
    if (!autoClear)
    {
        exitCheckWork();
    }
}

// 键盘按下 //
void KeyBoardEvent::keyDown(const string& key)
{
    /****************列表长度************************/
    int alphabet_list_len = sizeof(alphabet_list) / sizeof(alphabet_list[0]);  // 字母数字长度
    int function_keys_list_len = sizeof(function_keys_list) / sizeof(function_keys_list[0]);  // 功能键数组长度

    string key_upper = key;
    transform(key_upper.begin(), key_upper.end(), key_upper.begin(), ::tolower);

    for (int i = 0; i < alphabet_list_len; i++)
    {
        // 检查数组的字符是否与传入的参数一样(判断字母) 如果都没进入下一个循环
        if (key_upper == alphabet_list[i])
        {
            INPUT input; // 声明一个 INPUT 结构体变量 input，用于描述按键事件

            input.type = INPUT_KEYBOARD; // 指定 input 的类型为键盘输入
            input.ki.wScan = 0; // 扫描码置为 0，通常不需要使用
            input.ki.time = 0; // 时间戳置为 0
            input.ki.dwExtraInfo = 0; // 额外信息置为 0

            // 模拟按下目标键
            input.ki.wVk = alphabet_code[i]; // 指定模拟按下的键为 目标 键
            input.ki.dwFlags = 0; // 指定键盘按下事件，dwFlags 为 0
            SendInput(1, &input, sizeof(INPUT)); // 发送按键事件给系统

            _keyDownRecord(alphabet_code[i]);
            break;
        }
    }

    for (int i = 0; i < function_keys_list_len; i++)
    {
        // 是否与传入的参数一样 (判断功能键)
        if (key_upper == function_keys_list[i])
        {
            INPUT input; // 声明一个 INPUT 结构体变量 input，用于描述按键事件

            input.type = INPUT_KEYBOARD; // 指定 input 的类型为键盘输入
            input.ki.wScan = 0; // 扫描码置为 0，通常不需要使用
            input.ki.time = 0; // 时间戳置为 0
            input.ki.dwExtraInfo = 0; // 额外信息置为 0

            // 模拟按下目标键
            input.ki.wVk = function_code[i]; // 指定模拟按下的键为 目标 键
            input.ki.dwFlags = 0; // 指定键盘按下事件，dwFlags 为 0
            SendInput(1, &input, sizeof(INPUT)); // 发送按键事件给系统

            _keyDownRecord(function_code[i]);
            break;
        }
    }
}

// 驱动级模拟 //
void KeyBoardEvent::keyDown(const string& key, bool drivingStage)
{
    // false 回退系统级 //
    if (!drivingStage)
    {
        keyDown(key);
        return;
    }

    InterceptionContext context = interception_create_context();
    if (!context)
    {
        std::cerr << "Failed to create interception context" << std::endl;
        return;
    }

    interception_set_filter(context, interception_is_keyboard, INTERCEPTION_FILTER_KEY_DOWN | INTERCEPTION_FILTER_KEY_UP);

    InterceptionKeyStroke stroke;
    stroke.code = keyBoardCode[key];
    stroke.state = INTERCEPTION_KEY_DOWN;

    interception_send(context, INTERCEPTION_KEYBOARD(0), (const InterceptionStroke*)&stroke, 1);
    historykeyDown[key] = true;

    interception_destroy_context(context);
}

// 键盘释放 //
void KeyBoardEvent::keyUp(const string& key)
{
    int len = sizeof(free_keys.key_code) / sizeof(free_keys.key_code[0]);
    /****************列表长度************************/
    int alphabet_list_len = sizeof(alphabet_list) / sizeof(alphabet_list[0]);  // 字母数字长度
    int function_keys_list_len = sizeof(function_keys_list) / sizeof(function_keys_list[0]);  // 功能键数组长度

    string key_upper = key;
    transform(key_upper.begin(), key_upper.end(), key_upper.begin(), ::tolower);

    for (int i = 0; i < alphabet_list_len; i++) {
        // 检查数组的字符是否与传入的参数一样(判断字母) 如果都没进入下一个循环
        if (key_upper == alphabet_list[i]) {
            INPUT input = { 0 };
            input.type = INPUT_KEYBOARD;
            input.ki.wVk = alphabet_code[i];  // 键位吗码
            // 如果之前按键被按下了，现在需要释放
            input.ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(1, &input, sizeof(INPUT));

            _keyUpRecord(alphabet_code[i]);
            break;
        }
    }

    for (int i = 0; i < function_keys_list_len; i++)
    {
        // 是否与传入的参数一样 (判断功能键)
        if (key_upper == function_keys_list[i])
        {
            INPUT input = { 0 };
            input.type = INPUT_KEYBOARD;
            input.ki.wVk = function_code[i];  // 键位吗码
            // 如果之前按键被按下了，现在需要释放
            input.ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(1, &input, sizeof(INPUT));

            _keyUpRecord(function_code[i]);
            break;
        }
    }
}

// 驱动级模拟 //
void KeyBoardEvent::keyUp(const string& key, bool drivingStage)
{
    // false 回退系统级 //
    if (!drivingStage)
    {
        keyDown(key);
        return;
    }

    InterceptionContext context = interception_create_context();
    if (!context)
    {
        std::cerr << "Failed to create interception context" << std::endl;
        return;
    }

    interception_set_filter(context, interception_is_keyboard, INTERCEPTION_FILTER_KEY_DOWN | INTERCEPTION_FILTER_KEY_UP);

    InterceptionKeyStroke stroke;
    stroke.code = keyBoardCode[key];
    stroke.state = INTERCEPTION_KEY_UP;

    interception_send(context, INTERCEPTION_KEYBOARD(0), (const InterceptionStroke*)&stroke, 1);
    historykeyDown[key] = false;

    interception_destroy_context(context);
}

// 键盘点击 //
void KeyBoardEvent::pressKey(const std::string& key)
{
    keyDown(key);
    keyUp(key);
}

// 驱动级模拟 //
void KeyBoardEvent::pressKey(const string& key, bool drivingStage)
{
    // false 回退系统级 //
    if (!drivingStage)
    {
        keyDown(key);
        return;
    }

    keyDown(key, true);
    keyUp(key, true);
}

void KeyBoardEvent::writeStr(const string &output, double delay)
{
    fs::path writeStr = output;
    wstring text = writeStr.wstring();

    double actualDelay = (delay > 0) ? delay : 0.01;

    for (size_t i = 0; i < text.length(); ++i)
    {
        wchar_t ch = text[i];
        bool isLastChar = (i == text.length() - 1);

        if (ch == L'\n')
        {
            _sendEnterKey();
            // 换行符后不延迟，或者使用更短的延迟
            if (!isLastChar) wtl::sleep(actualDelay * 0.5);
        }
        else if (ch == L'\r')
        {
            continue;
        }
        else
        {
            _sendUnicodeChar(ch);
            if (!isLastChar) wtl::sleep(actualDelay);
        }
    }
}

// 赋值到剪切板 //
void KeyBoardEvent::copyStr(const char* str)
{
    // 复制字符窜到剪切板
    copy_str_structs self = { str, _copyStrIn };
    self._copyStrIn(self);
}

// 获取剪切板中的第一个文本数据
std::string KeyBoardEvent::getCopyStr()
{
    if (!OpenClipboard(nullptr)) {
        return ""; // 无法打开剪切板
    }

    HANDLE hData = GetClipboardData(CF_TEXT);
    if (hData == nullptr) {
        CloseClipboard();
        return ""; // 剪切板中没有文本数据
    }

    char* pszText = static_cast<char*>(GlobalLock(hData));
    if (pszText == nullptr) {
        CloseClipboard();
        return "";
    }

    std::string result(pszText);

    GlobalUnlock(hData);
    CloseClipboard();

    return scd_cpp.gbkToUtf8(result);
}

// 获取单个按键虚键码并返回 //
int KeyBoardEvent::_getOneKeyCode(string key_name)
{
    // 初始化索引值为第一个 //
    int index = 0;

    // 变量字母的列表，查找虚键码 //
    for (auto i : alphabet_list)
    {
        // 如果与传入的字符串相等，截取索引值获得虚键码并返回，反之索引增加 //
        if (key_name == i)
        {
            return alphabet_code[index];
        }
        index++;
    }

    // 如果第一轮没有找到，归零索引重新在功能名称找对应虚键码 //
    index = 0;

    // 变量字母的列表，查找虚键码 //
    for (auto i : function_keys_list)
    {
        // 如果与传入的字符串相等，截取索引值获得虚键码并返回，反之索引增加 //
        if (key_name == i)
        {
            return function_code[index];
        }
        index++;
    }

    // 如果都找不到说明快捷键注册不合法，返回-1 //
    cout << "The shortcut name does not exist. Make sure to register a valid shortcut\n";
    return  -1;
}

// 注册快捷键：把 "ctrl+alt+t" 解析成 修饰键掩码 + 主键，登记到快捷键表 //
// 支持任意数量修饰键，且允许只注册修饰键本身（如 "ctrl"、"ctrl+shift"）//
void KeyBoardEvent::addHotKey(const string& key, std::function<void()> targetVoidFunc)
{
    if (!targetVoidFunc)
    {
        cout << "addHotKey: the callback is empty, registration skipped\n";
        return;
    }

    // 按 '+' 切分键名，顺带去掉空白；这样 "-" "[" "/" 等符号键也能正确取到 //
    vector<string> tokens;
    string cur;
    for (char ch : key)
    {
        if (ch == '+')
        {
            tokens.push_back(cur);
            cur.clear();
        }
        else if (!isspace(static_cast<unsigned char>(ch)))
        {
            cur.push_back(ch);
        }
    }
    tokens.push_back(cur);

    unsigned mods       = 0;      // 修饰键掩码
    int      trigger    = 0;      // 主键虚键码，0 表示纯修饰键热键
    bool     hasTrigger = false;

    for (auto& token : tokens)
    {
        if (token.empty())
        {
            continue;  // "ctrl++" 这类写法产生的空片段直接跳过
        }
        transform(token.begin(), token.end(), token.begin(), ::tolower);

        int vk = _getOneKeyCode(token);
        if (vk == -1)
        {
            // _getOneKeyCode 已提示具体键名无效，这里补上是哪条注册被丢弃 //
            cout << "addHotKey: \"" << key << "\" registration skipped\n";
            return;
        }

        unsigned bit = _modBitOf(static_cast<DWORD>(vk));
        if (bit)
        {
            mods |= bit;
        }
        else if (hasTrigger)
        {
            cout << "addHotKey: \"" << key
                 << "\" has more than one non-modifier key, registration skipped\n";
            return;
        }
        else
        {
            trigger    = vk;
            hasTrigger = true;
        }
    }

    if (mods == 0 && !hasTrigger)
    {
        cout << "addHotKey: \"" << key << "\" resolves to no key, registration skipped\n";
        return;
    }

    hotKeyTable.push_back(HotKey{mods, trigger, std::move(targetVoidFunc)});
}

// 等待快捷键 //
void KeyBoardEvent::waitHotKey()
{
    // 如果已经注册那就删除重新注册 //
    if (hHook)
    {
        uninstallHotKeyEvent();
        hHook = nullptr;
    }

    // 监听快捷键循环函数
    hHook = SetWindowsHookEx(WH_KEYBOARD_LL, _keyboardProc, GetModuleHandle(nullptr), 0);  // 安装低级键盘钩子

    MSG msg;          // MSG 结构体用于存储从消息队列中获取的消息

    // 进入消息循环
    while (GetMessage(&msg, nullptr, 0, 0))  // 从消息队列中获取消息
    {
        TranslateMessage(&msg);           // 将虚拟键码转换成字符
        DispatchMessage(&msg);            // 分发消息到窗口过程
    }
}

// 函数为 static clearHotKey 类成员成员函数调用清理快捷键函数 //
void KeyBoardEvent::clearHotKey()
{
    if (hHook != NULL || hHook != nullptr)
    {
        // 确保钩子被卸载
        UnhookWindowsHookEx(hHook);       // 卸载钩子
        hHook = nullptr;  // 重置钩子句柄

        // 发送程序结束消息
        PostQuitMessage(0);
        exit(0);
    }
}

void KeyBoardEvent::uninstallHotKeyEvent()
{
    if (hHook != NULL || hHook != nullptr)
    {
        // 确保钩子被卸载
        UnhookWindowsHookEx(hHook);       // 卸载钩子
        hHook = nullptr;  // 重置钩子句柄
    }
}

/****************** 单个按键监听部分 *******************/

// 监听单个快捷键事件 //
void KeyBoardEvent::listenOneKeyEvent(bool real_time)
{
    wtl::StringCode scd;
    // 如果已经注册，注销重新注册 //
    if (hOneKeyEvent)
    {
        uninstallOneKeyEvent();
        hOneKeyEvent = nullptr;
    }

    // 判断是否需要实时监听 //
    if (real_time)
    {
        ONE_KEY_CODES.real_time = real_time;

        // 设置低级键盘钩子 实时监听//
        hOneKeyEvent = SetWindowsHookEx(WH_KEYBOARD_LL, _oneKeyEventRealTimeProc, GetModuleHandle(NULL), 0);

        if (hOneKeyEvent == nullptr) {
            std::cerr << scd.utf8ToGbk("钩子安装失败! 错误代码: ") << GetLastError() << std::endl;
            return;
        }

        // 消息循环（必须存在以确保钩子正常工作）
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    else
    {
        ONE_KEY_CODES.real_time = real_time;

        // 设置低级键盘钩子 非实时监听//
        hOneKeyEvent = SetWindowsHookEx(WH_KEYBOARD_LL, _oneKeyEventRealTimeProc, GetModuleHandle(NULL), 0);

        if (hOneKeyEvent == nullptr)
        {
            std::cerr << "钩子安装失败! 错误代码: " << GetLastError() << std::endl;
            return;
        }

        // 消息循环（必须存在以确保钩子正常工作）
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
}

// 默认不实时返回按键码仅在变化时候反馈 //
void KeyBoardEvent::listenOneKeyEvent()
{
    ONE_KEY_CODES.real_time = false;
    wtl::StringCode scd;

    // 如果已经注册，注销重新注册 //
    if (hOneKeyEvent)
    {
        uninstallOneKeyEvent();
        hOneKeyEvent = nullptr;
    }

    // 设置低级键盘钩子 非实时监听//
    hOneKeyEvent = SetWindowsHookEx(WH_KEYBOARD_LL, _oneKeyEventProc, GetModuleHandle(NULL), 0);

    if (hOneKeyEvent == nullptr)
    {
        std::cerr << scd.utf8ToGbk("钩子安装失败! 错误代码: ") << GetLastError() << std::endl;
        return;
    }

    // 消息循环（必须存在以确保钩子正常工作）
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

long KeyBoardEvent::getOneKeyCode()
{
    if (ONE_KEY_CODES.real_time)
    {
        if (!OneKeyEventProcessed.load())
        {
            OneKeyEventProcessed.store(true);
            long currentCode = ONE_KEY_CODES.ONE_KEY_CODE;
            ONE_KEY_CODES.ONE_KEY_CODE = 0; // 重置为0，避免重复处理
            return currentCode;
        }
    }
    else
    {
        if (!OneKeyEventProcessed.load())
        {
            OneKeyEventProcessed.store(true);
            long currentCode = ONE_KEY_CODES.ONE_KEY_CODE;
            ONE_KEY_CODES.ONE_KEY_CODE = 0; // 重置为0，避免重复处理
            return currentCode;
        }
    }
    return 0;
}

// 退出单个按键事件箭头 //
void KeyBoardEvent::exitOneKeyEvent()
{
    if (hOneKeyEvent != NULL || hOneKeyEvent != nullptr)
    {
        // 确保钩子被卸载
        UnhookWindowsHookEx(hOneKeyEvent);       // 卸载钩子
        hOneKeyEvent = nullptr;  // 重置钩子句柄
        // 发送程序结束消息 //
        PostQuitMessage(0);
        exit(0); // 退出程序
    }
}

// 注销事件但是不退出程序 //
void KeyBoardEvent::uninstallOneKeyEvent()
{
    if (hOneKeyEvent != NULL || hOneKeyEvent != nullptr)
    {
        // 确保钩子被卸载
        UnhookWindowsHookEx(hOneKeyEvent);       // 卸载钩子
        hOneKeyEvent = nullptr;  // 重置钩子句柄
    }
}

// 程序退出时的键盘资源清理 //
void KeyBoardEvent::cleanup()
{
    // 1. 释放未松开的普通按键
    int len = sizeof(free_keys.key_code) / sizeof(free_keys.key_code[0]);
    for (int f = 0; f < len; f++)
    {
        if (free_keys.key_code[f] != 0)
        {
            INPUT input = { 0 };
            input.type = INPUT_KEYBOARD;
            input.ki.wVk = free_keys.key_code[f];  // 键位吗码
            input.ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(1, &input, sizeof(INPUT));
        }
    }

    // 2. 清理驱动级键盘操作
    KeyBoardEvent kbd;
    for (auto &[k, v] : historykeyDown)
    {
        if (v)
        {
            kbd.keyUp(k, true);
        }
    }

    // 3. 注销单按键监听
    if (hOneKeyEvent != NULL || hOneKeyEvent != nullptr)
    {
        UnhookWindowsHookEx(hOneKeyEvent);       // 卸载钩子
        hOneKeyEvent = nullptr;  // 重置钩子句柄
    }

    // 4. 注销快捷键监听
    if (hHook != NULL || hHook != nullptr)
    {
        UnhookWindowsHookEx(hHook);       // 卸载钩子
        hHook = nullptr;  // 重置钩子句柄
    }
}

} // namespace wtl
