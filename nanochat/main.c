#include "avltree.h"
#include "pinyin.h"
#include "oled.h"
#include "keyboard.h"
#include "infer.h"

#define INPUT_BUFFER_LENGTH (1024)

#define IME_MODE_HANZI    (0)
#define IME_MODE_ALPHABET (1)
#define IME_MODE_NUMBER   (2)

static Nano_Context ctx;

static char *MODEL_PATH_1 = "/emmc/_model/nano_168m_625000_sft_947000.bin";
static char *MODEL_PATH_2 = "/emmc/_model/nano_56m_99000_sft_v2_200000.bin";
static char *MODEL_PATH_3 = "/emmc/_model/1-基础模型-99000.bin";
static char *LORA_PATH_3  = "/emmc/_model/2-插件-猫娘.bin";

void load_model(char *model_path, char *lora_path, float repetition_penalty, float temperature, float top_p, unsigned int top_k, unsigned long long rng_seed) {
    ctx.random_seed = rng_seed;
    ctx.llm = (LLM *)calloc(1, (sizeof(LLM)));
    ctx.tokenizer = (Tokenizer *)calloc(1, (sizeof(Tokenizer)));
    load_llm(ctx.llm, ctx.tokenizer, model_path);
    ctx.sampler = build_sampler(ctx.llm->config.vocab_size, repetition_penalty, temperature, top_p, top_k, ctx.random_seed);
    ctx.lora = (lora_path) ? load_lora(ctx.llm, lora_path) : NULL;
}

void unload_model() {
    free_llm(ctx.llm, ctx.tokenizer);
    free_sampler(ctx.sampler);
}

uint32_t on_prefilling(wchar_t *prompt, uint32_t pos, uint32_t num_prompt_tokens) {
    // 按住A键中止推理
    char key = keyboard_read_key();
    if (key == 10) {
        printf("推理中止\n");
        return 1;
    }
    render_text(L"Pre-filling...");
    OLED_DrawLine(0, 60, 128, 60, 1);
    OLED_DrawLine(0, 63, 128, 63, 1);
    OLED_DrawLine(127, 60, 127, 63, 1);
    OLED_DrawLine(0, 61, pos * 128 / (num_prompt_tokens - 2), 61, 1);
    OLED_DrawLine(0, 62, pos * 128 / (num_prompt_tokens - 2), 62, 1);
    OLED_Refresh();
    return 0;
}

uint32_t on_decoding(wchar_t *output, uint32_t pos, float tps) {
    // 按住A键中止推理
    char key = keyboard_read_key();
    if (key == 10) {
        printf("推理中止\n");
        return 1;
    }
    OLED_SoftClear();
    render_text(output);
    OLED_Refresh();
    return 0;
}

uint32_t on_finished(float tps, uint32_t status) {
    // OLED_SoftClear();
    // wchar_t tps_wcstr[20];
    // swprintf(tps_wcstr, 20, L"TPS=%.2f", tps);
    // render_text(tps_wcstr);
    // OLED_Refresh();
    printf("TPS = %f\n", tps);
    return 0;
}

void show_splash_screen() {
    OLED_SoftClear();

    // render_text(L" \n    Project MARGA!   \n  Powered by Nano LM\n   BD4SUR  2025-04   ");
    render_line(L"Project MARGA!", 24, 2);
    render_line(L"完全离线电子鹦鹉", 16, 20);
    render_line(L"自研Nano模型强力驱动", 4, 34);
    render_line(L"BD4SUR 2025-4", 26, 50);

    OLED_DrawLine(0, 0, 127, 0, 1);
    OLED_DrawLine(0, 15, 127, 15, 1);
    OLED_DrawLine(0, 0, 0, 63, 1);
    OLED_DrawLine(127, 0, 127, 63, 1);
    OLED_DrawLine(0, 63, 127, 63, 1);

    OLED_Refresh();
}

