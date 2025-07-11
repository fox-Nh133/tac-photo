#include "lvgl.h"
#include "photo_display.h"

void photo_display_show_image(const char *path)
{
    lv_obj_t *img = lv_img_create(lv_scr_act());
    lv_img_set_src(img, path);  // 例: "S:/sample.jpg"
    lv_obj_center(img);
}
