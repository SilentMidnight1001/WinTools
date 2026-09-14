#pragma once
/**
 * keyboard.hpp — 键盘事件库（C++17 / Windows）
 *
 * 仅暴露功能 API 接口，所有实现细节（Windows API、Interception 驱动、
 * 键盘钩子等）均位于 keyboard.cpp，避免使用方编译时引入过多头文件。
 *
 * 使用：
 *   wtl::KeyBoardEvent keyboard;
 *   keyboard.pressHotKey("ctrl", "alt", "delete");
 *   keyboard.addHotKey("ctrl+alt+t", [](){ ... });
 */

#include <string>
#include <functional>

namespace wtl
{
/*************** 键盘事件码枚举 ***************/
// 按下为正数（虚键码），抬起为负数（-虚键码） //
namespace keyCode
{
    enum KEYCODEENUM {
        /*************** 按下虚键码整数 ****************/
        // 26字母键值码按下 //
        KEY_A_DOWN=65, KEY_B_DOWN=66, KEY_C_DOWN=67, KEY_D_DOWN=68, KEY_E_DOWN=69, KEY_F_DOWN=70,
        KEY_G_DOWN=71, KEY_H_DOWN=72, KEY_I_DOWN=73, KEY_J_DOWN=74, KEY_K_DOWN=75, KEY_L_DOWN=76,
        KEY_M_DOWN=77, KEY_N_DOWN=78, KEY_O_DOWN=79, KEY_P_DOWN=80, KEY_Q_DOWN=81, KEY_R_DOWN=82,
        KEY_S_DOWN=83, KEY_T_DOWN=84, KEY_U_DOWN=85, KEY_V_DOWN=86, KEY_W_DOWN=87, KEY_X_DOWN=88,
        KEY_Y_DOWN=89, KEY_Z_DOWN=90,

        // 0-9数字键值码 //
        KEY_0_DOWN=48, KEY_1_DOWN=49, KEY_2_DOWN=50, KEY_3_DOWN=51, KEY_4_DOWN=52, KEY_5_DOWN=53,
        KEY_6_DOWN=54, KEY_7_DOWN=55, KEY_8_DOWN=56, KEY_9_DOWN=57,

        // 右侧 //
        KEY_RIGHT_0_DOWN=96,KEY_RIGHT_1_DOWN=97,KEY_RIGHT_2_DOWN=98,KEY_RIGHT_3_DOWN=99,
        KEY_RIGHT_4_DOWN=100,KEY_RIGHT_5_DOWN=101,KEY_RIGHT_6_DOWN=102,KEY_RIGHT_7_DOWN=103,
        KEY_RIGHT_8_DOWN=104,KEY_RIGHT_9_DOWN=105,

        // 特殊键键值码 //
        KEY_F1_DOWN=112, KEY_F2_DOWN=113, KEY_F3_DOWN=114, KEY_F4_DOWN=115, KEY_F5_DOWN=116, KEY_F6_DOWN=117,
        KEY_F7_DOWN=118, KEY_F8_DOWN=119, KEY_F9_DOWN=120, KEY_F10_DOWN=121, KEY_F11_DOWN=122, KEY_F12_DOWN=123,
        KEY_ESC_DOWN=27, KEY_TAB_DOWN=9, KEY_ENTER_DOWN=13, KEY_BACKSPACE_DOWN=8, KEY_SPACE_DOWN=32, KEY_DELETE_DOWN=127,
        KEY_UP_DOWN=38, KEY_DOWN_DOWN=40, KEY_LEFT_DOWN=37, KEY_RIGHT_DOWN=39, KEY_CTRL_DOWN=162, KEY_ALT_DOWN=164,
        KEY_SHIFT_DOWN=160, KEY_CAPSLOCK_DOWN=20, KEY_CTRL_LEFT_DOWN=163, KEY_SHIFT_RIGHT_DOWN=161, KEY_ALT_RIGHT_DOWN=165,
        KEY_INS_DOWN=45,KEY_HOME_DOWN=36,KEY_PGUP_DOWN=33,KEY_DEL_DOWN=46,KEY_END_DOWN=35,
        KEY_RIGHT_SHIFT_DOWN=161,KEY_RIGHT_CTRL_DOWN=163,KEY_RIGHT_ALT_DOWN=165,KEY_NUM_DOWN=144,
        KEY_RIGHT_ADD_DOWN=107,KEY_RIGHT_SUB_DOWN=109,

