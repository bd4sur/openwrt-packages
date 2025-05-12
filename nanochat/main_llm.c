#include "infer.h"

#define OUTPUT_BUFFER_LENGTH (512)

static Nano_Context *g_llm_ctx;

static char *MODEL_PATH_1 = "/emmc/_model/nano_168m_625000_sft_947000.bin";

Nano_Context *load_model(char *model_path, char *lora_path, float repetition_penalty, float temperature, float top_p, unsigned int top_k, unsigned long long rng_seed) {
    Nano_Context *ctx = (Nano_Context*)calloc(1, sizeof(Nano_Context));
    ctx->random_seed = rng_seed;
    ctx->llm = (LLM *)calloc(1, (sizeof(LLM)));
    ctx->tokenizer = (Tokenizer *)calloc(1, (sizeof(Tokenizer)));
    load_llm(ctx->llm, ctx->tokenizer, model_path);
    ctx->sampler = build_sampler(ctx->llm->config.vocab_size, repetition_penalty, temperature, top_p, top_k, ctx->random_seed);
    ctx->lora = (lora_path) ? load_lora(ctx->llm, lora_path) : NULL;
    return ctx;
}

void unload_model(Nano_Context *ctx) {
    free_llm(ctx->llm, ctx->tokenizer);
    free_sampler(ctx->sampler);
}

int32_t on_prefilling(Nano_Session *session) {
    // printf("Pre-filling...\n");
    return LLM_RUNNING_IN_PREFILLING;
}

int32_t on_decoding(Nano_Session *session) {
    uint32_t output_length = wcslen(session->output_text);
    printf("%lc", session->output_text[output_length - 1]);
    fflush(stdout);
    return LLM_RUNNING_IN_DECODING;
}

int32_t on_finished(Nano_Session *session) {
    printf("TPS = %f\n", session->tps);
    return LLM_STOPPED_NORMALLY;
}


int main() {
    if(!setlocale(LC_CTYPE, "")) return -1;

    float repetition_penalty = 1.1f;
    float temperature = 1.0f;
    float top_p = 0.5f;
    unsigned int top_k = 0;
    unsigned long long random_seed = (unsigned int)time(NULL);
    uint32_t max_seq_len = 512;

    g_llm_ctx = load_model(MODEL_PATH_1, NULL, repetition_penalty, temperature, top_p, top_k, random_seed);

    wchar_t *prompt = apply_chat_template(NULL, NULL, L"西红柿炒鸡蛋怎么做？");

    printf("%ls\n", prompt);

    generate_sync(g_llm_ctx, prompt, max_seq_len, on_prefilling, on_decoding, on_finished);

    unload_model(g_llm_ctx);

#ifdef MATMUL_PTHREAD
    matmul_pthread_cleanup();
#endif

    return 0;
}
