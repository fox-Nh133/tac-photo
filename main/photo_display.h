#ifndef PHOTO_DISPLAY_H
#define PHOTO_DISPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Show a JPEG image using LVGL from the given file path.
 * 
 * @param path File path to the JPEG image (e.g. "S:/sample.jpg")
 */
void photo_display_show_image(const char *path);

#ifdef __cplusplus
}
#endif

#endif // PHOTO_DISPLAY_H
