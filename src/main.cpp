#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cctype>

namespace CampusSim {

    // ----------------- 数据结构 -----------------

    struct Stats {
        int physique        = 0;  // 体质
        int study           = 0;  // 学力
        int network         = 0;  // 人脉
        int reputation      = 0;  // 名誉
        int experience      = 0;  // 经验
        int publicPractice  = 0;  // 公能实践
        int volunteer       = 0;  // 志愿服务
        int socialPractice  = 0;  // 社会实践
    };

    struct GameState {
        Stats stats;
        std::map<std::string, bool> flags;  // 记录关键历史选择
    };

    struct Choice {
        std::string text;                   // 选项文字（UTF-8）
        int dPhysique       = 0;            // 体质 变化
        int dStudy          = 0;            // 学力 变化
        int dNetwork        = 0;            // 人脉 变化
        int dReputation     = 0;            // 名誉 变化
        int dExperience     = 0;            // 经验 变化
        int dPublicPractice = 0;            // 公能实践 变化
        int dVolunteer      = 0;            // 志愿服务 变化
        int dSocialPractice = 0;            // 社会实践 变化
        std::string nextSceneId;            // 下一个场景 ID
        std::vector<std::string> setFlags;  // 选了这个选项要打的 flag
    };

    struct Scene {
        std::string id;
        std::string backgroundPath;
        std::string dialogue;               // 剧情文本（UTF-8，可多行）
        std::vector<Choice> choices;
    };

    // ----------------- 工具函数 -----------------

    std::string trim(const std::string& s) {
        std::size_t start = 0;
        while (start < s.size() &&
               std::isspace(static_cast<unsigned char>(s[start]))) {
            ++start;
        }
        std::size_t end = s.size();
        while (end > start &&
               std::isspace(static_cast<unsigned char>(s[end - 1]))) {
            --end;
        }
        return s.substr(start, end - start);
    }

    bool startsWith(const std::string& s, const std::string& prefix) {
        return s.rfind(prefix, 0) == 0;
    }

    std::vector<std::string> split(const std::string& s, char delim) {
        std::vector<std::string> result;
        std::string current;
        std::istringstream iss(s);
        while (std::getline(iss, current, delim)) {
            result.push_back(trim(current));
        }
        return result;
    }

    // DELTA 字段：例如 "energy=-1,mood=+2,grade=0" 或 "精力=-1,心情=+2,成绩=0"
    void parseDelta(const std::string& s, Choice& choice) {
        if (s.empty()) return;
        auto items = split(s, ',');
        for (auto& item : items) {
            if (item.empty()) continue;
            auto kv = split(item, '=');
            if (kv.size() != 2) continue;
            std::string key = kv[0];
            int value = 0;
            try {
                value = std::stoi(kv[1]);
            } catch (...) {
                continue;
            }

            // 支持中文字段名和简单英文缩写
            if (key == "physique" || key == "体质" || key == "P") {
                choice.dPhysique += value;
            } else if (key == "study" || key == "学力" || key == "X") {
                choice.dStudy += value;
            } else if (key == "network" || key == "人脉" || key == "R") {
                choice.dNetwork += value;
            } else if (key == "reputation" || key == "名誉" || key == "M") {
                choice.dReputation += value;
            } else if (key == "experience" || key == "经验" || key == "J") {
                choice.dExperience += value;
            } else if (key == "public" || key == "公能实践" || key == "G") {
                choice.dPublicPractice += value;
            } else if (key == "volunteer" || key == "志愿服务" || key == "Z") {
                choice.dVolunteer += value;
            } else if (key == "social" || key == "社会实践" || key == "S") {
                choice.dSocialPractice += value;
            }
        }
    }

    // FLAGS 字段：例如 "join_union,oversleep"
    void parseFlags(const std::string& s, Choice& choice) {
        if (s.empty()) return;
        auto items = split(s, ',');
        for (auto& item : items) {
            if (!item.empty()) {
                choice.setFlags.push_back(item);
            }
        }
    }

