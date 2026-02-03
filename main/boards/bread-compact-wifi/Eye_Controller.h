/*
* 文件名：Eye_Control.h
* 功能：定义眼睛控制类
* 参考代码架构来源：【小智官方开源文档】https://kcn80f4hacgs.feishu.cn/docx/J2MrdqW27oybcCxu7Sfc4gBcn4g
*/

#include "mcp_server.h"
#include "screen_driver.h" // 引入我们刚才写的驱动
#include <esp_log.h>

#define TAG_EYE "眼睛控制："

class Eye_Controller  { 
private:
    bool is_open_ = true; // 记录当前逻辑状态

public:
    Eye_Controller() {
        auto& server = McpServer::GetInstance();
        
        /* 真相是： 大模型听懂了你的话，但它以为你是在和它玩角色扮演 (Roleplay)。它觉得回复一句“眼睛闭上啦~”就完成了任务，它觉得没必要去调用硬件工具。 */

        // 1. 修改睁眼描述
        // 原来："把眼睛睁开/显示睁眼表情" (太软了)
        // 改为："硬件控制指令：切换屏幕显示为睁眼动画。必须调用此工具才能改变眼睛状态。"
        server.AddTool("眼睛.睁开", "硬件控制指令：强制屏幕显示睁眼动画。用户要求睁眼时必须调用。", 
                       PropertyList(), [this](const PropertyList&) {
                           is_open_ = true;
                           ScreenDriver::GetInstance().ForceOpenEye();
                           ESP_LOGW(TAG_EYE, "已睁开眼睛");
                           return "success";
                       });

        // 2. 修改闭眼描述
        // 原来："把眼睛闭上/显示闭眼表情"
        // 改为："硬件控制指令：切换屏幕显示为闭眼动画。用户要求闭眼时必须调用。"
        server.AddTool("眼睛.闭上", "硬件控制指令：强制屏幕显示闭眼动画。用户要求闭眼时必须调用。", 
                       PropertyList(), [this](const PropertyList&) {
                           is_open_ = false;
                           ScreenDriver::GetInstance().ForceCloseEye();
                           ESP_LOGW(TAG_EYE, "已闭上眼睛");
                           return "success";
                       });
    }

};