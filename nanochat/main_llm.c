#include "infer.h"

#define OUTPUT_BUFFER_LENGTH (512)

static Nano_Context ctx;

static char *MODEL_PATH_1 = "/emmc/_model/nano_168m_625000_sft_947000.bin";

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
    // printf("Pre-filling...\n");
    return 0;
}

uint32_t on_decoding(wchar_t *output, uint32_t pos, float tps) {
    uint32_t output_length = wcslen(output);
    printf("%lc", output[output_length - 1]);
    fflush(stdout);
    return 0;
}

uint32_t on_finished(wchar_t *output, uint32_t pos, float tps) {
    printf("TPS = %f\n", tps);
    return 0;
}


int main() {
    if(!setlocale(LC_CTYPE, "")) return -1;

    float repetition_penalty = 1.1f;
    float temperature = 1.0f;
    float top_p = 0.5f;
    unsigned int top_k = 0;
    unsigned long long random_seed = (unsigned int)time(NULL);
    uint32_t max_seq_len = 512;

    load_model(MODEL_PATH_1, NULL, repetition_penalty, temperature, top_p, top_k, random_seed);

    wchar_t prompt[1024] = L"<|instruct_mark|>";
    wcscat(prompt, L"西红柿炒鸡蛋怎么做？");
    wcscat(prompt, L"<|response_mark|>");

    int32_t flag = generate(ctx, prompt, max_seq_len, on_prefilling, on_decoding, on_finished);

    unload_model();

#ifdef MATMUL_PTHREAD
    global_cleanup();
#endif

    return 0;
}
