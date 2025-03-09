#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <ios>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <ctime>
#include <random>
#include <fstream>
#include <sstream>

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <zlib.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_freetype.h>
#include <GLFW/glfw3.h>

#include <ImGuiFileDialog.h>

#include <argparse/argparse.hpp>

#include "version.h"
#include "ttf/fonts.h"

#ifdef _WIN32
#include <windows.h>
#endif

bool disable_backspace = false;

std::string get_today_date() {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d");
    return oss.str();
}

std::string get_random_number() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1, 100);
    std::ostringstream oss;
    oss << std::setw(3) << std::setfill('0') << dis(gen); 
    return oss.str();
}

using json = nlohmann::json;

// 全局变量
#ifndef _WIN32
std::string file_prefix = "chat_history/";
#else
std::string file_prefix = "chat_history\\";
#endif

std::string file_ext = ".json.zstd";
std::future<std::string> filename_future;
std::future<void> save_future;

std::vector<std::string> messages;
std::mutex messages_mutex;
std::condition_variable cv;
bool new_message = false;

// 对话历史记录
std::vector<json> chat_history;

// 文件名
bool filename_generated = false;
std::string filename;

struct AppConfig {
    std::string api_key;
    std::string model;
    std::string system_prompt;
    std::string save_path;

    static AppConfig get_default();
};

AppConfig AppConfig::get_default() {
    return AppConfig{
        "no_api_key",
        "deepseek-chat",
        "You are a helpful assistant.",
        "chat_history"
    };
}

// 从配置文件读取 API 密钥
// std::string read_api_key(const std::string& config_file) {
//     std::ifstream file(config_file);
//     if (!file.is_open()) {
//         std::cerr << "Failed to open config file: " << config_file << std::endl;
//         return "";
//     }
//     std::string api_key;
//     std::getline(file, api_key);
//     return api_key;
// }

AppConfig read_config(const std::string& config_file, AppConfig& config) {
    std::ifstream file(config_file, std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file: " << config_file << std::endl;
        return config;
    }
    std::size_t file_size = file.tellg();
    file.seekg(0, std::ios_base::beg);

    std::string content;
    content.resize(file_size + 10);
    file.read(content.data(), file_size);

    json config_json = json::parse(content);

    if (config_json.contains("api-key"))
        config.api_key = config_json["api-key"];
    if (config_json.contains("model"))
        config.model = config_json["model"];
    if (config_json.contains("system-prompt"))
        config.system_prompt = config_json["system-prompt"];
    if (config_json.contains("save_path"))
        config.save_path = config_json["save_path"];

    return config;
}

void my_key_call_back(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (disable_backspace && key == GLFW_KEY_BACKSPACE) {
        // 忽略退格键的事件，不做任何处理
        return;
    }
    // 继续处理其他键
    ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
}

// 移除文件名中的非法字符
std::string sanitize_filename(const std::string& filename) {
    std::string sanitized = filename;
    // 定义非法字符
    const std::string illegal_chars = "+?=<>:\"/\\|*"; // 根据需要添加其他非法字符
    // 移除非法字符
    sanitized.erase(std::remove_if(sanitized.begin(), sanitized.end(), [&](char c) {
        return illegal_chars.find(c) != std::string::npos;
    }), sanitized.end());
    return sanitized;
}

// 调用 DeepSeek API 总结文件名
std::string summarize_filename(const std::string& api_key, const std::string& user_message) {
    // 构造请求体
    json request_body = {
        {"model", "deepseek-chat"},
        {"messages", {
            {{"role", "system"}, {"content", "Summarize the following message into a short filename (less than 20 characters, no special characters):"}},
            {{"role", "user"}, {"content", user_message}}
        }},
        {"stream", false}
    };

    // 发送请求
    cpr::Response response = cpr::Post(
        cpr::Url{"https://api.deepseek.com/chat/completions"},
        cpr::Header{
            {"Authorization", "Bearer " + api_key},
            {"Content-Type", "application/json"}
        },
        cpr::Body{request_body.dump()}
    );

    // 处理响应
    if (response.status_code == 200) {
        json json_response = json::parse(response.text);
        std::string summary = json_response["choices"][0]["message"]["content"];
        return sanitize_filename(summary); // 移除非法字符
    } else {
        std::cerr << "Error summarizing filename: " << response.text << std::endl;
        return "chat_history"; // 默认文件名
    }
}

// 生成文件名
std::string generate_filename(const std::string& user_message, const std::string& api_key) {
    filename = summarize_filename(api_key, user_message);
    filename_generated = true;
    std::cout << "Generated filename: " << filename << std::endl; // 调试输出
    return filename;
}

