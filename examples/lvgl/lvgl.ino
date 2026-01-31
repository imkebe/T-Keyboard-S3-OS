/*
 * @Description: None
 * @version: None
 * @Author: None
 * @Date: 2023-06-10 17:30:22
 * @LastEditors: LILYGO_L
 * @LastEditTime: 2023-12-19 16:15:48
 */
#include <Arduino.h>
#include "lvgl.h"
#include "TFT_eSPI.h"
#include "pin_config.h"
#include "T-Keyboard_S3_Drive.h"
#include "FastLED.h"
#include "../../src/config/ConfigLoader.h"
#include "../../src/actions/ActionDispatcher.h"
#include "../../src/actions/ActionRegistry.h"

#include <array>

// How many leds in your strip?
#define NUM_LEDS 4

#define DATA_PIN WS2812B_DATA

// Define the array of leds
CRGB leds[NUM_LEDS];

/*Change to your screen resolution*/
static const uint16_t screenWidth = 128;
static const uint16_t screenHeight = 128;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * screenHeight / 10];

TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight); /* TFT instance */

namespace
{
constexpr const char *kMissingAssetLabel = LV_SYMBOL_WARNING "\nMissing";
constexpr uint8_t kDisplayCount = 4;
constexpr uint8_t kProfileSwitchEventId = 10;

struct KeyDisplayData
{
    std::string label;
    std::string icon_path;
    bool icon_available = false;
};

struct FsDriverContext
{
    fs::FS *fs = nullptr;
};

FsDriverContext sd_context;
FsDriverContext spiffs_context;
ConfigLoader config_loader;
ActionRegistry action_registry;
ActionDispatcher action_dispatcher(action_registry);
std::array<KeyDisplayData, kDisplayCount> displays;
std::array<const KeyConfig *, kDisplayCount> key_bindings{};
bool sd_ready = false;
bool spiffs_ready = false;
std::string active_profile_id;

bool StartsWith(const std::string &value, const std::string &prefix)
{
    if (value.size() < prefix.size())
    {
        return false;
    }
    return value.compare(0, prefix.size(), prefix) == 0;
}

bool EnsureLeadingSlash(std::string &path)
{
    if (path.empty())
    {
        return false;
    }
    if (path.front() != '/')
    {
        path.insert(path.begin(), '/');
    }
    return true;
}

bool FileExists(fs::FS &fs, const char *path)
{
    File file = fs.open(path, "r");
    if (!file)
    {
        return false;
    }
    file.close();
    return true;
}

void *LvglFsOpen(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    auto *context = static_cast<FsDriverContext *>(drv->user_data);
    if (!context || !context->fs)
    {
        return nullptr;
    }
    const char *open_mode = (mode & LV_FS_MODE_WR) ? "w" : "r";
    File file = context->fs->open(path, open_mode);
    if (!file)
    {
        return nullptr;
    }
    return new File(file);
}

lv_fs_res_t LvglFsClose(lv_fs_drv_t *, void *file_p)
{
    auto *file = static_cast<File *>(file_p);
    if (!file)
    {
        return LV_FS_RES_FS_ERR;
    }
    file->close();
    delete file;
    return LV_FS_RES_OK;
}

lv_fs_res_t LvglFsRead(lv_fs_drv_t *, void *file_p, void *buf, uint32_t btr, uint32_t *br)
{
    auto *file = static_cast<File *>(file_p);
    if (!file)
    {
        return LV_FS_RES_FS_ERR;
    }
    *br = file->read(static_cast<uint8_t *>(buf), btr);
    return LV_FS_RES_OK;
}

lv_fs_res_t LvglFsSeek(lv_fs_drv_t *, void *file_p, uint32_t pos, lv_fs_whence_t whence)
{
    auto *file = static_cast<File *>(file_p);
    if (!file)
    {
        return LV_FS_RES_FS_ERR;
    }
    uint32_t target = pos;
    if (whence == LV_FS_SEEK_CUR)
    {
        target = file->position() + pos;
    }
    else if (whence == LV_FS_SEEK_END)
    {
        target = file->size() + pos;
    }
    if (!file->seek(target))
    {
        return LV_FS_RES_FS_ERR;
    }
    return LV_FS_RES_OK;
}

lv_fs_res_t LvglFsTell(lv_fs_drv_t *, void *file_p, uint32_t *pos)
{
    auto *file = static_cast<File *>(file_p);
    if (!file)
    {
        return LV_FS_RES_FS_ERR;
    }
    *pos = file->position();
    return LV_FS_RES_OK;
}

void RegisterLvglFsDriver(char letter, FsDriverContext &context, lv_fs_drv_t &driver)
{
    lv_fs_drv_init(&driver);
    driver.letter = letter;
    driver.user_data = &context;
    driver.open_cb = LvglFsOpen;
    driver.close_cb = LvglFsClose;
    driver.read_cb = LvglFsRead;
    driver.seek_cb = LvglFsSeek;
    driver.tell_cb = LvglFsTell;
    lv_fs_drv_register(&driver);
}

bool ResolveIconPath(const std::string &icon, std::string &out_lvgl_path, bool sd_ready, bool spiffs_ready)
{
    if (icon.empty())
    {
        return false;
    }

    std::string path = icon;
    if (StartsWith(path, "S:") || StartsWith(path, "F:"))
    {
        out_lvgl_path = path;
        return true;
    }

    char drive_letter = '\0';
    fs::FS *target_fs = nullptr;

    if (StartsWith(path, "sd:"))
    {
        drive_letter = 'S';
        path = path.substr(3);
        target_fs = sd_ready ? &SD : nullptr;
    }
    else if (StartsWith(path, "fs:") || StartsWith(path, "spiffs:"))
    {
        drive_letter = 'F';
        path = path.substr(path.find(':') + 1);
        target_fs = spiffs_ready ? &SPIFFS : nullptr;
    }

    if (drive_letter == '\0' || !target_fs)
    {
        return false;
    }

    if (!EnsureLeadingSlash(path))
    {
        return false;
    }

    if (!FileExists(*target_fs, path.c_str()))
    {
        return false;
    }

    out_lvgl_path = std::string(1, drive_letter) + ":" + path;
    return true;
}

const ProfileConfig *ResolveActiveProfile(const ConfigRoot &config)
{
    return config.ActiveProfile();
}

const std::vector<KeyConfig> &ResolveKeys(const ConfigRoot &config, const ProfileConfig *profile)
{
    return profile ? profile->keys : config.keys;
}

const std::vector<ActionConfig> &ResolveActions(const ConfigRoot &config, const ProfileConfig *profile)
{
    return profile ? profile->actions : config.actions;
}

const ActionConfig *FindActionById(const std::vector<ActionConfig> &actions, const std::string &action_id)
{
    for (const auto &action : actions)
    {
        if (action.id == action_id)
        {
            return &action;
        }
    }
    return nullptr;
}

void BuildKeyBindings(const std::vector<KeyConfig> &keys)
{
    key_bindings.fill(nullptr);
    for (const auto &key : keys)
    {
        if (key.key_index < kDisplayCount)
        {
            key_bindings[key.key_index] = &key;
        }
    }
}

void ApplyProfile(const ConfigRoot &config, const ProfileConfig *profile)
{
    for (uint8_t i = 0; i < kDisplayCount; ++i)
    {
        displays[i].label = "Key " + std::to_string(i + 1);
        displays[i].icon_path.clear();
        displays[i].icon_available = false;
    }

    const auto &keys = ResolveKeys(config, profile);
    BuildKeyBindings(keys);

    for (const auto &key : keys)
    {
        if (!key.enabled || key.key_index >= kDisplayCount)
        {
            continue;
        }
        displays[key.key_index].label = key.label.empty() ? displays[key.key_index].label : key.label;
        if (!key.icon.empty())
        {
            std::string resolved;
            if (ResolveIconPath(key.icon, resolved, sd_ready, spiffs_ready))
            {
                displays[key.key_index].icon_path = resolved;
                displays[key.key_index].icon_available = true;
            }
        }
    }

    for (uint8_t i = 0; i < kDisplayCount; ++i)
    {
        RenderKeyDisplay(i, displays[i]);
        lv_timer_handler();
        delay(10);
    }
}

bool SwitchToProfileId(const std::string &profile_id)
{
    if (!config_loader.SetActiveProfile(profile_id))
    {
        return false;
    }
    active_profile_id = profile_id;
    ApplyProfile(config_loader.config(), ResolveActiveProfile(config_loader.config()));
    return true;
}

bool CycleProfile()
{
    const auto &config = config_loader.config();
    if (config.profiles.empty())
    {
        return false;
    }

    size_t current_index = 0;
    const auto *active = ResolveActiveProfile(config);
    if (active)
    {
        for (size_t i = 0; i < config.profiles.size(); ++i)
        {
            if (config.profiles[i].id == active->id)
            {
                current_index = i;
                break;
            }
        }
    }

    size_t next_index = (current_index + 1) % config.profiles.size();
    return SwitchToProfileId(config.profiles[next_index].id);
}

ActionStatus HandleProfileSwitchAction(const ActionConfig &action, const ActionDispatcher &)
{
    if (action.payload.empty())
    {
        return ActionStatus::Failure(-1, "profile_switch payload is required");
    }
    if (action.payload == "next" || action.payload == "cycle")
    {
        return CycleProfile() ? ActionStatus::Ok() : ActionStatus::Failure(-1, "No profiles to cycle");
    }
    return SwitchToProfileId(action.payload) ? ActionStatus::Ok() : ActionStatus::Failure(-1, "Unknown profile id");
}

void DispatchKeyBinding(uint8_t key_index)
{
    if (key_index >= kDisplayCount)
    {
        return;
    }

    const KeyConfig *key = key_bindings[key_index];
    if (!key || !key->enabled)
    {
        return;
    }

    const auto &config = config_loader.config();
    const auto *profile = ResolveActiveProfile(config);
    const auto &actions = ResolveActions(config, profile);

    if (!key->actions.empty())
    {
        action_dispatcher.DispatchActions(key->actions);
        return;
    }

    if (!key->action_id.empty())
    {
        if (const auto *action = FindActionById(actions, key->action_id))
        {
            action_dispatcher.DispatchAction(*action);
        }
        else
        {
            Serial.println(String("ConfigLoader: Unknown action_id ") + key->action_id.c_str());
        }
    }
}

void RenderKeyDisplay(uint8_t screen_index, const KeyDisplayData &data)
{
    static const uint8_t kScreenMap[kDisplayCount] = {
        N085_Screen_1,
        N085_Screen_2,
        N085_Screen_3,
        N085_Screen_4,
    };

    N085_Screen_Set(kScreenMap[screen_index]);

    lv_obj_t *root = lv_scr_act();
    lv_obj_clean(root);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x0B0B0B), LV_PART_MAIN);

    if (data.icon_available && !data.icon_path.empty())
    {
        lv_obj_t *img = lv_img_create(root);
        lv_img_set_src(img, data.icon_path.c_str());
        lv_obj_align(img, LV_ALIGN_CENTER, 0, -6);
    }
    else
    {
        lv_obj_t *missing = lv_label_create(root);
        lv_label_set_text(missing, kMissingAssetLabel);
        lv_obj_align(missing, LV_ALIGN_CENTER, 0, -6);
    }

    lv_obj_t *label = lv_label_create(root);
    const char *label_text = data.label.empty() ? "Key" : data.label.c_str();
    lv_label_set_text(label, label_text);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -6);
}
} // namespace

