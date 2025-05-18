# 
# Nano Language Model
#
#   BD4SUR 2024-10
#
#   Forked from:
#     - https://github.com/karpathy/llama2.c
#
# python export.py qwen3-0b6.bin --hf /path/to/Qwen3-0.6B
# python export.py qwen3-1b7.bin --hf /path/to/Qwen3-1.7B

import json
import struct
import argparse

import numpy as np
import torch
from torch import nn

from model import ModelArgs, Transformer


# -----------------------------------------------------------------------------
# common utilities

def serialize_fp32(file, tensor):
    """ writes one fp32 tensor to file that is open in wb mode """
    d = tensor.detach().cpu().view(-1).to(torch.float32).numpy()
    b = struct.pack(f'{len(d)}f', *d)
    file.write(b)

def serialize_base64(file, b64):
    """ writes one base64 bytestring to file that is open in wb mode """
    b = struct.pack(f'{len(b64)}B', *b64)
    file.write(b)

def serialize_int8(file, tensor):
    """ writes one int8 tensor to file that is open in wb mode """
    d = tensor.detach().cpu().view(-1).numpy().astype(np.int8)
    b = struct.pack(f'{len(d)}b', *d)
    file.write(b)

def quantize_q80(w, group_size):
    """
    takes a tensor and returns the Q8_0 quantized version
    i.e. symmetric quantization into int8, range [-127,127]
    """
    assert w.numel() % group_size == 0
    w = w.float() # convert to float32
    w = w.reshape(-1, group_size)
    # find the max in each group
    wmax = torch.abs(w).max(dim=1).values
    # calculate the scaling factor such that float = quant * scale
    scale = wmax / 127.0
    # scale into range [-127, 127]
    quant = w / scale[:,None]
    # round to nearest integer
    int8val = torch.round(quant).to(torch.int8)
    # dequantize by rescaling
    fp32val = (int8val.float() * scale[:,None]).view(-1)
    fp32valr = fp32val.reshape(-1, group_size)
    # calculate the max error in each group
    err = torch.abs(fp32valr - w).max(dim=1).values
    # find the max error across all groups
    maxerr = err.max().item()
    return int8val, scale, maxerr



#############################################

# this is a horrible gpt-2 unicode byte encoder hack from https://github.com/openai/gpt-2/blob/master/src/encoder.py#L9
# this has poisoned all HF tokenizer configs that use ByteLevel decoder/preprocessor
# as a result we get crazy UTF-8-as-bytes-as-UTF8 in the tokenizer data that we need to convert back
def gpt2_bytes_to_unicode():
    bs = list(range(ord("!"), ord("~")+1))+list(range(ord("¡"), ord("¬")+1))+list(range(ord("®"), ord("ÿ")+1))
    cs = bs[:]
    n = 0
    for b in range(2**8):
        if b not in bs:
            bs.append(b)
            cs.append(2**8+n)
            n += 1
    cs = [chr(n) for n in cs]
    return dict(zip(bs, cs))

def serialize_tokenizer(file):

    token_byte_length = 0

    with open("/home/bd4sur/ai/_model/Qwen3/Qwen3-0.6B/tokenizer.json", "r") as f:
        tokenizer = json.load(f)

    tokens = [""] * 151936
    scores = [0] * 151936

    vocab = tokenizer["model"]["vocab"]

    tokens_gpt2 = not tokenizer["model"].get("byte_fallback", False)

    for t, i in vocab.items():
        tokens[i] = t

    for added in tokenizer["added_tokens"]:
        tokens[added["id"]] = added["content"]

    # compute score as negative merge index so that earlier merges get selected first
    for i, m in enumerate(tokenizer["model"]["merges"]):
        t1, t2 = m[0], m[1] # Qwen3
        ti = vocab[t1 + t2]
        if scores[ti] == 0:
            scores[ti] = -(1 + i)

    # postprocess tokens
    gpt2_decode = {v: k for k, v in gpt2_bytes_to_unicode().items()}

    for i, t in enumerate(tokens):
        if tokens_gpt2:
            b = bytes([gpt2_decode.get(c, 0) for c in t])
        else:
            # t = t.replace('\u2581', ' ') # sentencepiece uses this character as whitespace
            b = t.encode('utf-8')

        b = b.replace(b"\0", b"\7") # replace null bytes with bell characters
        assert b.count(0) == 0 # no null bytes allowed

        tokens[i] = b
        token_byte_length += len(b)

    # record the max token length
    max_token_length = max(len(t) for t in tokens)

    # write to a binary file
    # the tokenizer.bin file is the same as .model file, but .bin

    tokenizer_field_bytes = 4 + 4 + 8 * min(len(tokens), len(scores)) + token_byte_length

    print(f"len(tokens) = {len(tokens)}")
    print(f"len(scores) = {len(scores)}")
    print(f"token_byte_length = {token_byte_length}")

    write_count = 0

    file.write(struct.pack('I', tokenizer_field_bytes))  # 模型文件中词表部分的字节数（不含本字段的4个字节）
    write_count += 4
    file.write(struct.pack("I", max_token_length))
    write_count += 4

    count = 0

    for byte, score in zip(tokens, scores):
        file.write(struct.pack("fI", score, len(byte)))
        write_count += 8
        if count < 100:
            print(f"[{count}] len(byte) = {len(byte)}")
        file.write(byte)
        write_count += len(byte)
        count += 1

    print(f"Write count = {write_count}")



