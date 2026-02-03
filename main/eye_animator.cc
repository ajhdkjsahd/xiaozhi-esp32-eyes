#include "eye_animator.h"
#include <esp_log.h>
#include <cstdlib> // for rand()
#include <ctime>

// 日志标签，用于 ESP_LOG 输出
#define TAG "ANIM"

// === 原有的全幅眨眼序列 (用于精神状态) ===
// 包含三组不同的眨眼序列，用于在精神状态下随机选择
static const std::vector<std::vector<int>> ACTIVE_BLINK_SEQS = {
    {6, 7, 8, 9, 10, 11},      // 第一组眨眼序列
    {20, 21, 22, 23, 24, 25},  // 第二组眨眼序列
    {31, 32, 33, 34, 35, 36}   // 第三组眨眼序列
};

// === 新增：慵懒眨眼序列 (仅在空闲时使用) ===
// 逻辑：半睁(45) -> 微闭(44) -> 闭(42) -> 全闭(43) -> 闭(42) -> 微闭(44) -> 半睁(45)
// 形成一个完整的闭合循环，呈现慵懒的眨眼效果
static const std::vector<int> LAZY_BLINK_SEQ = {45, 44, 42, 43, 42, 44, 45};

// === 慵懒微动池 (平时停留的帧) ===
// 空闲状态下眼睛保持的微动帧，主要为半睁和微闭状态
static const std::vector<int> LAZY_IDLE_FRAMES = {44, 45}; 

// === 精神微动池 (保持不变，剔除掉 42-45) ===
// 精神状态下眼睛保持的微动帧，包含多种不同的眼神姿态
static const std::vector<int> ACTIVE_IDLE_FRAMES = {
    0, 1, 2, 3, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 
    26, 27, 28, 29, 30, 37, 38, 39, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59
};

/**
 * @brief 生成指定范围内的随机整数
 * 
 * @param min 最小值（包含）
 * @param max 最大值（包含）
 * @return int 生成的随机整数
 */
int RandomInt(int min, int max) {
    return min + (rand() % (max - min + 1));
}

/**
 * @brief 启动眼睛动画任务
 * 
 * 初始化随机种子并创建 FreeRTOS 任务，开始执行眼睛动画逻辑
 */
void EyeAnimator::Start() {
    // 使用当前系统滴答数作为随机种子，确保每次运行时随机序列不同
    srand(xTaskGetTickCount()); 
    
    // 创建动画任务
    // 参数：任务函数、任务名、任务栈大小、任务参数、任务优先级、任务句柄
    xTaskCreate(AnimationTask, "EyeAnim", 4096, this, 5, &task_handle_);
}

/**
 * @brief 设置眼睛当前状态
 * 
 * @param state 要设置的眼睛状态
 * 
 * 处理状态变化，当从 OPEN 状态切换到 LISTENING 状态时，触发唤醒动画
 */
void EyeAnimator::SetState(EyeState state) {
    // 检测状态跳变：从非聆听状态 -> 变成聆听状态
    if (current_state_ == EyeState::OPEN && state == EyeState::LISTENING) {
        // 设置唤醒动画标志，让主循环在下一次迭代中处理
        play_wake_up_anim_ = true; 
    }
    
    // 更新当前状态
    current_state_ = state;
}

/**
 * @brief 强制控制眼睛闭合
 * 
 * @param force true: 强制闭合眼睛，false: 解除强制闭合
 */
void EyeAnimator::SetForceClose(bool force) {
    force_closed_ = force;
}

/**
 * @brief 播放单个帧图像
 * 
 * @param image_id 要显示的图像ID
 * 
 * 根据图像ID计算对应的Flash地址，然后发送指令给屏幕显示图像
 */
void EyeAnimator::PlayFrame(int image_id) {
    // === 地址映射逻辑 ===
    // 基地址：2212352 (0x21C000) - Flash中存储图像数据的起始地址
    // 偏移量：80000 (0x13880) - 每个图像占用的字节数
    uint32_t address = 2212352 + (image_id * 80000);

    // 拼装FSIMG指令：FSIMG(地址, X, Y, 宽度, 高度, 透明度)
    char buf[64];
    snprintf(buf, sizeof(buf), "FSIMG(%lu,20,20,200,200,0);\r\n", address);
    
    // 发送指令给屏幕
    ScreenDriver::GetInstance().SendCommand(buf);
}

// 修改函数签名，加个参数，或者在内部判断 current_state_
/**
 * @brief 播放一次眨眼动画
 * 
 * 根据当前眼睛状态选择不同的眨眼序列：
 * - OPEN 状态：播放慵懒眨眼序列，速度较慢
 * - 其他状态：从精神眨眼序列中随机选择一组，速度较快
 */
