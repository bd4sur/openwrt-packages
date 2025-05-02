#include "OLED.h"

static u8 t = 0;

static const int content[9] = {1547, 1372, 1899, 29, 3074, 2343, 1281, 899, 4389};

void setup() {
    Wire.begin();
    OLED_Init();
    OLED_Clear();
}

void loop() {
    OLED_SoftClear();

    int line_width = 0;
    
    for (int i = 0; i < 9; i++) {
        int current_char = content[i];
        if (current_char > 94) {
            OLED_ShowChar(line_width, 0, GB2312_12_12[current_char-94], 12, 12, 1);
            line_width += 12;
        }
        else {
            OLED_ShowChar(line_width, 0, ASCII_6_12[current_char], 6, 12, 1);
            line_width += 6;
        }
        if (i >= (t % 9)) break;
    }

    OLED_DrawLine(0, 59, 128, 59, 1);
    OLED_DrawLine(0, 63, 128, 63, 1);
    OLED_DrawLine(127, 59, 127, 63, 1);
    OLED_DrawLine(0, 60, t, 60, 1);
    OLED_DrawLine(0, 61, t, 61, 1);
    OLED_DrawLine(0, 62, t, 62, 1);
    OLED_Refresh();
    t++;
    if(t > 127) t = 0;
}