void render_input_buffer(uint32_t *input_buffer, uint32_t ime_mode_flag, uint32_t is_show_cursor) {
    OLED_SoftClear();
    wchar_t text[1024] = L"请输入问题：     [";
    if (ime_mode_flag == IME_MODE_HANZI) {
        wcscat(text, L"汉]\n");
    }
    else if (ime_mode_flag == IME_MODE_ALPHABET) {
        wcscat(text, L"En]\n");
    }
    else if (ime_mode_flag == IME_MODE_NUMBER) {
        wcscat(text, L"数]\n");
    }
    wcscat(text, input_buffer);
    if (is_show_cursor) wcscat(text, L"_");
    render_text(text);
    OLED_Refresh();
}

void render_pinyin_input(uint32_t **candidate_pages, uint32_t pinyin_keys, uint32_t current_page, uint32_t candidate_page_num, uint32_t is_picking) {
    OLED_SoftClear();
    // 计算候选列表长度
    uint32_t candidate_num = 0;
    wchar_t cc[11];
    wchar_t cindex[21] = L"1 2 3 4 5 6 7 8 9 0 ";
    if (candidate_pages) {
        for(int j = 0; j < 10; j++) {
            wchar_t ch = candidate_pages[current_page][j];
            if (!ch) break;
            cc[j] = ch;
            candidate_num++;
        }
        cc[candidate_num] = 0;
        cindex[candidate_num << 1] = 0;
    }

    wchar_t text[1024];
    if (is_picking) {
        swprintf(text, 1024, L" \n\nPY[%-6d]   (%2d/%2d)\n", pinyin_keys, (current_page+1), candidate_page_num);
        wcscat(text, cindex);
        wcscat(text, L"\n");
    }
    else {
        swprintf(text, 1024, L" \n\nPY[%-6d]\n\n", pinyin_keys);
    }
    if (candidate_pages) {
        wcscat(text, cc);
    }
    else {
        wcscat(text, L"(无候选字)");
    }
    render_text(text);
    OLED_Refresh();
}

void render_symbol_input(uint32_t **candidate_pages, uint32_t current_page, uint32_t candidate_page_num) {
    OLED_SoftClear();
    // 计算候选列表长度
    uint32_t candidate_num = 0;
    uint32_t list_char_width = 0;
    wchar_t cc[21];
    wchar_t cindex[21] = L"1 2 3 4 5 6 7 8 9 0 ";
    if (candidate_pages) {
        for(int j = 0; j < 10; j++) {
            wchar_t ch = candidate_pages[current_page][j];
            if (!ch) {
                break;
            }
            else if (ch < 127) {
                cc[list_char_width++] = ch;
                cc[list_char_width++] = L' ';
            }
            else {
                cc[list_char_width++] = ch;
            }
            candidate_num++;
        }
        cc[list_char_width] = 0;
        cindex[candidate_num << 1] = 0;
    }

    wchar_t text[1024];
    swprintf(text, 1024, L" \n\nSymbols      (%2d/%2d)\n", (current_page+1), candidate_page_num);
    wcscat(text, cindex);
    wcscat(text, L"\n");

    if (candidate_pages) {
        wcscat(text, cc);
    }
    else {
        wcscat(text, L"(无候选符号)");
    }
    render_text(text);
    OLED_Refresh();
}

uint32_t *refresh_input_buffer(uint32_t *input_buffer, uint32_t *input_counter) {
    if (input_counter) *input_counter = 0;
    uint32_t *new_input_buffer = (uint32_t *)realloc(input_buffer, INPUT_BUFFER_LENGTH * sizeof(uint32_t));
    if (!new_input_buffer) {
        free(input_buffer);
        return (uint32_t *)calloc(INPUT_BUFFER_LENGTH, sizeof(uint32_t));
    }
    else {
        for (uint32_t i = 0; i < INPUT_BUFFER_LENGTH; i++) new_input_buffer[i] = 0;
        return new_input_buffer;
    }
}


