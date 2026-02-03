#pragma once
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "screen_driver.h" // 引用屏幕驱动

/**
 * @brief 眼睛动画控制器类
 * 
 * 该类负责控制眼睛的各种动画效果，包括不同状态下的眼神、眨眼、微动等
 * 使用单例模式实现，确保全局只有一个动画控制器实例
 */
class EyeAnimator {
public:
    /**
     * @brief 获取单例实例
     * 
     * @return EyeAnimator& 动画控制器的唯一实例
     */
    static EyeAnimator& GetInstance() {
        static EyeAnimator instance;
        return instance;
    }

    /**
     * @brief 启动动画任务
     * 
     * 创建并启动眼睛动画的 FreeRTOS 任务，开始执行动画逻辑
     */
    void Start(); 

    /**
     * @brief 设置当前眼睛状态
     * 
     * @param state 要设置的眼睛状态（如 OPEN, LISTENING, SPEAKING, THINKING 等）
     */
    void SetState(EyeState state); 

    /**
     * @brief 强制控制眼睛闭合
     * 
     * @param force true: 强制闭合眼睛，false: 解除强制闭合
     */
    void SetForceClose(bool force);

private:
    /**
     * @brief 私有构造函数（单例模式）
     */
    EyeAnimator() = default;

    /**
     * @brief 动画任务函数
     * 
     * @param arg 任务参数，指向 EyeAnimator 实例
     */
    static void AnimationTask(void* arg); 

    /**
     * @brief 播放单个帧图像
     * 
     * @param image_id 要显示的图像ID，对应于Flash中的图像地址
     */
    void PlayFrame(int image_id);     

    /**
     * @brief 播放一次眨眼动画
     * 
     * 根据当前状态选择不同的眨眼序列（正常眨眼或慵懒眨眼）
     */
    void PlayBlink();                 
    
    // 内部变量
    EyeState current_state_ = EyeState::OPEN;     ///< 当前眼睛状态
    TaskHandle_t task_handle_ = nullptr;          ///< 动画任务句柄
    bool force_closed_ = false;                   ///< 强制闭合标志
    bool play_wake_up_anim_ = false;              ///< 唤醒动画标志
};