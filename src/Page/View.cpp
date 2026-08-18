#include "View.h"
#include "Model.h"
#include "../utils/log/log.h"

#include <unistd.h>
#include <time.h>
#include <stdio.h>

extern "C"
{
#include "../../libs/lvgl/src/extra/libs/png/lodepng.h"
}

using namespace Page;

void View::create(Operations &opts)
{
    // 获取 View 回调函数集
    _opts = opts;

    // 初始化字体
    fontCreate();

    // 总画布的创建
    contCreate(lv_scr_act());
    // 顶部栏
    topContCreate(ui.cont);
    // 网络状态卡片
    netContCreate(ui.cont);
    // 余额主卡片
    balanceContCreate(ui.cont);
    // 明细卡片（赠金 / 充值）
    detailContCreate(ui.cont);
    // 底部信息栏
    footerContCreate(ui.cont);

    // 开场动画
    ui.anim_timeline = lv_anim_timeline_create();

#define ANIM_DEF(start_time, obj, attr, start, end) \
    {start_time, obj, LV_ANIM_EXEC(attr), start, end, 500, lv_anim_path_ease_out, true}

#define ANIM_OPA_DEF(start_time, obj) \
    ANIM_DEF(start_time, obj, opa_scale, LV_OPA_TRANSP, LV_OPA_COVER)

    lv_anim_timeline_wrapper_t wrapper[] =
        {
            ANIM_OPA_DEF(0, ui.balanceCont.cont),
            ANIM_OPA_DEF(80, ui.detailCont.cont),
            ANIM_OPA_DEF(160, ui.netCont.cont),
            LV_ANIM_TIMELINE_WRAPPER_END
        };
    lv_anim_timeline_add_wrapper(ui.anim_timeline, wrapper);

    appearAnimStart();
}

void View::release()
{
    if (ui.anim_timeline)
    {
        lv_anim_timeline_del(ui.anim_timeline);
        ui.anim_timeline = nullptr;
    }
}

void View::setOperations(Operations &opts)
{
    _opts = opts;
}

void View::appearAnimStart(bool reverse) // 开始开场动画
{
    if (ui.anim_timeline)
    {
        lv_anim_timeline_set_reverse(ui.anim_timeline, reverse);
        lv_anim_timeline_start(ui.anim_timeline);
    }
}

/* ============================= 字体 ============================= */

void View::fontCreate(void)
{
    const char *fontPath = "/mnt/UDISK/font/SmileySans.ttf";

    // 检查中文字体是否存在
    _useCjk = (access(fontPath, R_OK) == 0);
    if (!_useCjk)
    {
        log_warn("[View] font %s not found, fallback to montserrat (English UI)", fontPath);
        return;
    }

    struct FontEntry
    {
        lv_ft_info_t *info;
        uint16_t weight;
    } fonts[] = {
        {&ui.fontCont.font16, 16},
        {&ui.fontCont.font20, 20},
        {&ui.fontCont.font24, 24},
        {&ui.fontCont.font32, 32},
        {&ui.fontCont.font48, 48},
    };

    for (auto &f : fonts)
    {
        f.info->name = fontPath;
        f.info->weight = f.weight;
        f.info->style = FT_FONT_STYLE_NORMAL;
        f.info->mem = nullptr;
        if (!lv_ft_font_init(f.info))
        {
            _useCjk = false;
            log_warn("[View] lv_ft_font_init failed (weight=%d), fallback to montserrat", f.weight);
            return;
        }
    }
}

const lv_font_t *View::fontSmall() { return _useCjk ? ui.fontCont.font16.font : &lv_font_montserrat_14; }
const lv_font_t *View::fontMid() { return _useCjk ? ui.fontCont.font20.font : &lv_font_montserrat_16; }
const lv_font_t *View::fontBig() { return _useCjk ? ui.fontCont.font24.font : &lv_font_montserrat_22; }
const lv_font_t *View::fontLarge() { return _useCjk ? ui.fontCont.font32.font : &lv_font_montserrat_22; }
const lv_font_t *View::fontHuge() { return _useCjk ? ui.fontCont.font48.font : &lv_font_montserrat_48; }

/* ============================= 画布 ============================= */

