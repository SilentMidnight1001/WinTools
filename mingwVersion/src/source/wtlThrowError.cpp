#include "wtlThrowError.hpp"

// ========== 模拟第三方库 ========== //
namespace third_party_lib {

    int divide(int a, int b) {
        if (b == 0) {
            wtl::ThrowError::showError("除数不能为零");   // 直接调用，无需宏
        }
        return a / b;
    }

    void readConfig(const std::string& path) {
        wtl::ThrowError::showError("配置文件不存在: " + path);
    }

}  // namespace third_party_lib

// ========== 调用方（你的项目代码）==========

void myBusinessLogic() {
    third_party_lib::divide(10, 0);
}