        /*************** 抬起,虚键码负数 ****************/
        KEY_A_UP=-65, KEY_B_UP=-66, KEY_C_UP=-67, KEY_D_UP=-68, KEY_E_UP=-69, KEY_F_UP=-70,
        KEY_G_UP=-71, KEY_H_UP=-72, KEY_I_UP=-73, KEY_J_UP=-74, KEY_K_UP=-75, KEY_L_UP=-76,
        KEY_M_UP=-77, KEY_N_UP=-78, KEY_O_UP=-79, KEY_P_UP=-80, KEY_Q_UP=-81, KEY_R_UP=-82,
        KEY_S_UP=-83, KEY_T_UP=-84, KEY_U_UP=-85, KEY_V_UP=-86, KEY_W_UP=-87, KEY_X_UP=-88,
        KEY_Y_UP=-89, KEY_Z_UP=-90,  // 26字母键值码

        // 0-9数字键值码 //
        KEY_0_UP=-48, KEY_1_UP=-49, KEY_2_UP=-50, KEY_3_UP=-51, KEY_4_UP=-52, KEY_5_UP=-53,
        KEY_6_UP=-54, KEY_7_UP=-55, KEY_8_UP=-56, KEY_9_UP=-57,

        // 右侧 //
        KEY_RIGHT_0_UP=-96,KEY_RIGHT_1_UP=-97,KEY_RIGHT_2_UP=-98,KEY_RIGHT_3_UP=-99,
        KEY_RIGHT_4_UP=-100,KEY_RIGHT_5_UP=-101,KEY_RIGHT_6_UP=-102,KEY_RIGHT_7_UP=-103,
        KEY_RIGHT_8_UP=-104,KEY_RIGHT_9_UP=-105,

        KEY_F1_UP=-112, KEY_F2_UP=-113, KEY_F3_UP=-114, KEY_F4_UP=-115, KEY_F5_UP=-116, KEY_F6_UP=-117,
        KEY_F7_UP=-118, KEY_F8_UP=-119, KEY_F9_UP=-120, KEY_F10_UP=-121, KEY_F11_UP=-122, KEY_F12_UP=-123,
        KEY_ESC_UP=-27, KEY_TAB_UP=-9, KEY_ENTER_UP=-13, KEY_BACKSPACE_UP=-8, KEY_SPACE_UP=-32, KEY_DELETE_UP=-127,
        KEY_UP_UP=-38, KEY_DOWN_UP=-40, KEY_LEFT_UP=-37, KEY_RIGHT_UP=-39, KEY_CTRL_UP=-162, KEY_ALT_UP=-164,
        KEY_SHIFT_UP=-160, KEY_CAPSLOCK_UP=-20, KEY_CTRL_RIGHT_UP=-163, KEY_SHIFT_RIGHT_UP=-161, KEY_ALT_RIGHT_UP=-165,
        KEY_INS_UP=-45, KEY_HOME_UP=-36,KEY_PGUP_UP=-33,KEY_DEL_UP=-46,KEY_END_UP=-35,
        KEY_RIGHT_SHIFT_UP=-161,KEY_RIGHT_CTRL_UP=-163,KEY_RIGHT_ALT_UP=-165,KEY_NUM_UP=-144,
        KEY_RIGHT_ADD_UP=-107,KEY_RIGHT_SUB_UP=-109,
    };
}

    /*************** 键盘事件类 ***************/
    class KeyBoardEvent
    {
    private:
        int _getOneKeyCode(std::string key_name);

