#pragma once

#include "../libs/lvgl/lvgl.h"
#include "../utils/lv_ext/lv_obj_ext_func.h"
#include "../utils/lv_ext/lv_anim_timeline_wrapper.h"
#include "../utils/Animations/Animations.h"
#include "ResourcePool.h"
#include <functional>

namespace Page
{
    using ExitCb = std::function<void(void)>;
    using RefreshCb = std::function<void(void)>;   // 手动刷新余额回调

    struct Operations
    {
        ExitCb exitCb;
        RefreshCb refreshCb;
    };

    class View
    {
    private:
        Operations _opts;      // View 回调函数集
        bool _useCjk = false;  // 是否成功加载中文字体

    public:
        struct
        {
            lv_obj_t *cont; // 总画布

            struct
            {
                lv_ft_info_t font16; // 自定义字体
                lv_ft_info_t font20;
                lv_ft_info_t font24;
                lv_ft_info_t font32;
                lv_ft_info_t font48;
            } fontCont;

            struct
            {
                lv_obj_t *cont;
                lv_obj_t *exitBtn;   // 退出按钮
                lv_obj_t *shotBtn;   // 截图按钮
                lv_obj_t *titleLabel;
            } topCont;

            struct
            {
                lv_obj_t *cont;
                lv_obj_t *dot;       // 网络状态点
                lv_obj_t *valueLabel;
            } netCont;

            struct
            {
                lv_obj_t *cont;
                lv_obj_t *refreshBtn;  // 刷新按钮
                lv_obj_t *refreshIcon; // 按钮内的刷新图标（单独旋转）
                lv_obj_t *titleLabel;
                lv_obj_t *valueLabel;   // 大号余额
                lv_obj_t *currencyLabel;
                lv_obj_t *statusLabel;  // 可用 / 欠费
            } balanceCont;

            struct
            {
                lv_obj_t *cont;
                lv_obj_t *grantedTitleLabel;
                lv_obj_t *grantedValueLabel;
                lv_obj_t *toppedTitleLabel;
                lv_obj_t *toppedValueLabel;
            } detailCont;

            struct
            {
                lv_obj_t *cont;
                lv_obj_t *updateLabel; // 最后更新时间
                lv_obj_t *msgLabel;    // 状态消息
            } footerCont;

            lv_anim_timeline_t *anim_timeline;
        } ui;

        void create(Operations &opts);
        void release(void);
        void setOperations(Operations &opts);
        void appearAnimStart(bool reverse = false);

        /* UI 更新接口（供 Model 调用） */
        void setNetwork(bool connected);
        void setBalanceText(const char *text);
        void setCurrencyText(const char *text);
        void setGrantedText(const char *text);
        void setToppedText(const char *text);
        void setAvailable(bool available);
        void setStatusMessage(const char *msg);
        void setLastUpdate(const char *text);
        void setRefreshBusy(bool busy); // 刷新按钮转圈动画
        void screenshot(void);          // 截屏保存为 PNG

    private:
        void fontCreate(void);
        void contCreate(lv_obj_t *obj);
        void topContCreate(lv_obj_t *obj);
        void netContCreate(lv_obj_t *obj);
        void balanceContCreate(lv_obj_t *obj);
        void detailContCreate(lv_obj_t *obj);
        void footerContCreate(lv_obj_t *obj);

        lv_obj_t *btnCreate(lv_obj_t *par, const void *img_src, lv_coord_t x_ofs, lv_coord_t y_ofs, lv_coord_t w = 50, lv_coord_t h = 50);

        // 根据是否加载中文字体，返回中文或英文文案
        const char *T(const char *cn, const char *en) { return _useCjk ? cn : en; }
        const lv_font_t *fontSmall();  // 16
        const lv_font_t *fontMid();    // 20
        const lv_font_t *fontBig();    // 24
        const lv_font_t *fontLarge();  // 32
        const lv_font_t *fontHuge();   // 48

        void sideTipsPopupCreate(lv_obj_t *obj, const char *tips);
        static void onEvent(lv_event_t *event);
        static void topContEventHandler(lv_event_t *event);
        static void balanceContEventHandler(lv_event_t *event);

        static void convertRGB2BGR(lv_img_dsc_t *snapshot);
    };

}
