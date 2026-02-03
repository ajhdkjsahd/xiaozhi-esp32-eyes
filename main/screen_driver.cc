#include "screen_driver.h"
#include "esp_log.h"
#include <cstring>
#include <vector> // 引入 vector 以防栈溢出
#include "eye_animator.h"

#define TAG "SCREEN"

#define TEST_TASK_STACK_SIZE    (4096)


void ScreenDriver::Init() {

    // 这里填入 UART 初始化代码

    // 1. 配置参数
    const uart_config_t uart_config = {
        .baud_rate = TEST_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // 2. 安装驱动 (注意设置 TX Buffer)
    ESP_ERROR_CHECK(uart_driver_install(TEST_UART_PORT_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 0, NULL, 0));
    
    // 3. 应用配置
    ESP_ERROR_CHECK(uart_param_config(TEST_UART_PORT_NUM, &uart_config));

    // 4. 设置引脚
    ESP_ERROR_CHECK(uart_set_pin(TEST_UART_PORT_NUM, TEST_UART_TXD, TEST_UART_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    ESP_LOGI(TAG, "UART1 Initialized on TX:%d RX:%d", TEST_UART_TXD, TEST_UART_RXD);

    EyeAnimator::GetInstance().Start(); // <--- 启动动画引擎

    ESP_LOGI(TAG, "屏幕串口初始化完成");
}


// 修改 SetEyeState，只负责转发状态
void ScreenDriver::SetEyeState(EyeState state) {
    // 如果处于强制闭眼状态，忽略所有状态切换请求（除了 CLOSE）
    if (force_closed_ && state != EyeState::CLOSE) {
        return; 
    }
    // 转发给动画引擎
    EyeAnimator::GetInstance().SetState(state);
}

// 给 MCP 调用的专门接口
void ScreenDriver::ForceCloseEye() {
    force_closed_ = true;
    // 告诉动画引擎：别动了，闭眼！
    EyeAnimator::GetInstance().SetForceClose(true);
}

void ScreenDriver::ForceOpenEye() {
    force_closed_ = false;
    // 告诉动画引擎：解除封印，开始表演！
    EyeAnimator::GetInstance().SetForceClose(false);
    // 顺便重置状态为 OPEN
    EyeAnimator::GetInstance().SetState(EyeState::OPEN);
}

void ScreenDriver::SendCommand(const char* cmd) {
    if (cmd == nullptr) return;
    uart_write_bytes(TEST_UART_PORT_NUM, cmd, strlen(cmd));
    // 很多串口屏需要结束符，比如 0xFF 0xFF 0xFF，别忘了加
    // const char end_bytes[] = "\r\n"; 
    // uart_write_bytes(TEST_UART_PORT_NUM, end_bytes, 2);
}



/**
 * @brief 发送文本字幕到屏幕
 * 
 * @param text 要显示的文本内容（UTF-8 编码）
 * 
 * 将 UTF-8 编码的文本转换为 GB2312 编码，处理特殊字符，然后发送到屏幕显示
 */
void ScreenDriver::SendSubtitle(const std::string& text) {
    // 1. 定义缓冲区
    // 使用 static 防止频繁申请内存，提高效率
    static char gbk_buf[1024];      // GBK 编码缓冲区
    static char final_cmd[1200];    // 最终指令缓冲区（比内容缓冲区大，留出指令头空间）

    // 2. 长度保护：防止源文本过长导致内存溢出
    // 截断源文本，留一点余量给转码膨胀，并添加省略号
    std::string safe_text = text;
    if (safe_text.length() > 800) {
        safe_text = safe_text.substr(0, 800) + "...";
    }

    // 3. 转码：UTF-8 内容 -> GB2312 内容
    // 注意：只转码文本内容，不转码指令头
    memset(gbk_buf, 0, sizeof(gbk_buf)); // 清空缓冲区
    UTF_8ToGB2312(gbk_buf, (char*)safe_text.c_str(), safe_text.length());

    // 4. 清洗特殊字符 (防止指令格式被破坏)
    // 由于指令格式为 SET_TXT(1, '内容'); 使用单引号包裹内容
    // 因此需要处理内容中的特殊字符
    for (int i = 0; gbk_buf[i] != '\0'; i++) {
        if (gbk_buf[i] == '\'') {
            gbk_buf[i] = '"'; // 将单引号替换为双引号，避免截断指令
        }
        // 处理换行符，防止屏幕无法识别
        if (gbk_buf[i] == '\n' || gbk_buf[i] == '\r') {
            gbk_buf[i] = ' ';  // 将换行符替换为空格
        }
    }

    // 5. 拼装最终指令
    // 格式：SET_TXT(1,'转换并清洗后的GBK内容');\r\n
    snprintf(final_cmd, sizeof(final_cmd), "SET_TXT(1,'%s');\r\n", gbk_buf);

    // 6. 发送指令到屏幕
    SendCommand(final_cmd);
}