void View::contCreate(lv_obj_t *obj)
{
    lv_obj_t *cont = lv_obj_create(obj);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, LV_HOR_RES, LV_VER_RES);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0xa18cd1), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(cont, lv_color_hex(0xfbc2eb), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(cont, LV_GRAD_DIR_VER, LV_PART_MAIN); // 垂直渐变
    lv_obj_set_style_bg_main_stop(cont, 0, LV_PART_MAIN);              // 渐变起点
    lv_obj_set_style_bg_grad_stop(cont, 255, LV_PART_MAIN);            // 渐变终点
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, 0);
    ui.cont = cont;
}

void View::topContCreate(lv_obj_t *obj)
{
    lv_obj_t *cont = lv_obj_create(obj);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, lv_pct(90), 44);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(cont, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0xeeeeee), LV_PART_MAIN);
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_radius(cont, 12, LV_PART_MAIN);
    ui.topCont.cont = cont;

    // 退出按钮（左）
    lv_obj_t *exitBtn = btnCreate(cont, nullptr, 0, 0, 34, 34);
    lv_obj_align(exitBtn, LV_ALIGN_LEFT_MID, 8, 0);
    lv_obj_set_style_bg_color(exitBtn, lv_color_hex(0xff6056), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(exitBtn, lv_color_hex(0xe44543), LV_STATE_PRESSED);
    lv_obj_set_ext_click_area(exitBtn, 15);
    ui.topCont.exitBtn = exitBtn;
    lv_obj_add_event_cb(exitBtn, topContEventHandler, LV_EVENT_SHORT_CLICKED, this);

    lv_obj_t *exitLabel = lv_label_create(exitBtn);
    lv_obj_remove_style_all(exitLabel);
    lv_obj_set_style_text_font(exitLabel, fontBig(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(exitLabel, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_center(exitLabel);
    lv_label_set_text(exitLabel, "x");

    // 标题（中）
    lv_obj_t *titleLabel = lv_label_create(cont);
    lv_obj_remove_style_all(titleLabel);
    lv_obj_set_style_text_font(titleLabel, fontBig(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(titleLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(titleLabel, lv_color_hex(0x333333), LV_STATE_DEFAULT);
    lv_obj_align(titleLabel, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(titleLabel, T("Token 余额监视器", "Token Monitor"));
    ui.topCont.titleLabel = titleLabel;

    // 截图按钮（右）
    lv_obj_t *shotBtn = btnCreate(cont, nullptr, 0, 0, 46, 34);
    lv_obj_align(shotBtn, LV_ALIGN_RIGHT_MID, -8, 0);
    lv_obj_set_style_bg_color(shotBtn, lv_color_hex(0xffd76d), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(shotBtn, lv_color_hex(0xdc9c00), LV_STATE_PRESSED);
    lv_obj_set_ext_click_area(shotBtn, 10);
    ui.topCont.shotBtn = shotBtn;
    lv_obj_add_event_cb(shotBtn, topContEventHandler, LV_EVENT_SHORT_CLICKED, this);

    lv_obj_t *shotLabel = lv_label_create(shotBtn);
    lv_obj_remove_style_all(shotLabel);
    lv_obj_set_style_text_font(shotLabel, fontSmall(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(shotLabel, lv_color_hex(0x222222), LV_STATE_DEFAULT);
    lv_obj_center(shotLabel);
    lv_label_set_text(shotLabel, "Shot");
}

void View::netContCreate(lv_obj_t *obj)
{
    lv_obj_t *cont = lv_obj_create(obj);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, lv_pct(90), 40);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(cont, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x2b2b3a), LV_PART_MAIN);
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 64);
    lv_obj_set_style_radius(cont, 12, LV_PART_MAIN);
    ui.netCont.cont = cont;

    // 状态点
    lv_obj_t *dot = lv_obj_create(cont);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, 14, 14);
    lv_obj_align(dot, LV_ALIGN_LEFT_MID, 14, 0);
    lv_obj_set_style_radius(dot, 255, LV_PART_MAIN);
    lv_obj_set_style_bg_color(dot, lv_color_hex(0x888888), LV_PART_MAIN);
    ui.netCont.dot = dot;

    // 状态文字
    lv_obj_t *valueLabel = lv_label_create(cont);
    lv_obj_remove_style_all(valueLabel);
    lv_obj_set_style_text_font(valueLabel, fontMid(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(valueLabel, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_align(valueLabel, LV_ALIGN_LEFT_MID, 38, 0);
    lv_label_set_text(valueLabel, T("网络：检测中...", "Network: checking..."));
    ui.netCont.valueLabel = valueLabel;
}

void View::balanceContCreate(lv_obj_t *obj)
{
    lv_obj_t *cont = lv_obj_create(obj);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, lv_pct(90), lv_pct(36));
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 116);
    lv_obj_set_style_radius(cont, 16, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(cont, 20, LV_PART_MAIN);
    lv_obj_set_style_shadow_ofs_y(cont, 6, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(cont, LV_OPA_20, LV_PART_MAIN);
    ui.balanceCont.cont = cont;

    // 标题
    lv_obj_t *titleLabel = lv_label_create(cont);
    lv_obj_remove_style_all(titleLabel);
    lv_obj_set_style_text_font(titleLabel, fontBig(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(titleLabel, lv_color_hex(0x555555), LV_STATE_DEFAULT);
    lv_obj_align(titleLabel, LV_ALIGN_TOP_LEFT, 16, 12);
    lv_label_set_text(titleLabel, T("DeepSeek 余额", "DeepSeek Balance"));
    ui.balanceCont.titleLabel = titleLabel;

    // 刷新按钮（右上，圆形）
    lv_obj_t *refreshBtn = btnCreate(cont, nullptr, 0, 0, 40, 40);
    lv_obj_align(refreshBtn, LV_ALIGN_TOP_RIGHT, -12, 8);
    lv_obj_set_style_radius(refreshBtn, 255, LV_PART_MAIN);
    lv_obj_set_style_bg_color(refreshBtn, lv_color_hex(0x7d45ed), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(refreshBtn, lv_color_hex(0x5c2fb8), LV_STATE_PRESSED);
    lv_obj_set_style_bg_img_src(refreshBtn, LV_SYMBOL_REFRESH, LV_PART_MAIN);
    lv_obj_set_style_text_font(refreshBtn, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(refreshBtn, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_ext_click_area(refreshBtn, 10);
    ui.balanceCont.refreshBtn = refreshBtn;
    lv_obj_add_event_cb(refreshBtn, balanceContEventHandler, LV_EVENT_SHORT_CLICKED, this);

    // 大号余额
    lv_obj_t *valueLabel = lv_label_create(cont);
    lv_obj_remove_style_all(valueLabel);
    lv_obj_set_style_text_font(valueLabel, fontHuge(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(valueLabel, lv_color_hex(0x333333), LV_STATE_DEFAULT);
    lv_obj_align(valueLabel, LV_ALIGN_CENTER, 0, -6);
    lv_label_set_text(valueLabel, "--");
    ui.balanceCont.valueLabel = valueLabel;

    // 币种
    lv_obj_t *currencyLabel = lv_label_create(cont);
    lv_obj_remove_style_all(currencyLabel);
    lv_obj_set_style_text_font(currencyLabel, fontMid(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(currencyLabel, lv_color_hex(0x999999), LV_STATE_DEFAULT);
    lv_obj_align(currencyLabel, LV_ALIGN_CENTER, 0, 36);
    lv_label_set_text(currencyLabel, "CNY");
    ui.balanceCont.currencyLabel = currencyLabel;

    // 可用状态
    lv_obj_t *statusLabel = lv_label_create(cont);
    lv_obj_remove_style_all(statusLabel);
    lv_obj_set_style_text_font(statusLabel, fontMid(), LV_STATE_DEFAULT);
    lv_obj_align(statusLabel, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_label_set_text(statusLabel, "--");
    ui.balanceCont.statusLabel = statusLabel;
}

void View::detailContCreate(lv_obj_t *obj)
{
    lv_obj_t *cont = lv_obj_create(obj);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, lv_pct(90), 74);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 306);
    ui.detailCont.cont = cont;

    // 赠金卡片（左）
    lv_obj_t *grantedCard = lv_obj_create(cont);
    lv_obj_remove_style_all(grantedCard);
    lv_obj_set_size(grantedCard, lv_pct(48), lv_pct(100));
    lv_obj_clear_flag(grantedCard, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(grantedCard, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(grantedCard, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_align(grantedCard, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_radius(grantedCard, 12, LV_PART_MAIN);

    lv_obj_t *grantedTitle = lv_label_create(grantedCard);
    lv_obj_remove_style_all(grantedTitle);
    lv_obj_set_style_text_font(grantedTitle, fontSmall(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(grantedTitle, lv_color_hex(0x888888), LV_STATE_DEFAULT);
    lv_obj_align(grantedTitle, LV_ALIGN_TOP_MID, 0, 10);
    lv_label_set_text(grantedTitle, T("赠金余额", "Granted"));
    ui.detailCont.grantedTitleLabel = grantedTitle;

    lv_obj_t *grantedValue = lv_label_create(grantedCard);
    lv_obj_remove_style_all(grantedValue);
    lv_obj_set_style_text_font(grantedValue, fontBig(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(grantedValue, lv_color_hex(0x333333), LV_STATE_DEFAULT);
    lv_obj_align(grantedValue, LV_ALIGN_CENTER, 0, 8);
    lv_label_set_text(grantedValue, "--");
    ui.detailCont.grantedValueLabel = grantedValue;

    // 充值卡片（右）
    lv_obj_t *toppedCard = lv_obj_create(cont);
    lv_obj_remove_style_all(toppedCard);
    lv_obj_set_size(toppedCard, lv_pct(48), lv_pct(100));
    lv_obj_clear_flag(toppedCard, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(toppedCard, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(toppedCard, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_align(toppedCard, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_radius(toppedCard, 12, LV_PART_MAIN);

    lv_obj_t *toppedTitle = lv_label_create(toppedCard);
    lv_obj_remove_style_all(toppedTitle);
    lv_obj_set_style_text_font(toppedTitle, fontSmall(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(toppedTitle, lv_color_hex(0x888888), LV_STATE_DEFAULT);
    lv_obj_align(toppedTitle, LV_ALIGN_TOP_MID, 0, 10);
    lv_label_set_text(toppedTitle, T("充值余额", "Topped Up"));
    ui.detailCont.toppedTitleLabel = toppedTitle;

    lv_obj_t *toppedValue = lv_label_create(toppedCard);
    lv_obj_remove_style_all(toppedValue);
    lv_obj_set_style_text_font(toppedValue, fontBig(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(toppedValue, lv_color_hex(0x333333), LV_STATE_DEFAULT);
    lv_obj_align(toppedValue, LV_ALIGN_CENTER, 0, 8);
    lv_label_set_text(toppedValue, "--");
    ui.detailCont.toppedValueLabel = toppedValue;
}

void View::footerContCreate(lv_obj_t *obj)
{
    lv_obj_t *cont = lv_obj_create(obj);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, lv_pct(90), 44);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(cont, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x2b2b3a), LV_PART_MAIN);
    lv_obj_align(cont, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_radius(cont, 12, LV_PART_MAIN);
    ui.footerCont.cont = cont;

    // 最后更新时间（左）
    lv_obj_t *updateLabel = lv_label_create(cont);
    lv_obj_remove_style_all(updateLabel);
    lv_obj_set_style_text_font(updateLabel, fontSmall(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(updateLabel, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_align(updateLabel, LV_ALIGN_LEFT_MID, 12, 0);
    lv_label_set_text(updateLabel, T("最后更新：--", "Last update: --"));
    ui.footerCont.updateLabel = updateLabel;

    // 状态消息（右，可滚动）
    lv_obj_t *msgLabel = lv_label_create(cont);
    lv_obj_remove_style_all(msgLabel);
    lv_obj_set_style_text_font(msgLabel, fontSmall(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(msgLabel, lv_color_hex(0xffd76d), LV_STATE_DEFAULT);
    lv_obj_set_width(msgLabel, lv_pct(45));
    lv_label_set_long_mode(msgLabel, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_align(msgLabel, LV_ALIGN_RIGHT_MID, -12, 0);
    lv_label_set_text(msgLabel, "");
    ui.footerCont.msgLabel = msgLabel;
}

/* ============================= UI 更新接口 ============================= */

void View::setNetwork(bool connected)
{
    lv_obj_set_style_bg_color(ui.netCont.dot,
                              connected ? lv_color_hex(0x39d353) : lv_color_hex(0xff5252),
                              LV_PART_MAIN);
    lv_label_set_text(ui.netCont.valueLabel,
                      T(connected ? "网络：已连接" : "网络：未连接",
                        connected ? "Network: Connected" : "Network: Disconnected"));
}

void View::setBalanceText(const char *text)
{
    lv_label_set_text(ui.balanceCont.valueLabel, text);
}

void View::setCurrencyText(const char *text)
{
    lv_label_set_text(ui.balanceCont.currencyLabel, text);
}

void View::setGrantedText(const char *text)
{
    lv_label_set_text(ui.detailCont.grantedValueLabel, text);
}

void View::setToppedText(const char *text)
{
    lv_label_set_text(ui.detailCont.toppedValueLabel, text);
}

void View::setAvailable(bool available)
{
    if (available)
    {
        lv_label_set_text(ui.balanceCont.statusLabel, T("● 可用", "● Available"));
        lv_obj_set_style_text_color(ui.balanceCont.statusLabel, lv_color_hex(0x2e7d32), LV_STATE_DEFAULT);
    }
    else
    {
        lv_label_set_text(ui.balanceCont.statusLabel, T("● 余额不足", "● Unavailable"));
        lv_obj_set_style_text_color(ui.balanceCont.statusLabel, lv_color_hex(0xc62828), LV_STATE_DEFAULT);
    }
}

void View::setStatusMessage(const char *msg)
{
    lv_label_set_text(ui.footerCont.msgLabel, msg);
}

void View::setLastUpdate(const char *text)
{
    char buf[64];
    lv_snprintf(buf, sizeof(buf), "%s%s", T("最后更新：", "Last update: "), text);
    lv_label_set_text(ui.footerCont.updateLabel, buf);
}

void View::setRefreshBusy(bool busy)
{
    if (busy)
    {
        // 旋转 3 圈，约 3.5 秒，模拟刷新过程
        lv_anim_center_rotate(ui.balanceCont.refreshBtn, 3600 * 3, 3500);
    }
}

/* ============================= 按钮 ============================= */

lv_obj_t *View::btnCreate(lv_obj_t *par, const void *img_src, lv_coord_t x_ofs, lv_coord_t y_ofs, lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *obj = lv_obj_create(par);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, w, h);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_align(obj, LV_ALIGN_LEFT_MID, x_ofs, y_ofs);
    lv_obj_set_style_bg_img_src(obj, img_src, LV_STATE_DEFAULT);

    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_width(obj, w / 1.1f, LV_STATE_PRESSED);  // 按下时缩小
    lv_obj_set_style_height(obj, h / 1.1f, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x356b8c), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x242947), LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xf2daaa), LV_STATE_FOCUSED);
    lv_obj_set_style_radius(obj, 9, LV_STATE_DEFAULT);

    static lv_style_transition_dsc_t tran;
    static const lv_style_prop_t prop[] = {LV_STYLE_WIDTH, LV_STYLE_HEIGHT, LV_STYLE_PROP_INV};
    lv_style_transition_dsc_init(&tran, prop, lv_anim_path_ease_out, 150, 0, NULL);
    lv_obj_set_style_transition(obj, &tran, LV_STATE_PRESSED);
    lv_obj_set_style_transition(obj, &tran, LV_STATE_FOCUSED);

    lv_obj_update_layout(obj);

    return obj;
}

/* ============================= 事件回调 ============================= */

void View::topContEventHandler(lv_event_t *event)
{
    View *instance = (View *)lv_event_get_user_data(event);
    LV_ASSERT_NULL(instance);

    lv_obj_t *obj = lv_event_get_current_target(event);

    if (obj == instance->ui.topCont.exitBtn)
    {
        log_info("[View] exitBtn clicked!");
        if (instance->_opts.exitCb)
            instance->_opts.exitCb();
    }
    else if (obj == instance->ui.topCont.shotBtn)
    {
        log_info("[View] shotBtn clicked!");
        instance->screenshot();
        instance->sideTipsPopupCreate(lv_layer_top(), instance->T("截图已保存", "Snapshot saved!"));
    }
}

void View::balanceContEventHandler(lv_event_t *event)
{
    View *instance = (View *)lv_event_get_user_data(event);
    LV_ASSERT_NULL(instance);

    lv_obj_t *obj = lv_event_get_current_target(event);

    if (obj == instance->ui.balanceCont.refreshBtn)
    {
        log_info("[View] refreshBtn clicked!");
        instance->setRefreshBusy(true);
        if (instance->_opts.refreshCb)
            instance->_opts.refreshCb();
    }
}

void View::sideTipsPopupCreate(lv_obj_t *obj, const char *tips)
{
    lv_obj_t *sidePop = lv_obj_create(obj);
    lv_obj_remove_style_all(sidePop);
    lv_obj_set_size(sidePop, 160, 40);
    lv_obj_set_style_bg_opa(sidePop, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(sidePop, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_align(sidePop, LV_ALIGN_BOTTOM_RIGHT, 0, -10);
    lv_obj_set_style_radius(sidePop, 10, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(sidePop);
    lv_obj_remove_style_all(label);
    lv_obj_set_style_text_font(label, fontMid(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(label, tips);
    lv_anim_move(sidePop, 10, -90, 700, 0);

    // 一次性定时器，1.5 秒后淡出
    lv_timer_t *timer = lv_timer_create([](lv_timer_t *timer)
                                        {
        lv_obj_t *sidePop = (lv_obj_t *)timer->user_data;
        lv_anim_drop_out(sidePop); }, 1500, sidePop);
    lv_timer_set_repeat_count(timer, 1);
}

/* ============================= 截图 ============================= */

void View::convertRGB2BGR(lv_img_dsc_t *snapshot)
{
    uint8_t tmp_data = 0;
    uint32_t count = 0;
    for (int w = 0; w < snapshot->header.w; w++)
    {
        for (int h = 0; h < snapshot->header.h; h++)
        {
            tmp_data = *(snapshot->data + count);
            *(uint8_t *)(snapshot->data + count) = *(snapshot->data + count + 2);
            *(uint8_t *)(snapshot->data + count + 2) = tmp_data;
            count += 4;
        }
    }
}

void View::screenshot(void)
{
    time_t timep;
    struct tm *p;
    char timeBuffer[64];
    char fileNameBuffer[128];

    time(&timep);
    p = localtime(&timep);
    strftime(timeBuffer, sizeof(timeBuffer), "screenshot-%Y%m%d-%H%M%S", p);

    lv_snprintf(fileNameBuffer, sizeof(fileNameBuffer), "%s.png", timeBuffer);

    lv_img_dsc_t *snapshot = lv_snapshot_take(lv_scr_act(), LV_IMG_CF_TRUE_COLOR_ALPHA);
    if (snapshot == nullptr)
    {
        log_error("[View] snapshot failed!");
        return;
    }

    // PNG 期望 BGR，lv_snapshot 得到 RGB，需转换
    convertRGB2BGR(snapshot);

    // 使用内存编码（lodepng_encode32），再用标准文件 I/O 写出，
    // 避免依赖 LVGL 文件系统驱动的 lv_fs_open
    unsigned char *pngBuf = nullptr;
    size_t pngSize = 0;
    unsigned int error = lodepng_encode32(&pngBuf, &pngSize, snapshot->data,
                                          snapshot->header.w, snapshot->header.h);

    lv_snapshot_free(snapshot);

    if (error != 0 || pngBuf == nullptr)
    {
        log_error("[View] lodepng encode failed: %s", lodepng_error_text(error));
        return;
    }

    FILE *fp = fopen(fileNameBuffer, "wb");
    if (fp != nullptr)
    {
        fwrite(pngBuf, 1, pngSize, fp);
        fclose(fp);
        log_info("[View] screenshot -> %s (%zu bytes)", fileNameBuffer, pngSize);
    }
    else
    {
        log_error("[View] cannot open %s for writing", fileNameBuffer);
    }

    lv_mem_free(pngBuf);
}