void WS2812B_KEY_Lvgl_Loop(void)
{
    switch (Key1_Flag)
    {
    case 0:
        leds[0] = CRGB::Black;
        FastLED.show();
        break;
    case 1:
        leds[0] = CRGB::White;
        FastLED.show();

        N085_Screen_Set(N085_Screen_1);
        break;

    default:
        break;
    }

    switch (Key2_Flag)
    {
    case 0:
        leds[1] = CRGB::Black;
        FastLED.show();
        break;
    case 1:
        leds[1] = CRGB::White;
        FastLED.show();

        N085_Screen_Set(N085_Screen_2);
        break;

    default:
        break;
    }

    switch (Key3_Flag)
    {
    case 0:
        leds[2] = CRGB::Black;
        FastLED.show();
        break;
    case 1:
        leds[2] = CRGB::White;
        FastLED.show();

        N085_Screen_Set(N085_Screen_3);
        break;

    default:
        break;
    }

    switch (Key4_Flag)
    {
    case 0:
        leds[3] = CRGB::Black;
        FastLED.show();
        break;
    case 1:
        leds[3] = CRGB::White;
        FastLED.show();

        N085_Screen_Set(N085_Screen_4);
        break;

    default:
        break;
    }
}

// /*Read the touchpad*/
// void my_touchpad_read( lv_indev_drv_t * indev_drv, lv_indev_data_t * data )
// {
//     uint16_t touchX, touchY;