def export_model(model, filepath):
    """
    Export the model weights in full float32 .bin file to be read from C.
    This is same as legacy_export, but with a proper header.
    """

    out_file = open(filepath, 'wb')

    #########################################################
    # 写入文件头（固定长度256B）

    print("Writing header...")

    major_version = 2025
    minor_version = 5

    # 1) write magic, which will be two uint32 of "BD4SURLM" in ASCII
    out_file.write(struct.pack('I', 0x42443453))
    out_file.write(struct.pack('I', 0x55524c4d))
    # --> 8 bytes

    # 2) write version, which will be int
    out_file.write(struct.pack('i', major_version))
    out_file.write(struct.pack('i', minor_version))
    # --> 16 bytes

    # 3) write file type TODO to be defined
    out_file.write(struct.pack('i', 3))  # Model type: Qwen3
    out_file.write(struct.pack('i', 36)) # Config Length: 32 bytes
    # --> 24 bytes

    # 4) write the model config, which will be 8 ints (32 bytes)
    p = model.params

    is_shared_classifier = torch.equal(model.tok_embeddings.weight, model.output.weight)
    header = struct.pack(
        "iiiiiiiii",
        p.max_seq_len,
        p.vocab_size,
        p.n_layers,
        p.dim,
        p.n_heads,
        p.n_heads if p.n_kv_heads is None else p.n_kv_heads,
        model.layers[0].feed_forward.w1.weight.shape[0],
        int(is_shared_classifier),
        p.head_dim
    )
    out_file.write(header)
    # --> 60 bytes

    # 5) write some other flags (TODO)

    # 6) pad rest with zeros; 'tell' returns current pos
    pad = 256 - out_file.tell()
    assert pad >= 0
    out_file.write(b'\0' * pad)


    #########################################################
    # 写入词表

    print("Writing tokenizer...")
    serialize_tokenizer(out_file)


    #########################################################
    # 写入模型参数

    print("Writing model parameters...")

    weights = [
        model.tok_embeddings.weight,
        *[layer.attention_norm.weight for layer in model.layers],
        *[layer.attention.wq.weight for layer in model.layers],
        *[layer.attention.wk.weight for layer in model.layers],
        *[layer.attention.wv.weight for layer in model.layers],
        *[layer.attention.wo.weight for layer in model.layers],

        # *[layer.attention.wq.bias for layer in model.layers], # Qwen2
        # *[layer.attention.wk.bias for layer in model.layers], # Qwen2
        # *[layer.attention.wv.bias for layer in model.layers], # Qwen2

        *[layer.attention.q_norm.weight for layer in model.layers], # Qwen3
        *[layer.attention.k_norm.weight for layer in model.layers], # Qwen3

        *[layer.ffn_norm.weight for layer in model.layers],
        *[layer.feed_forward.w1.weight for layer in model.layers],
        *[layer.feed_forward.w2.weight for layer in model.layers],
        *[layer.feed_forward.w3.weight for layer in model.layers],
        model.norm.weight,
        model.freqs_cos,
        model.freqs_sin,
    ]
    if not is_shared_classifier:
        weights.append(model.output.weight)

    param_count = 0
    for w in weights:
        param_count += w.detach().cpu().view(-1).numel()

    # 【NOTE 不需要】写入模型参数数（本字段8个字节）
    # out_file.write(struct.pack('Q', param_count)) # unsigned long long - uint64_t

    # 按照上面定义的维度顺序，将模型参数写入文件，没有其他定界符或填充数据
    for w in weights:
        serialize_fp32(out_file, w)

    print(f"Params = {param_count}")
    print(f"Total bin file length = {out_file.tell()}")

    #########################################################
    # 写入并关闭文件

    out_file.close()
    print(f"wrote {filepath}")



