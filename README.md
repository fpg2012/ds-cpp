# ds-cpp

A simple deepseek client based on Dear ImGui.

![](img/ui.png)

## Usage

```
Usage: ds-cpp [--help] [--version] [--config VAR] [--api-key VAR] [--disable-backspace VAR] [--wayland VAR]

ds-cpp (3b573825)
https://github.com/fpg2012/ds-cpp

Optional arguments:
  -h, --help           shows help message and exits 
  -v, --version        prints version information and exits 
  --config             path to config.json [nargs=0..1] [default: "config.json"]
  --api-key            [nargs=0..1] [default: "no_api_key"]
  --disable-backspace  [nargs=0..1] [default: false]
  --wayland            [nargs=0..1] [default: false]
```

Example of `config.json`

```
{
    "api-key": "sk-xxxxxxxxxx"
}
```

Chat histories will be stored in a zstd compressed json file, inside `chat_history` folder if there exists one.

## Build

### Clone this repository

```
git clone https://github.com/fpg2012/ds-cpp
```

### Download Dependencies

Download source code tarballs of the following dependencies, and put them in `thirdparty` directory.

1. [argparse v3.2](https://github.com/p-ranav/argparse/releases/tag/v3.2)
2. [Dear ImGui v1.91.8](https://github.com/ocornut/imgui/archive/refs/tags/v1.91.8.tar.gz)
3. [cpr v1.11.2](https://github.com/libcpr/cpr/releases/tag/1.11.2)
4. [glfw v3.4](https://github.com/glfw/glfw/releases/tag/3.4)
5. [nlohmann_json v3.11.3](https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.tar.gz)
6. [ImGuiFileDialog v0.6.7](https://github.com/aiekick/ImGuiFileDialog/archive/refs/tags/v0.6.7.tar.gz)
7. [stb_image.h](https://github.com/nothings/stb/blob/master/stb_image.h)

```
thirdparty
├── argparse
├── cpr
├── glfw
├── ImGuiFileDialog
├── nlohmann_json
├── imgui
└── stb/stb_image.h
```

### Compile

#### Linux:

如果要用输入法，建议给glfw打个补丁。（否则在打字时，退格不仅会删除输入法中“预编辑”的内容，也会删除输入框里面已经输入好的内容）。
It is recommended to patch glfw if you need to use an input method.

```
cd ds-cpp/thirdparty/glfw # cd into thirdparty/glfw
patch -p1 < ../../glfw_x11_ime.patch
```

`cd` to the repository directory, and compile everything.

```
mkdir build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
```

#### Windows:

Use Visual Studio to open the directory, after cmake configuration, build the solution.