//     bool touched = tft.getTouch( &touchX, &touchY, 600 );

//     if( !touched )
//     {
//         data->state = LV_INDEV_STATE_REL;
//     }
//     else
//     {
//         data->state = LV_INDEV_STATE_PR;

//         /*Set the coordinates*/
//         data->point.x = touchX;
//         data->point.y = touchY;

//         Serial.print( "Data x " );
//         Serial.println( touchX );

//         Serial.print( "Data y " );
//         Serial.println( touchY );
//     }
// }

/*Read the touchpad*/
void my_touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    uint16_t touchX = 64, touchY = 64;

    bool touched = Key1_Flag || Key2_Flag || Key3_Flag || Key4_Flag;

    if (!touched)
    {
        data->state = LV_INDEV_STATE_REL;
    }
    else
    {
        data->state = LV_INDEV_STATE_PR;

        /*Set the coordinates*/
        data->point.x = touchX;
        data->point.y = touchY;

        // Serial.print("Data x ");
        // Serial.println(touchX);

        // Serial.print("Data y ");
        // Serial.println(touchY);
    }
}

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp_drv);
}

void lvgl_initialization()
{
    String LVGL_Arduino = "Hello Arduino! ";
    LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();

    Serial.println(LVGL_Arduino);
    Serial.println("I am LVGL_Arduino");

    lv_init();

    lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * screenHeight / 10);

    /*Initialize the display*/
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    /*Change the following line to your display resolution*/
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    /*Initialize the (dummy) input device driver*/
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);
}