def export_quantized(model, tokenizer_config, filepath, group_size=64):
    """
    Export the model weights in Q8_0 into .bin file to be read from C.
    That is:
    - quantize all weights to symmetric int8, in range [-127, 127]
    - all other tensors (the rmsnorm params) are kept and exported in fp32
    - quantization is done in groups of group_size to reduce the effects of any outliers
    """

    cfg = model.config
    out_file = open(filepath, 'wb')

    #########################################################
    # 写入文件头（固定长度256B）

    print("Writing header...")

    major_version = 2024
    minor_version = 10

    # 1) write magic, which will be two uint32 of "BD4SURLM" in ASCII
    out_file.write(struct.pack('I', 0x42443453))
    out_file.write(struct.pack('I', 0x55524c4d))
    # --> 8 bytes

    # 2) write version, which will be int
    out_file.write(struct.pack('i', major_version))
    out_file.write(struct.pack('i', minor_version))
    # --> 16 bytes

    # 3) write file type TODO to be defined
    out_file.write(struct.pack('i', 0))  # Model type: Base model
    out_file.write(struct.pack('i', 32)) # Config Length: 32 bytes
    # --> 24 bytes

    # 4) write the model config, which will be 8 ints (32 bytes)
    cfg = model.config
    is_shared_classifier = torch.equal(model.tok_embeddings.weight, model.output.weight)
    header = struct.pack(
        "iiiiiiii",
        cfg.block_size,
        cfg.vocab_size,
        cfg.n_layer,
        cfg.n_embd,
        cfg.n_head,
        cfg.n_kv_head if cfg.n_kv_head is not None else cfg.n_head,
        cfg.n_hidden if cfg.n_hidden is not None else model.layers[0].feed_forward.w1.weight.shape[0],
        int(is_shared_classifier)
    )
    out_file.write(header)
    # --> 56 bytes

    # 5) write some other flags
    out_file.write(struct.pack('i', 800))        # 量化类型 TODO 待定义
    out_file.write(struct.pack('i', group_size)) # 量化参数(分组长度)

    # 6) pad rest with zeros; 'tell' returns current pos
    pad = 256 - out_file.tell()
    assert pad >= 0
    out_file.write(b'\0' * pad)


    #########################################################
    # 写入词表

    print("Writing tokenizer...")
    serialize_tokenizer(out_file, tokenizer_config)


    #########################################################
    # 校验量化参数

    while cfg.n_embd % group_size != 0:
        group_size //= 2
        print(f"BACKOFF: reducing group size to {group_size} to fit hidden_dim")

    weights = [
        model.tok_embeddings.weight,
        *[layer.attention.wq.weight for layer in model.layers],
        *[layer.attention.wk.weight for layer in model.layers],
        *[layer.attention.wv.weight for layer in model.layers],
        *[layer.attention.wo.weight for layer in model.layers],
        *[layer.feed_forward.w1.weight for layer in model.layers],
        *[layer.feed_forward.w2.weight for layer in model.layers],
        *[layer.feed_forward.w3.weight for layer in model.layers],
    ]
    shared_classifier = torch.equal(model.tok_embeddings.weight, model.output.weight)
    if not shared_classifier:
        weights.append(model.output.weight)
    for w in weights:
        assert w.numel() % group_size == 0, f"weight {i} has numel {w.numel()}, not a multiple of group_size {group_size}"


    #########################################################
    # 量化并写入模型参数
    # NOTE 注意：与非量化的参数排列顺序不同！

    print("Quantizing and writing model parameters...")

    # first let's write out all the params that we are keeping in fp32: the norms
    for layer in model.layers: # attention norms
        serialize_fp32(out_file, layer.attention_norm.weight)
    for layer in model.layers: # MLP norms
        serialize_fp32(out_file, layer.ffn_norm.weight)
    serialize_fp32(out_file, model.norm.weight) # final pre-classifier norm

    # now let's write out all the params that we are quantizing to Q8_0
    # note we skip classifier weights, which are shared with the embedding
    ew = []
    for i, w in enumerate(weights):
        q, s, err = quantize_q80(w, group_size)
        serialize_int8(out_file, q) # save the tensor in int8
        serialize_fp32(out_file, s) # save scale factors
        ew.append((err, w.shape))
        print(f"{i+1}/{len(weights)} quantized {tuple(w.shape)} to Q8_0 with max error {err}")

    # print the highest error across all weights, should be very small, e.g. O(~0.001)
    ew.sort(reverse=True)
    print(f"max quantization group error across all weights: {ew[0][0]}")

    # 最后写入RoPE参数
    serialize_fp32(out_file, model.freqs_cos)
    serialize_fp32(out_file, model.freqs_sin)


    #########################################################
    # 写入并关闭文件

    out_file.close()
    print(f"wrote {filepath}")


