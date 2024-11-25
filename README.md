# The ✨AI✨ C Compiler (aicc)

Though `gcc` and `clang` are very usefull and advanced C compilers, they still have their limits. You forgot a `;`? Error! You forgot a `free()`? Memory leak! To solve this I propose a more advanced C compiler, that utilizes the power of modern AI to fix your problems, `aicc`. The usage is simple.
```bash
aicc [input]
```
`aicc` uses AI to compile your code, which shall be forwarded to ChatGPT and optimized. Will your code compile? Will it be fast or slow? Will it have memory leaks? Leave it all to AI! You just need to make it understand what you want.

[Demo video](https://cloud-pch5l26jf-hack-club-bot.vercel.app/02024-11-21_19-57-23.mp4)

## Building

Dependencies: LLVM, curl

```sh
$ git clone --depth 1 https://github.com/cheyao/aicc
$ cd aicc
$ mkdir build && cd build
$ cmake -DOPENAI_API_KEY=<your_api_key> ..
$ cmake --build .
$ sudo cmake --install .
```

Remember to replace *your_api_key* with the API key you've created.

## Building with Nix

To build, get your OpenAI API key and run a nix build like this:
```sh
$ OPENAI_API_KEY=your_api_key nix build --impure
```
Remember to replace *your_api_key* with the API key you've created.
