# gguf-tools

[中文](README.md)

GGUF model file viewer, comparator, and inspector. Written in C with zero dependencies.

## Install

[Latest Release](https://github.com/antnesswcm/gguf-tools/releases)

Or build from source:

```console
$ git clone <repo-url> && cd gguf-tools
$ make
```

On Windows with MSVC:

```console
> .\build-msvc.ps1 build
```

Output: `build\x64\release\gguf-tools.exe`.

## Usage

```
Usage: gguf-tools <subcommand> [arguments...] [options...]
Subcommands:
  show <filename>                                   -- show GGUF model keys and tensors.
  inspect-tensor <filename> <tensor-name> [count]   -- show tensor weights.
  compare <file1> <file2>                           -- avg weights diff for matching tensor names.
  split-mixtral <ids...> mixtral.gguf out.gguf      -- extract expert.
Options:
  --verbose       :With 'show', print full arrays (e.g. token lists)
  --diffable      :Don't show tensor file offsets and sizes
Example:
  split-mixtral 65230776370407150546470161412165 mixtral.gguf out.gguf
```

## Examples

### show

```console
$ gguf-tools show phi-2.Q8_0.gguf
phi-2.Q8_0.gguf (ver 3): 20 key-value pairs, 325 tensors
general.architecture: [string] phi2
general.name: [string] Phi2
phi2.context_length: [uint32] 2048
phi2.embedding_length: [uint32] 2560
phi2.feed_forward_length: [uint32] 10240
phi2.block_count: [uint32] 32
phi2.attention.head_count: [uint32] 32
phi2.attention.head_count_kv: [uint32] 32
phi2.attention.layer_norm_epsilon: [float32] 0.000010
phi2.rope.dimension_count: [uint32] 32
general.file_type: [uint32] 7
tokenizer.ggml.add_bos_token: [bool] false
tokenizer.ggml.model: [string] gpt2
tokenizer.ggml.tokens: [array] [!, ", #, $, %, &, ', ...]

... many more key-value pairs ...

q8_0 tensor token_embd.weight @1806176, 131072000 weights, dims [2560,51200], 139264000 bytes
f32 tensor blk.0.attn_norm.bias @141070176, 2560 weights, dims [2560], 10240 bytes
f32 tensor blk.0.attn_norm.weight @141080416, 2560 weights, dims [2560], 10240 bytes
q8_0 tensor blk.0.attn_qkv.weight @141121376, 19660800 weights, dims [2560,7680], 20889600 bytes

... many more tensors ...
```

### compare

```console
$ gguf-tools compare mistral-7b-instruct-v0.2.Q8_0.gguf \
                     solar-10.7b-instruct-v1.0-uncensored.Q8_0.gguf
[token_embd.weight]: avg weights difference: 44.539944%
[blk.0.attn_q.weight]: avg weights difference: 48.717736%
[blk.0.attn_k.weight]: avg weights difference: 56.201885%
[blk.0.attn_v.weight]: avg weights difference: 47.087249%
[blk.0.ffn_gate.weight]: avg weights difference: 37.508761%
[blk.0.ffn_up.weight]: avg weights difference: 39.061584%
[blk.0.ffn_down.weight]: avg weights difference: 39.632648%
```

Computes average weight difference for tensors with matching names and parameter counts. Lower values indicate the two models share that layer more closely — useful for detecting finetune lineage, frozen layers, and modification magnitude.

### inspect-tensor

```console
$ gguf-tools inspect-tensor phi-2.Q8_0.gguf token_embd.weight 8
0.001234, -0.003456, 0.007891, -0.002345,
-0.005678, 0.001234, -0.008901, 0.003456
```

### split-mixtral

```console
$ gguf-tools split-mixtral 65230776370407150546470161412165 mixtral.gguf out.gguf
```

The 32-digit string maps to 32 layers; each digit (0-7) selects the expert to extract for that layer. Models produced this way cannot run inference — this is a library usage demo only.