    // 新的选项格式（不再在文本里写数值）：
    //
    // CHOICE:
    // 去教室认真上课   | energy=-1,mood=0,grade=+2 | classroom   | join_study
    // 去食堂吃早饭     | energy=+1,mood=+2,grade=-1 | canteen     |
    // 回宿舍再睡一会儿 | energy=+3,mood=+1,grade=-2 | dorm_morning| oversleep
    // ENDCHOICE
    //
    // 一行： 文本 | DELTA | NEXT | FLAGS
    void parseChoiceDefinition(const std::string& line, Scene& scene) {
        if (line.empty()) return;

        auto parts = split(line, '|');
        if (parts.empty()) return;

        Choice choice;

        // 文本（玩家看到的内容）
        choice.text = parts[0];

        std::string deltaStr;
        std::string nextId;
        std::string flagsStr;

        if (parts.size() == 2) {
            // 文本 | NEXT
            nextId = parts[1];
        } else if (parts.size() == 3) {
            // 文本 | DELTA | NEXT
            deltaStr = parts[1];
            nextId   = parts[2];
        } else {
            // 文本 | DELTA | NEXT | FLAGS
            deltaStr = parts[1];
            nextId   = parts[2];
            flagsStr = parts[3];
        }

        choice.nextSceneId = trim(nextId);
        parseDelta(deltaStr, choice);
        parseFlags(flagsStr, choice);

        scene.choices.push_back(choice);
    }

    // 将一段文本按像素宽度自动换行（基于 sf::String / UTF-32，兼容 SFML 3）
    sf::String wrapTextToWidth(const sf::String& input,
                               const sf::Font& font,
                               unsigned int characterSize,
                               float maxWidth) {
        // SFML 3: Text 需要传 font
        sf::Text measure(font, "", characterSize);

        sf::String result;
        sf::String currentLine;

        for (std::size_t i = 0; i < input.getSize(); ++i) {
            auto ch = input[i];

            // 保留原有换行：遇到 '\n' 就强制换行
            if (ch == '\n') {
                result += currentLine;
                result += '\n';
                currentLine.clear();
                continue;
            }

            sf::String testLine = currentLine;
            testLine += ch;

            measure.setString(testLine);
            auto bounds = measure.getLocalBounds();
            float width = bounds.size.x;  // SFML 3: 使用 size.x

            if (width > maxWidth && !currentLine.isEmpty()) {
                // 超出宽度：先把当前行放入结果，再重新开始一行
                result += currentLine;
                result += '\n';
                currentLine.clear();
                currentLine += ch;
            } else {
                currentLine = testLine;
            }
        }

        if (!currentLine.isEmpty()) {
            result += currentLine;
        }

        return result;
    }

    // 读取单个 .scene 文件
    bool loadSceneFile(const std::filesystem::path& path, Scene& scene) {
        std::ifstream in(path);
        if (!in) {
            std::cerr << "无法打开场景文件: " << path << "\n";
            return false;
        }

        std::string line;

        // 读 ID
        while (std::getline(in, line)) {
            line = trim(line);
            if (!line.empty()) break;
        }
        if (!startsWith(line, "ID:")) {
            std::cerr << "场景文件缺少 ID: " << path << "\n";
            return false;
        }
        scene.id = trim(line.substr(3));

        // 读 BG
        while (std::getline(in, line)) {
            line = trim(line);
            if (!line.empty()) break;
        }
        if (!startsWith(line, "BG:")) {
            std::cerr << "场景文件缺少 BG: " << path << "\n";
            return false;
        }
        scene.backgroundPath = trim(line.substr(3));

        // 状态机：TEXT 区 + CHOICE 区
        enum class Section {
            None,
            Text,
            Choice
        };

        Section section = Section::None;
        std::ostringstream dialogueBuf;

        while (std::getline(in, line)) {
            std::string t = trim(line);
            if (t.empty()) {
                if (section == Section::Text) {
                    dialogueBuf << "\n";
                }
                continue;
            }

            if (startsWith(t, "TEXT:")) {
                section = Section::Text;
                continue;
            }
            if (startsWith(t, "ENDTEXT")) {
                section = Section::None;
                continue;
            }
            if (startsWith(t, "CHOICE:")) {
                section = Section::Choice;
                continue;
            }
            if (startsWith(t, "ENDCHOICE")) {
                section = Section::None;
                continue;
            }

            if (section == Section::Text) {
                if (!dialogueBuf.str().empty() &&
                    dialogueBuf.str().back() != '\n') {
                    dialogueBuf << "\n";
                }
                dialogueBuf << t;
            } else if (section == Section::Choice) {
                parseChoiceDefinition(t, scene);
            } else {
                // 其他内容忽略
            }
        }

        scene.dialogue = dialogueBuf.str();
        return true;
    }