void EyeAnimator::PlayBlink() {
    if (current_state_ == EyeState::OPEN) {
        // 播放慵懒眨眼序列
        for (int img_id : LAZY_BLINK_SEQ) {
            PlayFrame(img_id);
            vTaskDelay(pdMS_TO_TICKS(200)); // 200ms 一帧，速度较慢，呈现慵懒效果
        }
    } else {
        // 播放精神眨眼序列
        // 随机选择一组眨眼序列
        int seq_idx = RandomInt(0, ACTIVE_BLINK_SEQS.size() - 1);
        const auto& seq = ACTIVE_BLINK_SEQS[seq_idx];
        
        for (int img_id : seq) {
            PlayFrame(img_id);
            vTaskDelay(pdMS_TO_TICKS(35)); // 35ms 一帧，速度较快，呈现精神状态
        }
    }
}

// === 核心动画逻辑任务 ===
/**
 * @brief 眼睛动画任务函数
 * 
 * @param arg 任务参数，指向 EyeAnimator 实例
 * 
 * 主要动画循环，处理以下逻辑：
 * 1. 强制闭眼处理
 * 2. 唤醒动画处理
 * 3. 定时眨眼
 * 4. 根据当前状态的微动效果
 */
void EyeAnimator::AnimationTask(void* arg) {
    // 将任务参数转换为 EyeAnimator 实例指针
    EyeAnimator* self = (EyeAnimator*)arg;
    
    // 记录上次眨眼的时间（使用系统滴答数）
    TickType_t last_blink_time = xTaskGetTickCount();
    
    // 下次眨眼的间隔，初始化为 3 秒
    int next_blink_interval = 3000; 

    // 主循环，持续执行动画逻辑
    while (true) {
        // === 1. 处理强制闭眼逻辑 ===
        if (self->force_closed_ || self->current_state_ == EyeState::CLOSE) {
            // 播放闭眼帧（ID 8，需根据实际图像序列确认）
            self->PlayFrame(8); 
            
            // 每500ms检查一次是否解除了强制
            vTaskDelay(pdMS_TO_TICKS(500)); 
            
            // 跳过本次循环的后续逻辑，不执行眨眼和微动
            continue; 
        }

        // === 2. 处理唤醒动画 (快速眨眼两下) ===
        if (self->play_wake_up_anim_) {
            // 清除唤醒动画标志
            self->play_wake_up_anim_ = false; 
            
            // 快速眨眼两次
            self->PlayBlink(); 
            vTaskDelay(pdMS_TO_TICKS(100)); // 两次眨眼之间的间隔
            self->PlayBlink();
            
            // 播放完后，短暂等待再继续正常逻辑
            vTaskDelay(pdMS_TO_TICKS(200));
        }

        // === 3. 检查是否该眨眼了 ===
        TickType_t now = xTaskGetTickCount();
        if (pdTICKS_TO_MS(now - last_blink_time) > next_blink_interval) {
            // 播放一次眨眼动画
            self->PlayBlink(); 
            
            // 更新上次眨眼时间
            last_blink_time = now;
            
            // 根据当前状态设置下次眨眼间隔
            if (self->current_state_ == EyeState::OPEN) {
                // 慵懒模式下，眨眼间隔更长且更随机 (3-8秒)
                next_blink_interval = RandomInt(3000, 8000); 
            } else {
                // 精神状态下，眨眼间隔相对较短 (2-6秒)
                next_blink_interval = RandomInt(2000, 6000);
            }
        }

        // === 4. 非眨眼期间的微动效果 ===
        // 根据当前状态选择微动帧池
        const std::vector<int>* current_pool;
        if (self->current_state_ == EyeState::OPEN) {
            current_pool = &LAZY_IDLE_FRAMES; // 慵懒状态下的微动帧
        } else {
            current_pool = &ACTIVE_IDLE_FRAMES; // 精神状态下的微动帧
        }

        // 从当前帧池中随机选择一帧
        int current_img_idx = (*current_pool)[RandomInt(0, current_pool->size() - 1)];
        
        // 播放选中的微动帧
        self->PlayFrame(current_img_idx);

        // 根据当前状态设置微动间隔
        int wait_time = 0;

        switch (self->current_state_) {
            case EyeState::LISTENING:
                // 聆听状态：眼神专注，变动极慢 (800-1500ms)
                wait_time = RandomInt(800, 1500);
                break;

            case EyeState::SPEAKING:
                // 说话状态：眼神活跃，变动较快 (150-400ms)
                wait_time = RandomInt(150, 400);
                break;

            case EyeState::THINKING:
                // 思考状态：极快跳动，模拟眼球快速转动检索信息 (50-150ms)
                wait_time = RandomInt(50, 150);
                break;

            case EyeState::OPEN:
                // 空闲状态：半睁眼，呼吸极慢，显得很放松 (2000-4000ms)
                wait_time = RandomInt(2000, 4000); 
                break;

            default:
                // 默认状态：悠闲的节奏 (500-1200ms)
                wait_time = RandomInt(500, 1200);
                break;
        }

        // 等待指定时间，然后进入下一次循环
        vTaskDelay(pdMS_TO_TICKS(wait_time));
    }
}