# -----------------------------------------------------------------------------
# Load / import functions


def load_hf_model(model_path):

    try:
        from transformers import AutoModelForCausalLM
    except ImportError:
        print("Error: transformers package is required to load huggingface models")
        print("Please run `pip install transformers` to install it")
        return None

    # load HF model
    hf_model = AutoModelForCausalLM.from_pretrained(model_path)
    hf_dict = hf_model.state_dict()

    # convert LlamaConfig to ModelArgs
    config = ModelArgs()
    config.dim = hf_model.config.hidden_size
    config.n_layers = hf_model.config.num_hidden_layers
    config.n_heads = hf_model.config.num_attention_heads
    config.n_kv_heads = hf_model.config.num_key_value_heads
    config.head_dim = hf_model.config.head_dim
    config.vocab_size = hf_model.config.vocab_size
    config.hidden_dim = hf_model.config.intermediate_size
    config.norm_eps = hf_model.config.rms_norm_eps
    config.max_seq_len = hf_model.config.max_position_embeddings

    # create a new Transformer object and set weights
    model = Transformer(config)

    model.tok_embeddings.weight = nn.Parameter(hf_dict['model.embed_tokens.weight'])
    model.norm.weight = nn.Parameter(hf_dict['model.norm.weight'])

    head_dim = 128 # config.dim // config.n_heads # Qwen3

    # huggingface permutes WQ and WK, this function reverses it
    # see https://github.com/huggingface/transformers/blob/b132c1703eb1c8bd9dfa4ad6a9be2bfd6ef819e9/src/transformers/models/llama/convert_llama_weights_to_hf.py#L122
    def permute_reverse(w, heads, rotary_dim):
        head_dim = 128 # w.shape[0] // heads # Qwen3
        assert rotary_dim <= head_dim
        w = torch.unflatten(w, 0, (-1, head_dim))
        # wr is the rotary part, wk is the part kept unrotated
        wr = w[:, :rotary_dim]
        wk = w[:, rotary_dim:]
        # switch wr from outputting two rotary_dim/2 chunks to outputting values interleaved
        wr = torch.unflatten(wr, 1, (2, -1))
        wr = wr.transpose(1, 2)
        wr = wr.flatten(1, 2)
        # assemble the heads back
        w = torch.cat([wr, wk], dim=1)
        return torch.flatten(w, 0, 1)

    for layer in model.layers:
        i = layer.layer_id
        layer.attention_norm.weight = nn.Parameter(hf_dict[f'model.layers.{i}.input_layernorm.weight'])
        print(f"Layer {i} attention_norm.shape = {layer.attention_norm.weight.shape}")

        # layer.attention.wq.weight = nn.Parameter(permute_reverse(hf_dict[f'model.layers.{i}.self_attn.q_proj.weight'], config.n_heads, head_dim))
        # layer.attention.wk.weight = nn.Parameter(permute_reverse(hf_dict[f'model.layers.{i}.self_attn.k_proj.weight'], config.n_kv_heads, head_dim))
        layer.attention.wq.weight = nn.Parameter(hf_dict[f'model.layers.{i}.self_attn.q_proj.weight'])
        layer.attention.wk.weight = nn.Parameter(hf_dict[f'model.layers.{i}.self_attn.k_proj.weight'])
        layer.attention.wv.weight = nn.Parameter(hf_dict[f'model.layers.{i}.self_attn.v_proj.weight'])
        layer.attention.wo.weight = nn.Parameter(hf_dict[f'model.layers.{i}.self_attn.o_proj.weight'])
        print(f"Layer {i} wq.shape = {layer.attention.wq.weight.shape}")
        print(f"Layer {i} wk.shape = {layer.attention.wk.weight.shape}")
        print(f"Layer {i} wv.shape = {layer.attention.wv.weight.shape}")
        print(f"Layer {i} wo.shape = {layer.attention.wo.weight.shape}")

        # Qwen2
        # layer.attention.wq.bias = nn.Parameter(permute_reverse(hf_dict[f'model.layers.{i}.self_attn.q_proj.bias'], config.n_heads, head_dim))
        # layer.attention.wk.bias = nn.Parameter(permute_reverse(hf_dict[f'model.layers.{i}.self_attn.k_proj.bias'], config.n_kv_heads, head_dim))
        # layer.attention.wv.bias = nn.Parameter(hf_dict[f'model.layers.{i}.self_attn.v_proj.bias'])

        # Qwen3
        layer.attention.q_norm.weight = nn.Parameter(hf_dict[f'model.layers.{i}.self_attn.q_norm.weight'])
        layer.attention.k_norm.weight = nn.Parameter(hf_dict[f'model.layers.{i}.self_attn.k_norm.weight'])
        print(f"Layer {i} q_norm.shape = {layer.attention.q_norm.weight.shape}")
        print(f"Layer {i} k_norm.shape = {layer.attention.k_norm.weight.shape}")

        layer.ffn_norm.weight = nn.Parameter(hf_dict[f'model.layers.{i}.post_attention_layernorm.weight'])
        layer.feed_forward.w1.weight = nn.Parameter(hf_dict[f'model.layers.{i}.mlp.gate_proj.weight'])
        layer.feed_forward.w2.weight = nn.Parameter(hf_dict[f'model.layers.{i}.mlp.down_proj.weight'])
        layer.feed_forward.w3.weight = nn.Parameter(hf_dict[f'model.layers.{i}.mlp.up_proj.weight'])
        print(f"Layer {i} ffn_norm.shape = {layer.ffn_norm.weight.shape}")
        print(f"Layer {i} w1.shape = {layer.feed_forward.w1.weight.shape}")
        print(f"Layer {i} w2.shape = {layer.feed_forward.w2.weight.shape}")
        print(f"Layer {i} w3.shape = {layer.feed_forward.w3.weight.shape}")

    # final classifier
    model.output.weight = nn.Parameter(hf_dict['lm_head.weight'])
    print(f"output.shape = {model.output.weight.shape}")
    model.eval()
    return model