    // 载入整个 scenes 目录
    std::map<std::string, Scene> loadScenes(const std::string& dir) {
        std::map<std::string, Scene> scenes;
        namespace fs = std::filesystem;

        fs::path base(dir);
        if (!fs::exists(base) || !fs::is_directory(base)) {
            std::cerr << "场景目录不存在: " << dir << "\n";
            return scenes;
        }

        for (const auto& entry : fs::directory_iterator(base)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() != ".scene") continue;

            Scene s;
            if (loadSceneFile(entry.path(), s)) {
                scenes[s.id] = std::move(s);
            }
        }

        return scenes;
    }

    // 根据 flag 对“目标场景 ID”做重定向（你可以自己在这里写分支逻辑）
    std::string resolveSceneId(const std::string& rawId,
                               const std::map<std::string, bool>& flags) {
        // 示例：晚上的宿舍，根据是否加入学生会分两种剧情
        if (rawId == "dorm_evening") {
            auto it = flags.find("join_union");
            if (it != flags.end() && it->second) {
                return "dorm_evening_after_union";   // 加入学生会版
            } else {
                return "dorm_evening_normal";        // 普通版
            }
        }

        // 默认：不做重定向
        return rawId;
    }

    // ----------------- 主逻辑 -----------------

    void run() {
        sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u{1100u, 700u}),
        "Campus Simulator - New Script Format"
        );
        window.setFramerateLimit(60);

        // 字体：你已经把 NotoSansSC 或 Hiragino Sans GB 拷到这个路径了
        sf::Font font;
        if (!font.openFromFile("assets/NotoSansSC-Regular.otf")) {
            std::cerr << "无法加载字体 assets/NotoSansSC-Regular.otf\n";
            return;
        }

        // 载入所有场景
        auto scenes = loadScenes("scenes");
        if (scenes.empty()) {
            std::cerr << "未加载到任何场景，请检查 scenes 目录。\n";
            return;
        }

        std::string currentSceneId = "start";
        if (!scenes.count(currentSceneId)) {
            std::cerr << "缺少起始场景 ID: start\n";
            return;
        }
        Scene* currentScene = &scenes[currentSceneId];

        GameState game;  // 属性 + flags

        // 背景图
        sf::Texture backgroundTexture;
        bool hasBackground = false;
        sf::Vector2f backgroundScale{1.f, 1.f};

        auto loadBackgroundForCurrentScene = [&]() {
            hasBackground = false;
            if (!currentScene->backgroundPath.empty()) {
                if (backgroundTexture.loadFromFile(currentScene->backgroundPath)) {
                    hasBackground = true;
                    auto size = backgroundTexture.getSize();
                    backgroundScale = {
                        static_cast<float>(window.getSize().x) / size.x,
                        static_cast<float>(window.getSize().y) / size.y
                    };
                } else {
                    std::cerr << "无法加载背景图 " << currentScene->backgroundPath << "\n";
                }
            }
        };

        loadBackgroundForCurrentScene();

        // 对话框背景
        sf::RectangleShape dialogBox;
        dialogBox.setSize({
            static_cast<float>(window.getSize().x) - 80.f,
            310.f   // 增加对话框高度，避免TEXT和选项挤在一起
        });
        dialogBox.setPosition({40.f, 330.f});
        dialogBox.setFillColor(sf::Color(0, 0, 80, 200));

        // 对话文字
        sf::Text dialogueText(font, "", 20);
        dialogueText.setFillColor(sf::Color::White);
        dialogueText.setPosition({
            dialogBox.getPosition().x + 20.f,
            dialogBox.getPosition().y + 15.f   // 稍微上移一些
        });

        // 选项文字（最多 8 个）
        std::vector<sf::Text> choiceTexts;
        choiceTexts.reserve(8);
        for (int i = 0; i < 8; ++i) {
            sf::Text t(font, "", 18);
            t.setFillColor(sf::Color(230, 230, 210));
            choiceTexts.push_back(t);
        }

        // 属性显示（左上角）
        sf::Text statsText(font, "", 18);
        statsText.setFillColor(sf::Color::Yellow);
        // 初始位置先随便给一个值，真正的位置在 updateUI 里统一设置
        statsText.setPosition(sf::Vector2f{40.f, 40.f});

        // 属性栏背景框（左上角一整条状态栏，更明显）
        sf::RectangleShape statsBox;
        // 深蓝色、比较不透明，这样和你的霓虹背景能拉开对比
        statsBox.setFillColor(sf::Color(0, 0, 60, 220));
        // 边框稍微粗一点
        statsBox.setOutlineColor(sf::Color(255, 255, 255, 220));
        statsBox.setOutlineThickness(3.f);

        auto updateUI = [&]() {
            // ---- 计算对话和选项文本，并根据内容自适应对话框大小 ----

            // 1) 准备对话文本并测量高度
            const std::string& d = currentScene->dialogue;
            sf::String dlg = sf::String::fromUtf8(d.begin(), d.end());

            float dialogPaddingLeft   = 40.f;
            float dialogPaddingRight  = 40.f;
            float dialogPaddingTop    = 20.f;
            float dialogPaddingBottom = 20.f;
            float gapTextToChoice     = 20.f;
            float choiceLineSpacing   = 12.f;

            float dialogWidth = static_cast<float>(window.getSize().x)
                                - dialogPaddingLeft - dialogPaddingRight;
            float dialogMaxWidth = dialogWidth - 40.f; // 再预留一点内边距

            sf::String wrappedDlg = wrapTextToWidth(
                dlg,
                font,
                dialogueText.getCharacterSize(),
                dialogMaxWidth
            );
            dialogueText.setString(wrappedDlg);
            auto dlgBounds = dialogueText.getLocalBounds();
            float dlgHeight = dlgBounds.size.y;

            // 2) 准备选项文本，先全部 wrap 一遍并测量高度
            std::vector<float> choiceHeights(choiceTexts.size(), 0.f);
            float totalChoiceHeight = 0.f;

            for (std::size_t i = 0; i < choiceTexts.size(); ++i) {
                if (i < currentScene->choices.size()) {
                    const auto& ch = currentScene->choices[i];
                    std::string lineUtf8 = std::to_string(i + 1) + ") " + ch.text;

                    sf::String line = sf::String::fromUtf8(lineUtf8.begin(), lineUtf8.end());
                    float choiceMaxWidth = dialogMaxWidth - 40.f;
                    sf::String wrappedLine = wrapTextToWidth(
                        line,
                        font,
                        choiceTexts[i].getCharacterSize(),
                        choiceMaxWidth
                    );
                    choiceTexts[i].setString(wrappedLine);

                    auto cb = choiceTexts[i].getLocalBounds();
                    float h = cb.size.y;
                    choiceHeights[i] = h;
                    totalChoiceHeight += h + choiceLineSpacing;
                } else {
                    choiceTexts[i].setString("");
                    choiceHeights[i] = 0.f;
                }
            }
            // 去掉最后一个多加的行距
            if (totalChoiceHeight > 0.f) {
                totalChoiceHeight -= choiceLineSpacing;
            }

            // 3) 根据对话高度 + 选项高度计算对话框高度，并贴到底部
            float dialogHeight = dialogPaddingTop + dlgHeight;
            if (totalChoiceHeight > 0.f) {
                dialogHeight += gapTextToChoice + totalChoiceHeight;
            }
            dialogHeight += dialogPaddingBottom;

            // 将对话框的最小高度调为原来的三分之一
            float minDialogHeight = 60.f;
            if (dialogHeight < minDialogHeight) dialogHeight = minDialogHeight;
            float maxDialogHeight = static_cast<float>(window.getSize().y) * 0.6f;
            if (dialogHeight > maxDialogHeight) dialogHeight = maxDialogHeight;

            float dialogX = dialogPaddingLeft;
            // 离底部留 40 像素
            float dialogY = static_cast<float>(window.getSize().y) - dialogHeight - 40.f;

            dialogBox.setPosition({dialogX, dialogY});
            dialogBox.setSize({dialogWidth, dialogHeight});

            // 4) 布局对话文本和选项文本
            dialogueText.setPosition({dialogX + 20.f, dialogY + dialogPaddingTop});

            float currentY = dialogueText.getPosition().y + dlgHeight
                             + (totalChoiceHeight > 0.f ? gapTextToChoice : 0.f);
            float choiceX = dialogX + 40.f;

            for (std::size_t i = 0; i < choiceTexts.size(); ++i) {
                if (i < currentScene->choices.size()) {
                    choiceTexts[i].setPosition({choiceX, currentY});
                    currentY += choiceHeights[i] + choiceLineSpacing;
                }
            }

            // 属性栏：第一行 5 项（含经验），第二行 3 项
            std::string statsStr =
                "体质: "       + std::to_string(game.stats.physique) +
                "   学力: "     + std::to_string(game.stats.study) +
                "   人脉: "     + std::to_string(game.stats.network) +
                "   名誉: "     + std::to_string(game.stats.reputation) +
                "   经验: "     + std::to_string(game.stats.experience) +
                "\n公能实践: "  + std::to_string(game.stats.publicPractice) +
                "   志愿服务: " + std::to_string(game.stats.volunteer) +
                "   社会实践: " + std::to_string(game.stats.socialPractice);
            statsText.setString(
                sf::String::fromUtf8(statsStr.begin(), statsStr.end())
            );

            // 固定左上角属性栏文字位置（稍微往下，避免贴着窗口边缘）
            statsText.setPosition(sf::Vector2f{30.f, 40.f});

            // 左上角整条状态栏背景
            // 覆盖从左侧留一点边距到接近右侧的位置，高度给够一行半文字
            statsBox.setPosition(sf::Vector2f{20.f, 20.f});
            statsBox.setSize({ 1060.f, 70.f });
        };

        // 统一的选项命中检测：点击和悬停都用它
        auto hitTestChoice = [&](std::size_t idx, const sf::Vector2f& worldPos) -> bool {
            if (idx >= currentScene->choices.size()) return false;

            const sf::String& str = choiceTexts[idx].getString();
            if (str.isEmpty()) return false;

            sf::FloatRect bounds = choiceTexts[idx].getGlobalBounds();
            // SFML 3: FloatRect 使用 position/size 内部结构，但我们只需要 contains()
            // 这里直接用文本自身的边界作为命中区域
            return bounds.contains(worldPos);
        };

        updateUI();

        int hoveredIndex = -1;  // 当前鼠标悬停的选项索引，-1 表示没有

        // 主循环
        while (window.isOpen()) {
            int chosenIndex = -1;

            // 事件处理循环（兼容 C++17：不在 while 头部使用 init-statement）
            while (true) {
                auto event = window.pollEvent();
                if (!event) {
                    break; // 没有更多事件
                }

                if (event->is<sf::Event::Closed>()) {
                    window.close();
                }

                if (event->is<sf::Event::KeyPressed>()) {
                    const auto* key = event->getIf<sf::Event::KeyPressed>();
                    switch (key->code) {
                        case sf::Keyboard::Key::Num1: chosenIndex = 0; break;
                        case sf::Keyboard::Key::Num2: chosenIndex = 1; break;
                        case sf::Keyboard::Key::Num3: chosenIndex = 2; break;
                        case sf::Keyboard::Key::Num4: chosenIndex = 3; break;
                        case sf::Keyboard::Key::Num5: chosenIndex = 4; break;
                        case sf::Keyboard::Key::Num6: chosenIndex = 5; break;
                        case sf::Keyboard::Key::Num7: chosenIndex = 6; break;
                        case sf::Keyboard::Key::Num8: chosenIndex = 7; break;
                        default: break;
                    }
                }

                // 鼠标左键点击选项：根据点击坐标判断点中了哪一条选项
                if (event->is<sf::Event::MouseButtonPressed>()) {
                    const auto* mb = event->getIf<sf::Event::MouseButtonPressed>();
                    if (mb && mb->button == sf::Mouse::Button::Left) {
                        // 将像素坐标转成世界坐标（考虑视图变换）
                        sf::Vector2f worldPos = window.mapPixelToCoords(mb->position);

                        for (std::size_t i = 0; i < choiceTexts.size(); ++i) {
                            if (hitTestChoice(i, worldPos)) {
                                chosenIndex = static_cast<int>(i);
                                break;
                            }
                        }
                    }
                }

                // 鼠标移动：更新当前悬停的选项索引，用于高亮显示
                if (event->is<sf::Event::MouseMoved>()) {
                    const auto* mv = event->getIf<sf::Event::MouseMoved>();
                    if (mv) {
                        sf::Vector2f worldPos = window.mapPixelToCoords(mv->position);
                        hoveredIndex = -1;
                        for (std::size_t i = 0; i < choiceTexts.size(); ++i) {
                            if (hitTestChoice(i, worldPos)) {
                                hoveredIndex = static_cast<int>(i);
                                break;
                            }
                        }
                    }
                }
            }

            // 处理选项
            if (chosenIndex >= 0 &&
                static_cast<std::size_t>(chosenIndex) < currentScene->choices.size()) {

                const Choice& choice = currentScene->choices[chosenIndex];

                // 1. 改属性
                game.stats.physique        += choice.dPhysique;
                game.stats.study           += choice.dStudy;
                game.stats.network         += choice.dNetwork;
                game.stats.reputation      += choice.dReputation;
                game.stats.experience      += choice.dExperience;
                game.stats.publicPractice  += choice.dPublicPractice;
                game.stats.volunteer       += choice.dVolunteer;
                game.stats.socialPractice  += choice.dSocialPractice;

                auto clamp = [](int v) {
                    if (v < 0) return 0;
                    if (v > 10) return 10;
                    return v;
                };
                game.stats.physique        = clamp(game.stats.physique);
                game.stats.study           = clamp(game.stats.study);
                game.stats.network         = clamp(game.stats.network);
                game.stats.reputation      = clamp(game.stats.reputation);
                game.stats.experience      = clamp(game.stats.experience);
                game.stats.publicPractice  = clamp(game.stats.publicPractice);
                game.stats.volunteer       = clamp(game.stats.volunteer);
                game.stats.socialPractice  = clamp(game.stats.socialPractice);

                // 2. 记录 flags
                for (const auto& f : choice.setFlags) {
                    game.flags[f] = true;
                }

                // 3. 计算真正要去的场景 ID（根据 flags 做分支）
                std::string targetId =
                    resolveSceneId(choice.nextSceneId, game.flags);

                auto it = scenes.find(targetId);
                if (it != scenes.end()) {
                    currentSceneId = targetId;
                    currentScene   = &it->second;
                    loadBackgroundForCurrentScene();
                    updateUI();
                } else {
                    std::cerr << "找不到场景: " << targetId << "\n";
                }
            }

            // 绘制
            window.clear(sf::Color(20, 20, 40));

            if (hasBackground) {
                sf::Sprite bg(backgroundTexture);
                bg.setScale(backgroundScale);
                window.draw(bg);
            }

            window.draw(dialogBox);
            window.draw(dialogueText);

            for (std::size_t i = 0; i < choiceTexts.size(); ++i) {
                if (i >= currentScene->choices.size()) continue;

                if (static_cast<int>(i) == hoveredIndex) {
                    // 悬停选项：变亮、加下划线，并在前面加一个小箭头 ">"
                    choiceTexts[i].setFillColor(sf::Color(255, 255, 200));
                    choiceTexts[i].setStyle(sf::Text::Underlined);

                    sf::Text arrow(font, ">", choiceTexts[i].getCharacterSize());
                    arrow.setFillColor(sf::Color(255, 255, 200));
                    arrow.setPosition(sf::Vector2f{
                        choiceTexts[i].getPosition().x - 20.f,
                        choiceTexts[i].getPosition().y
                    });
                    window.draw(arrow);
                } else {
                    // 非悬停：恢复默认颜色和样式
                    choiceTexts[i].setFillColor(sf::Color(230, 230, 210));
                    choiceTexts[i].setStyle(sf::Text::Regular);
                }

                window.draw(choiceTexts[i]);
            }
            window.draw(statsBox);   // 先画左上角属性栏背景框
            window.draw(statsText);  // 再画属性文字

            window.display();
        }
    }

} // namespace CampusSim

int main() {
    CampusSim::run();
    return 0;
}