void lvgl_background()
{
    /*Change the active screen's background color*/
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0xAFF1FF), LV_PART_MAIN);
}

void lv_style_img(void)
{
    static lv_style_t style;                               // 创建样式
    lv_style_init(&style);                                 // 初始化样式
    lv_style_set_border_side(&style, LV_BORDER_SIDE_NONE); // 设置样式边框显示范围
    // lv_style_set_radius(&style, 0); // 设置圆角半径

    LV_IMG_DECLARE(LOGO2);                         // 加载图片声明 .c文件的图片
    lv_style_set_bg_img_src(&style, &LOGO2);       // 设置背景图片
    lv_style_set_bg_img_opa(&style, LV_OPA_COVER); // 设置背景图片透明度
    // lv_style_set_bg_img_recolor(&style, lv_palette_main(LV_PALETTE_INDIGO)); // 设置背景图片重着色
    // lv_style_set_bg_img_recolor_opa(&style, LV_OPA_20);                      // 设置背景图片重着色透明度
    lv_style_set_bg_img_tiled(&style, false); // 设置背景图片平铺

    lv_style_set_x(&style, lv_pct(10)); // 设置样式的x位置 ，基准点左上角
    lv_style_set_y(&style, 79);         // 设置样式的y位置

    /*Create an object with the new style*/
    lv_obj_t *obj = lv_obj_create(lv_scr_act()); // 创建对象
    lv_obj_add_style(obj, &style, 0);            // 设置对象的样式
    lv_obj_set_size(obj, 105, 45);               // 设置对象的尺寸

    // lv_obj_center(obj);
}

