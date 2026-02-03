#pragma once
#include <stdint.h>
#include <driver/gpio.h>
#include <driver/uart.h>
#include <string>    // 用于字符串处理
#include "utf8togb2312.h" // UTF-8 转 GB2312 编码转换

// ================= 配置区域 =================
// 屏幕串口配置参数，方便统一管理和修改
#define TEST_UART_PORT_NUM      (UART_NUM_1)    ///< 串口端口号
#define TEST_UART_TXD           (GPIO_NUM_9)    ///< 串口发送引脚
#define TEST_UART_RXD           (GPIO_NUM_10)   ///< 串口接收引脚
#define TEST_UART_BAUD_RATE     (115200)        ///< 串口波特率
#define BUF_SIZE                (1024)          ///< 串口缓冲区大小

/**
 * @brief 眼睛状态枚举
 * 
 * 定义眼睛的不同状态，用于控制眼睛动画效果
 */
enum class EyeState {
    OPEN,       ///< 睁开（默认/空闲状态）
    CLOSE,      ///< 闭上（MCP 控制或休眠状态）
    LISTENING,  ///< 聆听中（专注倾听的动画效果）
    THINKING,   ///< 思考中（思考时的动画效果）
    SPEAKING    ///< 说话中（说话时的动画效果）
};

/**
 * @brief 屏幕驱动类
 * 
 * 负责屏幕的初始化、串口通信、眼睛状态控制和字幕显示等功能
 * 使用单例模式实现，确保全局只有一个屏幕驱动实例
 */
class ScreenDriver {
public:
    /**
     * @brief 获取单例实例
     * 
     * @return ScreenDriver& 屏幕驱动的唯一实例
     */
    static ScreenDriver& GetInstance() {
        static ScreenDriver instance;
        return instance;
    }

    /**
     * @brief 初始化屏幕驱动
     * 
     * 配置并初始化串口，启动眼睛动画引擎
     */
    void Init();

    /**
     * @brief 设置眼睛状态
     * 
     * @param state 要设置的眼睛状态
     * 
     * 控制眼睛的动画效果，根据不同状态显示不同的动画
     */
    void SetEyeState(EyeState state);

    /**
     * @brief 强制睁开眼睛
     * 
     * 解除强制闭眼状态，恢复正常的眼睛动画
     */
    void ForceOpenEye();

    /**
     * @brief 强制闭上眼睛
     * 
     * 强制眼睛保持闭合状态，忽略其他状态请求
     */
    void ForceCloseEye();

    /**
     * @brief 发送文本字幕到屏幕
     * 
     * @param text 要显示的文本内容（UTF-8 编码）
     * 
     * 将 UTF-8 编码的文本转换为 GB2312 编码并发送到屏幕显示
     */
    void SendSubtitle(const std::string& text);

    /**
     * @brief 发送原始指令到屏幕
     * 
     * @param cmd 要发送的指令字符串
     * 
     * 直接发送原始指令到屏幕，用于控制屏幕的各种功能
     */
    void SendCommand(const char* cmd); 

private:
    /**
     * @brief 私有构造函数（单例模式）
     */
    ScreenDriver() = default;

    bool force_closed_ = false; ///< 强制闭眼标志，true表示当前处于强制闭眼状态
};