// 异步生成文件名
void start_filename_generation(const std::string& api_key, const std::string& message) {
    filename_future = std::async(std::launch::async, [api_key, message]() {
        return generate_filename(message, api_key);
    });
}

// 检查文件名是否生成完成
std::string get_generated_filename() {
    if (filename_generated) {
        return filename;
    }
    if (filename_future.valid() && filename_future.wait_for(std::chrono::seconds(2)) == std::future_status::ready) {
        filename = filename_future.get();
        return filename_future.get();
    }
    return "untitled-" + get_today_date() + "-" + get_random_number();
}

// 解压缩 zlib 数据
std::string decompress_zlib(const std::string& compressed_data) {
    z_stream zs;
    memset(&zs, 0, sizeof(zs));

    if (inflateInit(&zs) != Z_OK) {
        throw std::runtime_error("Failed to initialize zlib");
    }

    zs.next_in = (Bytef*)compressed_data.data();
    zs.avail_in = compressed_data.size();

    int ret;
    char out_buffer[32768];
    std::string out_string;

    do {
        zs.next_out = reinterpret_cast<Bytef*>(out_buffer);
        zs.avail_out = sizeof(out_buffer);

        ret = inflate(&zs, 0);

        if (out_string.size() < zs.total_out) {
            out_string.append(out_buffer, zs.total_out - out_string.size());
        }
    } while (ret == Z_OK);

    inflateEnd(&zs);

    if (ret != Z_STREAM_END) {
        throw std::runtime_error("Failed to decompress data");
    }

    return out_string;
}

// 压缩 zlib 数据
std::string compress_zlib(const std::string& data) {
    z_stream zs;
    memset(&zs, 0, sizeof(zs));

    if (deflateInit(&zs, Z_BEST_COMPRESSION) != Z_OK) {
        throw std::runtime_error("Failed to initialize zlib");
    }

    zs.next_in = (Bytef*)data.data();
    zs.avail_in = data.size();

    int ret;
    char out_buffer[32768];
    std::string out_string;

    do {
        zs.next_out = reinterpret_cast<Bytef*>(out_buffer);
        zs.avail_out = sizeof(out_buffer);

        ret = deflate(&zs, Z_FINISH);

        if (out_string.size() < zs.total_out) {
            out_string.append(out_buffer, zs.total_out - out_string.size());
        }
    } while (ret == Z_OK);

    deflateEnd(&zs);

    if (ret != Z_STREAM_END) {
        throw std::runtime_error("Failed to compress data");
    }

    return out_string;
}

// 加载压缩的对话记录
void load_compressed_chat_history(const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << file_path << std::endl;
        return;
    }

    std::stringstream compressed_data;
    compressed_data << file.rdbuf();
    file.close();

    std::string json_data = decompress_zlib(compressed_data.str());

    try {
        json imported_history = json::parse(json_data);
        chat_history = imported_history.get<std::vector<json>>();

        messages.clear();
        for (const auto& entry : chat_history) {
            std::string role = entry["role"];
            std::string content = entry["content"];
            if (role == "user") {
                messages.push_back("You: " + content);
            } else if (role == "assistant") {
                messages.push_back("DeepSeek: " + content);
            }
        }

        filename_generated = true;
        filename = file_path;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing imported chat history: " << e.what() << std::endl;
    }
}

// 保存压缩的对话记录
void export_compressed_chat_history(const std::string& file_path) {
    std::string json_data = json(chat_history).dump();
    std::string compressed_data = compress_zlib(json_data);

    std::ofstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for export: " << file_path << std::endl;
        return;
    }
    file.write(compressed_data.data(), compressed_data.size());
    file.close();
}

// 异步保存聊天记录
void auto_save_chat_history() {
    if (filename_generated) {
        export_compressed_chat_history(file_prefix + filename + file_ext);
        std::cout << "Auto-saved to: " << file_prefix + filename + file_ext << std::endl;
    }
}