int main() {
    if(!setlocale(LC_CTYPE, "")) return -1;

    ///////////////////////////////////////
    // OLED 初始化

    OLED_Init();
    OLED_Clear();

    ////////////////////////////////////////////////
    // LLM Init

    render_text(L" 正在加载语言模型\n Nano-168M-QA\n 请稍等...");
    OLED_Refresh();

    float repetition_penalty = 1.1f;
    float temperature = 1.0f;
    float top_p = 0.5f;
    unsigned int top_k = 0;
    unsigned long long random_seed = (unsigned int)time(NULL);
    uint32_t max_seq_len = 512;

    load_model(MODEL_PATH_1, NULL, repetition_penalty, temperature, top_p, top_k, random_seed);

    ///////////////////////////////////////
    // 矩阵按键初始化与读取

    if(keyboard_init() < 0) return -1;
    char prev_key = 0;

    // 全局状态标志
    uint32_t STATE = 0;

    // 汉英数输入模式标志
    uint32_t ime_mode_flag = 0; // 0汉字 1英文 2数字

    // 符号列表
    wchar_t symbols[55] = L"，。、？！：；“”‘’（）《》…―～・【】 !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";

    // 按键对应的字母列表
    wchar_t alphabet[10][9] = {L"", L"", L"abcABC", L"defDEF", L"ghiGHI", L"jklJKL", L"mnoMNO", L"pqrsPRQS", L"tuvTUV", L"wxyzWXYZ"};

    // 单字拼音键码暂存
    uint32_t pinyin_keys = 0;

    // 候选字翻页相关
    uint32_t *candidates = NULL;
    uint32_t candidate_num = 0;
    uint32_t **candidate_pages = NULL;
    uint32_t candidate_page_num = 0;
    uint32_t current_page = 0;

    // 全局文字输入缓冲
    uint32_t *input_buffer = (uint32_t *)calloc(INPUT_BUFFER_LENGTH, sizeof(uint32_t)); // 文字输入缓冲区
    uint32_t input_counter = 0;

    show_splash_screen();

    while (1) {
        char key = keyboard_read_key();
        if(key != 16 && key != prev_key) {
            printf("当前按键：%d\n", key);

            switch(STATE) {

            /////////////////////////////////////////////
STATE_0:    // 初始状态：等待输入拼音/字母/数字，或者将文字输入缓冲区的内容提交给大模型
            /////////////////////////////////////////////

            case 0:

                // 0：输入符号（数字输入模式下是直接输入0）
                if (key == 0) {
                    if (ime_mode_flag == IME_MODE_NUMBER) {
                        input_buffer[input_counter++] = L'0';
                        render_input_buffer(input_buffer, ime_mode_flag, 1);
                        STATE = 0;
                    }
                    else {
                        candidates = (uint32_t *)calloc(54, sizeof(uint32_t));
                        for (int i = 0; i < 54; i++) candidates[i] = (uint32_t)symbols[i];
                        candidate_pages = candidate_paging(candidates, 54, 10, &candidate_page_num);
                        render_symbol_input(candidate_pages, current_page, candidate_page_num);

                        current_page = 0;
                        STATE = 3;
                    }
                }

                // 1：数字输入模式下是直接输入1，其余模式无动作
                else if (key == 1) {
                    if (ime_mode_flag == IME_MODE_NUMBER) {
                        input_buffer[input_counter++] = L'1';
                        render_input_buffer(input_buffer, ime_mode_flag, 1);
                        STATE = 0;
                    }
                }

                // 2-9：输入拼音/字母/数字，根据输入模式标志，转向不同的状态
                else if (key >= 2 && key <= 9) {
                    if (ime_mode_flag == IME_MODE_HANZI) {
                        STATE = 1;
                        goto STATE_1;
                    }
                    else if (ime_mode_flag == IME_MODE_ALPHABET) {
                        candidates = (uint32_t *)calloc(9, sizeof(uint32_t)); // 每个按键最多有8个字母（大小写）
                        if      (key == 2) for (int i = 0; i < 6; i++) candidates[i] = (uint32_t)alphabet[2][i];
                        else if (key == 3) for (int i = 0; i < 6; i++) candidates[i] = (uint32_t)alphabet[3][i];
                        else if (key == 4) for (int i = 0; i < 6; i++) candidates[i] = (uint32_t)alphabet[4][i];
                        else if (key == 5) for (int i = 0; i < 6; i++) candidates[i] = (uint32_t)alphabet[5][i];
                        else if (key == 6) for (int i = 0; i < 6; i++) candidates[i] = (uint32_t)alphabet[6][i];
                        else if (key == 7) for (int i = 0; i < 8; i++) candidates[i] = (uint32_t)alphabet[7][i];
                        else if (key == 8) for (int i = 0; i < 6; i++) candidates[i] = (uint32_t)alphabet[8][i];
                        else if (key == 9) for (int i = 0; i < 8; i++) candidates[i] = (uint32_t)alphabet[9][i];
                        candidate_pages = candidate_paging(candidates, 8, 10, &candidate_page_num);
                        render_symbol_input(candidate_pages, current_page, candidate_page_num);

                        current_page = 0;
                        STATE = 4;
                    }
                    else if (ime_mode_flag == IME_MODE_NUMBER) {
                        input_buffer[input_counter++] = L'0' + key;
                        render_input_buffer(input_buffer, ime_mode_flag, 1);
                        STATE = 0;
                    }
                }

                // A键：清除输入缓冲区和屏幕
                else if (key == 10) {
                    input_buffer = refresh_input_buffer(input_buffer, &input_counter);

                    render_input_buffer(input_buffer, ime_mode_flag, 1);

                    STATE = 0;
                }

                // B键：转到设置
                else if (key == 11) {
                    OLED_SoftClear();
                    // render_text(L"Nano语言模型\nProject MARGA!\n \n(c) BD4SUR 2025-04");
                    render_text(L"选择语言模型：\n\n1. Nano-168M-QA\n2. Nano-56M-QA\n3. Nano-56M-Neko");
                    OLED_Refresh();

                    STATE = 5;
                }

                // C键：依次切换汉-英-数输入模式
                else if (key == 12) {
                    ime_mode_flag = (ime_mode_flag + 1) % 3;
                    render_input_buffer(input_buffer, ime_mode_flag, 1);
                    STATE = 0;
                }

                // D键：提交
                else if (key == 13) {
                    render_input_buffer(input_buffer, ime_mode_flag, 1);

                    STATE = 10;
                    goto STATE_10;
                }

                // *键：关于
                else if (key == 14) {
                    OLED_SoftClear();
                    render_text(L"Nano语言模型\nProject MARGA!\n \n(c) BD4SUR 2025-04");
                    OLED_Refresh();

                    STATE = 0;
                }

                break;

            /////////////////////////////////////////////
STATE_1:    // 拼音输入状态
            /////////////////////////////////////////////

            case 1:

                // D键：开始选字
                if (key == 13) {
                    render_pinyin_input(candidate_pages, pinyin_keys, current_page, candidate_page_num, 1);

                    // printf("  候选字列表（第%d页）：\n", current_page);
                    // for(int j = 0; j < 10; j++) {
                    //     uint32_t ch = candidate_pages[current_page][j];
                    //     printf("%lc, ", ch);
                    // }

                    STATE = 2;
                }

                // A键：取消输入拼音，清除已输入的所有按键，回到初始状态
                else if (key == 10) {
                    render_input_buffer(input_buffer, ime_mode_flag, 1);

                    current_page = 0;
                    pinyin_keys = 0;
                    STATE = 0;
                }

                // 2-9键：继续输入拼音
                else if (key >= 2 && key <= 9) {
                    pinyin_keys *= 10;
                    pinyin_keys += (uint32_t)key;

                    // printf("当前输入的数字：%d\n", pinyin_keys);

                    candidates = candidate_hanzi_list(pinyin_keys, &candidate_num);

                    if (candidates) { // 如果当前键码有对应的候选字
                        // 候选字列表分页
                        candidate_pages = candidate_paging(candidates, candidate_num, 10, &candidate_page_num);
                        render_pinyin_input(candidate_pages, pinyin_keys, current_page, candidate_page_num, 0);
                    }
                    else {
                        render_pinyin_input(NULL, pinyin_keys, 0, 0, 0);
                    }

                    STATE = 1;
                }

                break;

            /////////////////////////////////////////////
STATE_2:    // 候选字选择状态
            /////////////////////////////////////////////

            case 2:

                // 0-9键：从候选字列表中选定一个字，选定后转到初始状态
                if(key >= 0 && key <= 9) {
                    uint32_t index = (key == 0) ? 9 : (key - 1); // 按键0对应9
                    // 将选中的字加入输入缓冲区
                    uint32_t ch = candidate_pages[current_page][index];
                    if (ch) {
                        input_buffer[input_counter++] = ch;
                    }
                    else {
                        printf("选定了列表之外的字，忽略。\n");
                    }

                    render_input_buffer(input_buffer, ime_mode_flag, 1);

                    free(candidates);
                    free_candidate_pages(candidate_pages, candidate_page_num);
                    current_page = 0;

                    pinyin_keys = 0;
                    STATE = 0;
                }

                // *键：候选字翻页到上一页
                else if (key == 14) {
                    if(current_page > 0) {
                        current_page--;
                        render_pinyin_input(candidate_pages, pinyin_keys, current_page, candidate_page_num, 1);
                    }

                    STATE = 2;
                }

                // #键：候选字翻页到下一页
                else if (key == 15) {
                    if(current_page < candidate_page_num - 1) {
                        current_page++;
                        render_pinyin_input(candidate_pages, pinyin_keys, current_page, candidate_page_num, 1);
                    }

                    STATE = 2;
                }

                // A键：取消选择，回到初始状态
                else if (key == 10) {
                    render_input_buffer(input_buffer, ime_mode_flag, 1);

                    current_page = 0;
                    pinyin_keys = 0;
                    STATE = 0;
                }

                break;

            /////////////////////////////////////////////
STATE_3:    // 符号选择状态
            /////////////////////////////////////////////

            case 3:

                // 0-9键：从符号列表中选定一个符号，选定后转到初始状态
                if(key >= 0 && key <= 9) {
                    uint32_t index = (key == 0) ? 9 : (key - 1); // 按键0对应9
                    // 将选中的符号加入输入缓冲区
                    uint32_t ch = candidate_pages[current_page][index];
                    if (ch) {
                        input_buffer[input_counter++] = ch;
                    }
                    else {
                        printf("选定了列表之外的符号，忽略。\n");
                    }

                    render_input_buffer(input_buffer, ime_mode_flag, 1);

                    free(candidates);
                    free_candidate_pages(candidate_pages, candidate_page_num);
                    current_page = 0;

                    pinyin_keys = 0;
                    STATE = 0;
                }

                // *键：候选字翻页到上一页
                else if (key == 14) {
                    if(current_page > 0) {
                        current_page--;
                        render_symbol_input(candidate_pages, current_page, candidate_page_num);
                    }

                    STATE = 3;
                }

                // #键：候选字翻页到下一页
                else if (key == 15) {
                    if(current_page < candidate_page_num - 1) {
                        current_page++;
                        render_symbol_input(candidate_pages, current_page, candidate_page_num);
                    }

                    STATE = 3;
                }

                // A键：取消选择，回到初始状态
                else if (key == 10) {
                    render_input_buffer(input_buffer, ime_mode_flag, 1);

                    current_page = 0;
                    pinyin_keys = 0;
                    STATE = 0;
                }

                break;

            /////////////////////////////////////////////
STATE_4:    // 英文输入选择状态
            /////////////////////////////////////////////

            case 4:

                // 0-9键：从字母列表中选定一个字母，选定后转到初始状态
                if(key >= 0 && key <= 9) {
                    uint32_t index = (key == 0) ? 9 : (key - 1); // 按键0对应9
                    // 将选中的字母加入输入缓冲区
                    uint32_t ch = candidate_pages[current_page][index];
                    if (ch) {
                        input_buffer[input_counter++] = ch;
                    }
                    else {
                        printf("选定了列表之外的字母，忽略。\n");
                    }

                    render_input_buffer(input_buffer, ime_mode_flag, 1);

                    free(candidates);
                    free_candidate_pages(candidate_pages, candidate_page_num);
                    current_page = 0;

                    STATE = 0;
                }

                // A键：取消选择，回到初始状态
                else if (key == 10) {
                    render_input_buffer(input_buffer, ime_mode_flag, 1);

                    current_page = 0;
                    STATE = 0;
                }

                break;

            /////////////////////////////////////////////
STATE_5:    // 选择语言模型状态
            /////////////////////////////////////////////

            case 5:

                if (key == 1) {
                    unload_model();
                    OLED_SoftClear(); render_text(L" 正在加载语言模型\n Nano-168M-QA\n 请稍等..."); OLED_Refresh();
                    load_model(MODEL_PATH_1, NULL, repetition_penalty, temperature, top_p, top_k, random_seed);
                    OLED_SoftClear(); render_text(L"加载完成~"); OLED_Refresh();
                    usleep(1000*1000);
                    render_input_buffer(input_buffer, ime_mode_flag, 1);
                    current_page = 0;
                    STATE = 0;
                }

                else if (key == 2) {
                    unload_model();
                    OLED_SoftClear(); render_text(L" 正在加载语言模型\n Nano-56M-QA\n 请稍等..."); OLED_Refresh();
                    load_model(MODEL_PATH_2, NULL, repetition_penalty, temperature, top_p, top_k, random_seed);
                    OLED_SoftClear(); render_text(L"加载完成~"); OLED_Refresh();
                    usleep(1000*1000);
                    render_input_buffer(input_buffer, ime_mode_flag, 1);
                    current_page = 0;
                    STATE = 0;
                }

                else if (key == 3) {
                    unload_model();
                    OLED_SoftClear(); render_text(L" 正在加载语言模型\n Nano-56M-Neko\n 请稍等..."); OLED_Refresh();
                    load_model(MODEL_PATH_3, LORA_PATH_3, repetition_penalty, temperature, top_p, top_k, random_seed);
                    OLED_SoftClear(); render_text(L"加载完成~"); OLED_Refresh();
                    usleep(1000*1000);
                    render_input_buffer(input_buffer, ime_mode_flag, 1);
                    current_page = 0;
                    STATE = 0;
                }

                // A键：取消操作，回到初始状态
                else if (key == 10) {
                    OLED_SoftClear();
                    render_text(L"操作已取消");
                    OLED_Refresh();

                    usleep(1000*1000);

                    render_input_buffer(input_buffer, ime_mode_flag, 1);

                    current_page = 0;
                    STATE = 0;
                }

                break;

            /////////////////////////////////////////////
STATE_10:   // 提交候选字到LLM，开始推理
            /////////////////////////////////////////////

            case 10:

                // 从STATE_0转换过来，响应D键，开始推理
                if (key == 13) {
                    OLED_SoftClear();

                    wchar_t prompt[1024] = L"<|instruct_mark|>";
                    wcscat(prompt, input_buffer); // "<|instruct_mark|>西红柿炒鸡蛋怎么做？<|response_mark|>"
                    wcscat(prompt, L"<|response_mark|>");

                    generate(ctx, prompt, max_seq_len, on_prefilling, on_decoding, on_finished);

                    input_buffer = refresh_input_buffer(input_buffer, &input_counter);

                    STATE = 0;
                }

                break;

            default:
                break;
            }
        }
        prev_key = key;

    }

    unload_model();

    OLED_Close();
    return 0;
}