static void anim_x_cb(void *var, int32_t v)
{
    lv_obj_set_x((_lv_obj_t *)var, v);
}

static void sw_event_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    lv_obj_t *label = (lv_obj_t *)lv_event_get_user_data(e);

    if (lv_obj_has_state(sw, LV_STATE_CHECKED))
    {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, label);
        lv_anim_set_values(&a, lv_obj_get_x(label), 11);
        lv_anim_set_time(&a, 500);
        lv_anim_set_exec_cb(&a, anim_x_cb);
        lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
        lv_anim_start(&a);
    }
    else
    {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, label);
        lv_anim_set_values(&a, lv_obj_get_x(label), -lv_obj_get_width(label));
        lv_anim_set_time(&a, 500);
        lv_anim_set_exec_cb(&a, anim_x_cb);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
        lv_anim_start(&a);
    }
}

/**
 * Start animation on an event
 */
void lv_example_anim_1(void)
{
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_recolor(label, true);
    lv_label_set_text(label, "         #FF864C Ciallo#\n#195194 T-Keyboard-S3#");
    lv_obj_set_pos(label, -150, 12);

    lv_obj_t *sw = lv_switch_create(lv_scr_act());
    lv_obj_center(sw);
    lv_obj_add_state(sw, LV_STATE_CHECKED);
    lv_obj_clear_state(sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw, sw_event_cb, LV_EVENT_VALUE_CHANGED, label);
}

void setup()
{
    /* TFT init */
    Serial.begin(115200);
    Serial.println("Ciallo");

    T_Keyboard_S3_Key_Initialization();

    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS); // GRB ordering is typical
    FastLED.setBrightness(50);

    ledcAttachPin(N085_BL, 1);
    ledcSetup(1, 20000, 8);
    ledcWrite(1, 0);

    N085_Screen_Set(N085_Initialize);
    N085_Screen_Set(N085_Screen_ALL);
    tft.begin();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);

    lvgl_initialization();

    lvgl_background();

    lv_style_img();

    lv_example_anim_1();

    static lv_fs_drv_t sd_driver;
    static lv_fs_drv_t spiffs_driver;
    sd_ready = SD.begin();
    spiffs_ready = SPIFFS.begin();
    if (sd_ready)
    {
        sd_context.fs = &SD;
        RegisterLvglFsDriver('S', sd_context, sd_driver);
    }
    if (spiffs_ready)
    {
        spiffs_context.fs = &SPIFFS;
        RegisterLvglFsDriver('F', spiffs_context, spiffs_driver);
    }

    action_registry.Register("profile_switch", HandleProfileSwitchAction);

    if (config_loader.reloadConfig())
    {
        const auto &config = config_loader.config();
        T_Keyboard_S3_Set_Key_Debounce(config.debounce_ms);
        const auto *profile = ResolveActiveProfile(config);
        if (profile)
        {
            active_profile_id = profile->id;
        }
        ApplyProfile(config, profile);
    }
    else
    {
        Serial.println(String("ConfigLoader: ") + config_loader.lastError().c_str());
        ConfigRoot fallback;
        ApplyProfile(fallback, nullptr);
    }

    lv_timer_handler();

    delay(1000);

    for (int j = 0; j < 2; j++)
    {
        delay(2000);
        for (int i = 0; i < 255; i++)
        {
            ledcWrite(1, i);
            delay(2);
        }
        for (int i = 255; i > 0; i--)
        {
            ledcWrite(1, i);
            delay(5);
        }
    }
    delay(2000);
    for (int i = 0; i < 255; i++)
    {
        ledcWrite(1, i);
        delay(2);
    }
}

void loop()
{
    lv_timer_handler();
    // delay(5);

    uint8_t key_event = T_Keyboard_S3_Key_Trigger();
    if (key_event == kProfileSwitchEventId)
    {
        CycleProfile();
    }
    else if (key_event >= 1 && key_event <= kDisplayCount)
    {
        DispatchKeyBinding(static_cast<uint8_t>(key_event - 1));
    }

    WS2812B_KEY_Lvgl_Loop();
}