// 调用 DeepSeek API 进行聊天
void chat_with_deepseek(const AppConfig &config, const std::string& message) {
    // 添加用户消息到历史记录
    chat_history.push_back({{"role", "user"}, {"content", message}});

    // 如果是第一轮对话，生成文件名
    if (chat_history.size() == 2) { // 第一轮对话（system + user）
        start_filename_generation(config.api_key, message);
    }

    // 构造请求体
    json request_body = {
        {"model", config.model},
        {"messages", chat_history},
        {"stream", true} // 启用流式传输
    };

    // 发送请求
    cpr::Response response = cpr::Post(
        cpr::Url{"https://api.deepseek.com/chat/completions"},
        cpr::Header{
            {"Authorization", "Bearer " + config.api_key},
            {"Content-Type", "application/json"}
        },
        cpr::Body{request_body.dump()},
        cpr::WriteCallback{[&](const std::string_view& data, intptr_t) -> bool {
            std::istringstream stream((std::string(data)));
            std::string line;
            while (std::getline(stream, line)) {
                if (line.empty() || line == "data: [DONE]") continue;

                try {
                    json json_response = json::parse(line.substr(6));
                    std::string token = json_response["choices"][0]["delta"]["content"];
                    if (!token.empty()) {
                        std::lock_guard<std::mutex> lock(messages_mutex);
                        if (!messages.empty() && messages.back().find("DeepSeek: ") == 0) {
                            messages.back() += token;
                        } else {
                            messages.push_back("DeepSeek: " + token);
                        }
                        new_message = true;
                        cv.notify_one();
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error parsing JSON: " << e.what() << std::endl;
                }
            }
            return true;
        }}
    );

    if (response.status_code != 200) {
        std::lock_guard<std::mutex> lock(messages_mutex);
        messages.push_back("Error: " + response.text);
        new_message = true;
        cv.notify_one();
    } else {
        // 添加助手消息到历史记录
        chat_history.push_back({{"role", "assistant"}, {"content", messages.back().substr(10)}});
    }

    // 每轮对话后自动保存聊天记录
    auto_save_chat_history();
}

// 主函数
int main(int argc, char **argv) {
#ifdef _WIN32
    FreeConsole();
#endif
    AppConfig config = AppConfig::get_default();
    std::string config_path = "config.json";
    bool use_wayland = false;
    
    argparse::ArgumentParser program("ds-cpp");
    
    std::stringstream description;
    description << "ds-cpp (" << GIT_COMMIT_HASH << ")\nhttps://github.com/fpg2012/ds-cpp";

    program.add_description(description.str());
    program.add_argument("--config").help("path to config.json").default_value("config.json").store_into(config_path);
    program.add_argument("--api-key").default_value("no_api_key").store_into(config.api_key);
    program.add_argument("--disable-backspace").default_value(false).store_into(disable_backspace);
    program.add_argument("--wayland").default_value(false).store_into(use_wayland);
    try {
        program.parse_args(argc, argv);    // Example: ./main --color orange
    }
    catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        std::exit(1);
    }

    // AppConfig config = read_config(config_path);
    read_config(config_path, config);
    chat_history.push_back({{"role", "system"}, {"content", config.system_prompt}});
    file_prefix = config.save_path;

    // 初始化 GLFW 和 ImGui（省略部分代码）
    if (config.api_key.empty()) {
        std::cerr << "API key is empty. Please check your config file." << std::endl;
        // return -1;
        config.api_key = "no api key!!!";
    }
    config.api_key.resize(256);

#ifdef __linux__
    if (!use_wayland)
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
#endif

    // 初始化 GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    // 创建窗口
    GLFWwindow* window = glfwCreateWindow(600, 600, "DeepSeek Chat", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // 启用垂直同步

    // 初始化 Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    // 加载中文字体
    // you don't need to free ptr1 & ptr2, because their ownership are transferred to ImGui
    unsigned char *ptr1 = new unsigned char[fusion_pixel_12px_monospaced_zh_hant_ttf_len];
    memcpy(ptr1, fusion_pixel_12px_monospaced_zh_hant_ttf, fusion_pixel_12px_monospaced_zh_hant_ttf_len);
    io.Fonts->AddFontFromMemoryTTF(ptr1, fusion_pixel_12px_monospaced_zh_hant_ttf_len, 18.0f, nullptr, io.Fonts->GetGlyphRangesChineseFull());

    static ImWchar ranges[] = { 0x1, 0x1FFFF, 0 };
    static ImFontConfig cfg;
    cfg.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor;
    cfg.OversampleH = cfg.OversampleV = 1;
    cfg.MergeMode = true;
    // io.Fonts->AddFontFromFileTTF("NotoEmoji-Regular.ttf", 18.0f, &cfg, ranges);
    unsigned char *ptr2 = new unsigned char[NotoEmoji_Regular_ttf_len];
    memcpy(ptr2, NotoEmoji_Regular_ttf, NotoEmoji_Regular_ttf_len);
    io.Fonts->AddFontFromMemoryTTF(ptr2, NotoEmoji_Regular_ttf_len, 18.0f, &cfg, ranges);
    io.Fonts->Build();

    // 初始化 ImGui 后端
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    glfwSetKeyCallback(window, my_key_call_back);

    // 主循环
    std::string input_text;
    input_text.resize(1024 * 64); // 为输入框分配足够的空间
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // 开始新的 ImGui 帧
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 创建全屏 UI
        ImGui::SetNextWindowPos(ImVec2(0, 0)); // 窗口位置在左上角
        ImGui::SetNextWindowSize(io.DisplaySize); // 窗口大小设置为整个显示区域
        ImGui::Begin("DeepSeek Chat", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        
        if (ImGui::CollapsingHeader("API Key Configuration")) {
            ImGui::Text("API Key:");
            ImGui::PushItemWidth(ImGui::GetWindowWidth() - 10);
            if (ImGui::InputText("##API_KEY", config.api_key.data(), config.api_key.size(), ImGuiInputTextFlags_EnterReturnsTrue)) {
                config.api_key = config.api_key.substr(0, config.api_key.find('\0'));
            }
            ImGui::Checkbox("Disable Backspace", &disable_backspace);
            ImGui::PopItemWidth();
        }

        // 显示聊天记录
        ImGui::BeginChild("Scrolling", ImVec2(0, ImGui::GetFrameHeightWithSpacing() - 200)); // 聊天记录区域
        for (const auto& message : messages) {
            float wrap_width = ImGui::GetWindowWidth() - 10; // 留出一些边距
            ImGui::PushTextWrapPos(wrap_width);
            // 检查消息是否是用户的消息
            if (message.find("You: ") == 0) {
                // 设置文本颜色为黄色
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 0, 255));
            } else if (message.find("Error: ") == 0) {
                // 设置文本颜色为红色
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
            }
            ImGui::TextWrapped("%s", message.c_str());
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                ImGui::SetClipboardText(message.c_str());
            }
            if (message.find("You: ") == 0 || message.find("Error: ") == 0) {
                // 设置文本颜色为黄色
                ImGui::PopStyleColor();
            }
            ImGui::PopTextWrapPos();

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                // 双击选中文本
                ImGui::SetClipboardText(message.c_str());
            }
        }
        if (new_message) {
            ImGui::SetScrollHereY(1.0f);
            new_message = false;
        }
        ImGui::EndChild();

        // 输入框和发送按钮
        bool send_message = false;
        ImGui::PushItemWidth(ImGui::GetWindowWidth());
        if (ImGui::InputTextMultiline("##Input", input_text.data(), input_text.size(), ImVec2(-1, ImGui::GetTextLineHeight() * 4), ImGuiInputTextFlags_None)) {
            // 检测 Ctrl+回车
            if (io.KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))) {
                send_message = true;
            } else if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
                // 单独按下回车，插入换行符
                std::string current_input(input_text);
                size_t cursor_pos = current_input.find('\0');
                if (cursor_pos != std::string::npos) {
                    current_input.insert(cursor_pos, "\n");
                    input_text = current_input;
                    input_text.resize(1024 * 64); // 重置缓冲区大小
                }
            }
        }

        // 发送按钮
        if (ImGui::Button("Send", ImVec2(-1, 0)) || send_message) {
            std::string user_input(input_text.data()); // 截断到实际内容
            messages.push_back("You: " + user_input);
            std::thread(chat_with_deepseek, config, user_input).detach();
            input_text.clear();
            input_text.resize(1024 * 64); // 重置缓冲区大小
        }
        ImGui::PopItemWidth();

        // ImGui::PushItemWidth(ImGui::GetWindowWidth() / 2);
        // 导出和导入按钮
        if (ImGui::Button("Export", ImVec2(-1, 0))) {
            if (!filename.empty()) {
                export_compressed_chat_history(file_prefix + filename + file_ext);
            }
        }
        if (ImGui::Button("Import", ImVec2(-1, 0))) {
            IGFD::FileDialogConfig config;
            config.path = "."; // 默认路径
            ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlg", "Choose File", ".ztsd", config);
        }
        // ImGui::PopItemWidth();

        // 文件选择对话框
        if (ImGuiFileDialog::Instance()->Display("ChooseFileDlg")) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::string file_path = ImGuiFileDialog::Instance()->GetFilePathName();
                load_compressed_chat_history(file_path);
            }
            ImGuiFileDialog::Instance()->Close();
        }

        ImGui::End();

        // 渲染
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // 清理
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