    public:
        KeyBoardEvent();

        // 功能部分
        void keyDown(const std::string& key);                 // 键盘按下
        void keyDown(const std::string& key, bool drivingStage);  // 驱动级按下

        void keyUp(const std::string& key);                   // 键盘释放
        void keyUp(const std::string& key, bool drivingStage);    // 驱动级释放

        void pressKey(const std::string& key);                // 按键（按下并释放）
        void pressKey(const std::string& key, bool drivingStage);

        // 组合键（支持任意数量，最多 4 个）
        template<typename... Args>
        void pressHotKey(const std::string& first, Args... rest);
        template<typename... Args>
        void pressHotKeyDriving(const std::string& first, Args... rest);
        void pressHotKey() {}          // 递归终止条件
        void pressHotKeyDriving() {}   // 驱动级递归终止条件

        void writeStr(const std::string& output, double delay = 0.0);  // 逐字符输入字符串

        void copyStr(const char* str);                        // 复制字符串到剪贴板
        std::string getCopyStr();                             // 获取剪贴板内容

        /**
         * 注册快捷键（普通函数与类成员函数均可）
         *
         * 键名以 '+' 分隔，大小写不敏感，修饰键可为 ctrl / alt / shift / win，
         * 数量不限；符号键（"-" "[" "/" 等）同样可用。例："ctrl+alt+shift+win+k"。
         *
         * 触发规则：
         *   1. 含主键的组合（如 "ctrl+e"）在主键按下时触发，且要求修饰键
         *      **完全一致**——注册 "ctrl+e" 时按 ctrl+shift+e 不会触发它。
         *   2. 纯修饰键热键（如 "ctrl"、"ctrl+shift"）在**松开时**才触发，
         *      且要求按住期间没有按过其他键。因此 "ctrl" 与 "ctrl+e" 可以共存：
         *      按 ctrl+e 只触发 ctrl+e，单独按一下 ctrl 才触发 ctrl。
         *      这个延迟是语义上必需的——按下的瞬间无法区分"独立热键"和"组合键前缀"。
         *   3. 按住不放产生的自动重复会被过滤，一次按下只回调一次。
         *
         * 注意：回调在键盘钩子线程内**同步**执行。Windows 对低级钩子有
         * LowLevelHooksTimeout 限制（默认 300ms），回调若超时，系统会**静默卸载钩子**，
         * 表现为快捷键突然全部失灵且无任何报错。因此回调必须尽快返回，
         * 耗时操作（读写文件、弹窗、网络请求等）请自行另开线程处理。
         */
        void addHotKey(const std::string& key, std::function<void()> targetVoidFunc);
        void waitHotKey();                                    // 监听快捷键（阻塞）

        // 单个按键监听
        void listenOneKeyEvent(bool real_time);
        void listenOneKeyEvent();
        long getOneKeyCode();                                  // 获取键盘状态码

        // 清理快捷键
        static void clearHotKey();
        static void uninstallHotKeyEvent();

        // 清理单个按键监听
        static void exitOneKeyEvent();
        static void uninstallOneKeyEvent();

        // 程序退出时释放键盘资源（未松开的按键、驱动级按键、钩子等）
        static void cleanup();
    };

    /*************** 组合键模板实现 ***************/
    // 按下快捷键 //
    template<typename... Args>
    void KeyBoardEvent::pressHotKey(const std::string& first, Args... rest)
    {
        keyDown(first);       // 按下当前键
        pressHotKey(rest...); // 递归处理剩余键
        keyUp(first);         // 递归返回后释放当前键（逆序释放）
    }

    // 驱动级操作 //
    template<typename... Args>
    void KeyBoardEvent::pressHotKeyDriving(const std::string& first, Args... rest)
    {
        keyDown(first, true);        // 驱动级按下当前键
        pressHotKeyDriving(rest...); // 驱动级递归处理剩余键
        keyUp(first, true);          // 驱动级递归返回后释放当前键（逆序释放）
    }

} // namespace wtl