def load_lora(lora_path):
    print(f"LoRA module file path: {lora_path}")
    checkpoint_dict = torch.load(lora_path, map_location='cpu')
    if checkpoint_dict["is_lora"]:
        train_config = checkpoint_dict["train_config"]
        model_config = checkpoint_dict["model_config"]
        lora_config = {
            "lora_rank": train_config.lora_rank,
            "lora_alpha": train_config.lora_alpha,
        }
        return checkpoint_dict["lora"], lora_config, model_config
    else:
        return False


# -----------------------------------------------------------------------------
# CLI entrypoint

if __name__ == "__main__":

    parser = argparse.ArgumentParser()
    parser.add_argument("filepath", type=str, help="the output filepath")
    parser.add_argument("--version", default=1, type=int, help="the version to export with")
    parser.add_argument("--dtype", type=str, help="dtype of the model (fp16, fp32)", default="fp32")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--checkpoint", type=str, help="model checkpoint, .pt file")
    group.add_argument("--hf", type=str, help="HuggingFace model checkpoint")
    group.add_argument("--quant", type=str, help="model checkpoint, .pt file for exporting quantized model bin file")
    group.add_argument("--lora", type=str, help="lora module, .pt file")
    args = parser.parse_args()
    dtype = {"fp16": torch.float16, "fp32": torch.float32}[args.dtype]

    if args.hf:
        model = load_hf_model(args.hf)
        export_model(model, args.filepath)
