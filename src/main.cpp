#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cctype>
#include <cmath>

namespace CampusSim {

    // 初始窗口逻辑分辨率（只作为启动大小，用于 VideoMode）
    constexpr float INITIAL_WIDTH  = 1100.f;
    constexpr float INITIAL_HEIGHT = 700.f;

    // 背景中心微调偏移量：如果觉得画面偏上/偏下/偏左/偏右，可以在这里改
    // 正方向：X 向右为正；Y 向下为正
    constexpr float BG_CENTER_OFFSET_X = 0.f;
    constexpr float BG_CENTER_OFFSET_Y = 0.f;

    // ----------------- 数据结构 -----------------

    struct Stats {
        int physique        = 0;  // 体质
        int study           = 0;  // 学力
        int network         = 0;  // 人脉
        int reputation      = 0;  // 名誉
        int experience      = 0;  // 经验
        int GongnengLecture = 0;  // 公能讲座
        int volunteer       = 0;  // 志愿服务
        int socialPractice  = 0;  // 社会实践
    };

    struct GameState {
        Stats stats;
        std::map<std::string, bool> flags;  // 记录关键历史选择
    };

    struct Choice {
        std::string text;                   // 选项文字（UTF-8）
        int dPhysique        = 0;           // 体质 变化
        int dStudy           = 0;           // 学力 变化
        int dNetwork         = 0;           // 人脉 变化
        int dReputation      = 0;           // 名誉 变化
        int dExperience      = 0;           // 经验 变化
        int dGongnengLecture = 0;           // 公能讲座 变化
        int dVolunteer       = 0;           // 志愿服务 变化
        int dSocialPractice  = 0;           // 社会实践 变化
        std::string nextSceneId;            // 下一个场景 ID
        std::vector<std::string> setFlags;  // 选了这个选项要打的 flag
        std::vector<std::string> requiredFlags;  // 显示该选项所需为 true 的 flags（全部满足才显示）

        bool  timed          = false;  // 是否为限时选项（FLAGS 中包含 timedXX）
        float timeLimit      = 0.f;    // 限时总时长（秒）
        float remainingTime  = 0.f;    // 当前剩余时间（秒）
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

    // DELTA 字段：例如 "体质=-1,学力=+2" / "physique=-1,study=+2"
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
            } else if (key == "public" || key == "公能讲座" || key == "G") {
                choice.dGongnengLecture += value;
            } else if (key == "volunteer" || key == "志愿服务" || key == "Z") {
                choice.dVolunteer += value;
            } else if (key == "social" || key == "社会实践" || key == "S") {
                choice.dSocialPractice += value;
            }
        }
    }

    // FLAGS 字段：例如 "join_union,oversleep,timed10"
    void parseFlags(const std::string& s, Choice& choice) {
        if (s.empty()) return;
        if (s == "0") return;  // 0 作为占位符表示“没有 flags”

        auto items = split(s, ',');
        for (auto& item : items) {
            if (item.empty() || item == "0") continue;

            // 特殊语法：timed10 / timed5 / timed30 …… 表示限时选项
            // 规则：以 "timed" 开头，后面必须是纯数字，才视为限时关键词
            if (item.rfind("timed", 0) == 0 && item.size() > 5) {
                try {
                    int seconds = std::stoi(item.substr(5));
                    if (seconds > 0) {
                        choice.timed         = true;
                        choice.timeLimit     = static_cast<float>(seconds);
                        choice.remainingTime = choice.timeLimit;
                    }
                } catch (...) {
                    // 转数字失败就忽略限时配置，不影响其他 flag
                }
                continue;  // 不把 timedXX 当成普通 flag 记录
            }

            // 其他全部作为普通 flag 记录
            choice.setFlags.push_back(item);
        }
    }

    // REQUIRES 字段：例如 "research_invite,join_union"
    void parseRequiredFlags(const std::string& s, Choice& choice) {
        if (s.empty()) return;
        if (s == "0") return; // 0 作为占位符时视为“无条件”
        auto items = split(s, ',');
        for (auto& item : items) {
            if (!item.empty() && item != "0") {
                choice.requiredFlags.push_back(item);
            }
        }
    }

    // 新的选项格式：支持最多5列，最后一列为 REQUIRES
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
        std::string requiresStr;

        if (parts.size() == 2) {
            // 文本 | NEXT
            nextId = parts[1];
        } else if (parts.size() == 3) {
            // 文本 | DELTA | NEXT
            deltaStr = parts[1];
            nextId   = parts[2];
        } else if (parts.size() == 4) {
            // 文本 | DELTA | NEXT | FLAGS
            deltaStr = parts[1];
            nextId   = parts[2];
            flagsStr = parts[3];
        } else { // parts.size() >= 5
            // 文本 | DELTA | NEXT | FLAGS | REQUIRES
            deltaStr    = parts[1];
            nextId      = parts[2];
            flagsStr    = parts[3];
            requiresStr = parts[4];
        }

        choice.nextSceneId = trim(nextId);
        parseDelta(deltaStr, choice);
        parseFlags(flagsStr, choice);
        parseRequiredFlags(requiresStr, choice);

        scene.choices.push_back(choice);
    }

    // 将一段文本按像素宽度自动换行（基于 sf::String / UTF-32，兼容 SFML 3）
    sf::String wrapTextToWidth(const sf::String& input,
                               const sf::Font& font,
                               unsigned int characterSize,
                               float maxWidth) {
        sf::Text measure(font, "", characterSize);

        sf::String result;
        sf::String currentLine;

        for (std::size_t i = 0; i < input.getSize(); ++i) {
            auto ch = input[i];

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
            float width = bounds.size.x;

            if (width > maxWidth && !currentLine.isEmpty()) {
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

    // 根据 flag 对“目标场景 ID”做重定向（你可以自己扩展）
    std::string resolveSceneId(const std::string& rawId,
                               const std::map<std::string, bool>& flags) {
        if (rawId == "dorm_evening") {
            auto it = flags.find("join_union");
            if (it != flags.end() && it->second) {
                return "dorm_evening_after_union";
            } else {
                return "dorm_evening_normal";
            }
        }
        return rawId;
    }

    // ----------------- 主逻辑 -----------------

    void run() {
        sf::RenderWindow window(
            sf::VideoMode(sf::Vector2u{
                static_cast<unsigned int>(INITIAL_WIDTH),
                static_cast<unsigned int>(INITIAL_HEIGHT)
            }),
            "Campus Simulator"
        );
        window.setFramerateLimit(60);


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

        // 用于计算每一帧时间差的时钟
        sf::Clock frameClock;

        // 进入一个新场景时，重置该场景所有限时选项的计时器
        auto resetChoiceTimers = [&]() {
            if (!currentScene) return;
            for (auto& ch : currentScene->choices) {
                if (ch.timed) {
                    ch.remainingTime = ch.timeLimit;
                }
            }
        };

        // 背景图
        sf::Texture backgroundTexture;
        bool hasBackground = false;

        auto loadBackgroundForCurrentScene = [&]() {
            hasBackground = false;
            if (!currentScene->backgroundPath.empty()) {
                if (backgroundTexture.loadFromFile(currentScene->backgroundPath)) {
                    hasBackground = true;
                } else {
                    std::cerr << "无法加载背景图 " << currentScene->backgroundPath << "\n";
                }
            }
        };

        loadBackgroundForCurrentScene();
        resetChoiceTimers();

        // 对话框背景
        sf::RectangleShape dialogBox;
        dialogBox.setFillColor(sf::Color(0, 0, 150, 230));  // 更明显的深蓝色，方便观察

        // 对话文字
        sf::Text dialogueText(font, "", 20);
        dialogueText.setFillColor(sf::Color::White);

        // 选项文字（最多 8 个）
        std::vector<sf::Text> choiceTexts;
        choiceTexts.reserve(8);
        for (int i = 0; i < 8; ++i) {
            sf::Text t(font, "", 18);
            t.setFillColor(sf::Color(230, 230, 210));
            choiceTexts.push_back(t);
        }
        std::vector<std::size_t> visibleChoiceIndices;

        // 属性显示（左上角）
        sf::Text statsText(font, "", 18);
        statsText.setFillColor(sf::Color::Yellow);

        // 属性栏背景框（左上角）
        sf::RectangleShape statsBox;
        statsBox.setFillColor(sf::Color(0, 0, 60, 220));
        statsBox.setOutlineColor(sf::Color(255, 255, 255, 220));
        statsBox.setOutlineThickness(3.f);

        // UI 更新函数：根据当前窗口大小重新布局
        auto updateUI = [&]() {
            // 用当前视图尺寸做布局，避免 HiDPI 或视图缩放导致的坐标偏移
            const sf::View& view = window.getView();
            sf::Vector2f viewSize = view.getSize();
            float winW = viewSize.x;
            float winH = viewSize.y;
            if (winW <= 0.f || winH <= 0.f) return;

            const std::string& d = currentScene->dialogue;
            sf::String dlg = sf::String::fromUtf8(d.begin(), d.end());

            float dialogPaddingLeft   = 40.f;
            float dialogPaddingRight  = 40.f;
            float dialogPaddingTop    = 20.f;
            float dialogPaddingBottom = 20.f;
            float gapTextToChoice     = 20.f;
            float choiceLineSpacing   = 12.f;

            float dialogWidth = winW - dialogPaddingLeft - dialogPaddingRight;
            if (dialogWidth < 200.f) dialogWidth = 200.f;
            float dialogMaxWidth = dialogWidth - 40.f;

            sf::String wrappedDlg = wrapTextToWidth(
                dlg,
                font,
                dialogueText.getCharacterSize(),
                dialogMaxWidth
            );
            dialogueText.setString(wrappedDlg);
            auto dlgBounds = dialogueText.getLocalBounds();
            float dlgHeight = dlgBounds.size.y;

            // 1) 计算可见选项
            visibleChoiceIndices.clear();
            for (std::size_t i = 0; i < currentScene->choices.size(); ++i) {
                const Choice& ch = currentScene->choices[i];
                bool visible = true;

                // REQUIRES：所有 requiredFlags 必须为 true
                for (const auto& rf : ch.requiredFlags) {
                    auto it = game.flags.find(rf);
                    if (it == game.flags.end() || !it->second) {
                        visible = false;
                        break;
                    }
                }

                // 限时选项：时间耗尽就不再显示
                if (visible && ch.timed && ch.remainingTime <= 0.f) {
                    visible = false;
                }

                if (visible) {
                    visibleChoiceIndices.push_back(i);
                }
            }

            // 2) 准备选项文本并测高度
            std::vector<float> choiceHeights(choiceTexts.size(), 0.f);
            float totalChoiceHeight = 0.f;

            for (std::size_t i = 0; i < choiceTexts.size(); ++i) {
                if (i < visibleChoiceIndices.size()) {
                    const auto& ch = currentScene->choices[visibleChoiceIndices[i]];
                    std::string lineUtf8 = std::to_string(i + 1) + ") " + ch.text;

                    // 限时选项追加剩余时间（向上取整）
                    if (ch.timed && ch.remainingTime > 0.f) {
                        int seconds = static_cast<int>(std::ceil(ch.remainingTime));
                        if (seconds < 0) seconds = 0;
                        lineUtf8 += " (剩余" + std::to_string(seconds) + "秒)";
                    }

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
            if (totalChoiceHeight > 0.f) {
                totalChoiceHeight -= choiceLineSpacing;
            }

            // 3) 计算对话框高度，固定在底部，大小自适应内容
            float dialogHeight = dialogPaddingTop + dlgHeight;
            if (totalChoiceHeight > 0.f) {
                dialogHeight += gapTextToChoice + totalChoiceHeight;
            }
            dialogHeight += dialogPaddingBottom;

            // 限制高度范围，防止过小或占满全屏
            float minDialogHeight = 120.f;
            if (dialogHeight < minDialogHeight) dialogHeight = minDialogHeight;
            float maxDialogHeight = winH * 0.6f;
            if (dialogHeight > maxDialogHeight) dialogHeight = maxDialogHeight;

            // 对话框贴近底部，预留一点下边距
            float bottomMargin = 30.f;
            float dialogX = dialogPaddingLeft;
            float dialogY = winH - dialogHeight - bottomMargin;

            dialogBox.setPosition({dialogX, dialogY});
            dialogBox.setSize({dialogWidth, dialogHeight});

            // 对话文本位置：相对框顶的固定内边距
            dialogueText.setPosition({dialogX + 20.f, dialogY + dialogPaddingTop});

            // 选项排版：在对话文本下方开始，和内容保持间距
            float currentY = dialogueText.getPosition().y + dlgHeight
                             + (totalChoiceHeight > 0.f ? gapTextToChoice : 0.f);
            float choiceX  = dialogX + 40.f;

            for (std::size_t i = 0; i < choiceTexts.size(); ++i) {
                if (i < visibleChoiceIndices.size()) {
                    choiceTexts[i].setPosition({choiceX, currentY});
                    currentY += choiceHeights[i] + choiceLineSpacing;
                }
            }

            // 4) 属性栏文字和背景
            std::string statsStr =
                "体质: "       + std::to_string(game.stats.physique) +
                "   学力: "     + std::to_string(game.stats.study) +
                "   人脉: "     + std::to_string(game.stats.network) +
                "   名誉: "     + std::to_string(game.stats.reputation) +
                "   经验: "     + std::to_string(game.stats.experience) +
                "\n公能讲座: "  + std::to_string(game.stats.GongnengLecture) +
                "   志愿服务: " + std::to_string(game.stats.volunteer) +
                "   社会实践: " + std::to_string(game.stats.socialPractice);

            statsText.setString(sf::String::fromUtf8(statsStr.begin(), statsStr.end()));
            statsText.setPosition(sf::Vector2f{30.f, 40.f});

            statsBox.setPosition(sf::Vector2f{20.f, 20.f});
            statsBox.setSize({winW - 40.f, 70.f});

        };

        // 统一的选项命中检测
        auto hitTestChoice = [&](std::size_t idx, const sf::Vector2f& worldPos) -> bool {
            if (idx >= visibleChoiceIndices.size()) return false;
            const sf::String& str = choiceTexts[idx].getString();
            if (str.isEmpty()) return false;

            sf::FloatRect bounds = choiceTexts[idx].getGlobalBounds();
            return bounds.contains(worldPos);
        };

        // 先更新一次界面
        updateUI();

        int hoveredIndex = -1;  // 当前鼠标悬停的选项索引，-1 表示没有

        // 主循环
        while (window.isOpen()) {
            // 本帧时间差（秒）
            float dt = frameClock.restart().asSeconds();
            if (dt < 0.f) dt = 0.f;
            if (dt > 0.5f) dt = 0.5f;

            // 更新限时选项的剩余时间
            for (auto& ch : currentScene->choices) {
                if (ch.timed && ch.remainingTime > 0.f) {
                    ch.remainingTime -= dt;
                    if (ch.remainingTime < 0.f) {
                        ch.remainingTime = 0.f;
                    }
                }
            }

            // 先处理事件（包括窗口大小变化）
            int chosenIndex = -1;

            while (true) {
                auto event = window.pollEvent();
                if (!event) {
                    break;
                }

                // 窗口大小改变：更新视图和布局
                if (event->is<sf::Event::Resized>()) {
                    const auto* rs = event->getIf<sf::Event::Resized>();
                    if (rs) {
                        // 保持“世界坐标 == 像素坐标”，防止窗口缩放后视图仍用旧尺寸导致居中偏移
                        sf::View view(sf::FloatRect(
                            sf::Vector2f{0.f, 0.f},
                            sf::Vector2f{
                                static_cast<float>(rs->size.x),
                                static_cast<float>(rs->size.y)
                            }
                        ));
                        sf::Vector2f viewSize = view.getSize();
                        view.setCenter(sf::Vector2f{
                            viewSize.x * 0.5f,
                            viewSize.y * 0.5f
                        });
                        window.setView(view);

                        loadBackgroundForCurrentScene();
                        updateUI();
                    }
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

                // 鼠标左键点击选项
                if (event->is<sf::Event::MouseButtonPressed>()) {
                    const auto* mb = event->getIf<sf::Event::MouseButtonPressed>();
                    if (mb && mb->button == sf::Mouse::Button::Left) {
                        sf::Vector2f worldPos = window.mapPixelToCoords(mb->position);
                        for (std::size_t i = 0; i < visibleChoiceIndices.size(); ++i) {
                            if (hitTestChoice(i, worldPos)) {
                                chosenIndex = static_cast<int>(i);
                                break;
                            }
                        }
                    }
                }

                // 鼠标移动：更新悬停项
                if (event->is<sf::Event::MouseMoved>()) {
                    const auto* mv = event->getIf<sf::Event::MouseMoved>();
                    if (mv) {
                        sf::Vector2f worldPos = window.mapPixelToCoords(mv->position);
                        hoveredIndex = -1;
                        for (std::size_t i = 0; i < visibleChoiceIndices.size(); ++i) {
                            if (hitTestChoice(i, worldPos)) {
                                hoveredIndex = static_cast<int>(i);
                                break;
                            }
                        }
                    }
                }
            }

            // 然后根据最新窗口大小再更新一遍 UI（防止尺寸变化但没有事件时）
            updateUI();

            // 处理选项
            if (chosenIndex >= 0 &&
                static_cast<std::size_t>(chosenIndex) < visibleChoiceIndices.size()) {

                const Choice& choice = currentScene->choices[visibleChoiceIndices[chosenIndex]];

                // 1. 改属性
                game.stats.physique        += choice.dPhysique;
                game.stats.study           += choice.dStudy;
                game.stats.network         += choice.dNetwork;
                game.stats.reputation      += choice.dReputation;
                game.stats.experience      += choice.dExperience;
                game.stats.GongnengLecture += choice.dGongnengLecture;
                game.stats.volunteer       += choice.dVolunteer;
                game.stats.socialPractice  += choice.dSocialPractice;

                auto clamp = [](int v) {
                    if (v < -100) return -100;
                    if (v > 100) return 100;
                    return v;
                };
                game.stats.physique        = clamp(game.stats.physique);
                game.stats.study           = clamp(game.stats.study);
                game.stats.network         = clamp(game.stats.network);
                game.stats.reputation      = clamp(game.stats.reputation);
                game.stats.experience      = clamp(game.stats.experience);
                game.stats.GongnengLecture = clamp(game.stats.GongnengLecture);
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
                    resetChoiceTimers();
                    hoveredIndex = -1;
                    updateUI();
                } else {
                    std::cerr << "找不到场景: " << targetId << "\n";
                }
            }

            // 绘制
            window.clear(sf::Color(20, 20, 40));

            // 1) 背景：强制等比缩放 + 完整显示 + 严格居中（可能留黑边）
            if (hasBackground) {
                const sf::View& view = window.getView();
                sf::Vector2f viewCenter = view.getCenter();
                sf::Vector2f viewSize = view.getSize();
                float winW = viewSize.x;
                float winH = viewSize.y;

                sf::Sprite bg(backgroundTexture);
                auto texSize = backgroundTexture.getSize();
                float texW = static_cast<float>(texSize.x);
                float texH = static_cast<float>(texSize.y);

                if (texW > 0.f && texH > 0.f) {
                    // 将原点设置为纹理中心，方便以中心为基准缩放/居中
                    bg.setOrigin(sf::Vector2f{texW * 0.5f, texH * 0.5f});

                    // 计算两个缩放比例
                    float scaleX = winW / texW;
                    float scaleY = winH / texH;
                    // 取较小值，保证整张背景图完全显示，不被裁剪（可能留黑边）
                    float scale  = std::min(scaleX, scaleY);

                    // 设置等比缩放（SFML 3：使用 Vector2f）
                    bg.setScale(sf::Vector2f{scale, scale});

                    // 直接把 sprite 放在窗口中心，再加上可调偏移量
                    bg.setPosition(sf::Vector2f{
                        viewCenter.x + BG_CENTER_OFFSET_X,
                        viewCenter.y + BG_CENTER_OFFSET_Y
                    });
                }

                window.draw(bg);
            }

            // 2) 对话框 + 文本 + 选项
            window.draw(dialogBox);
            window.draw(dialogueText);

            for (std::size_t i = 0; i < visibleChoiceIndices.size(); ++i) {
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
                    choiceTexts[i].setFillColor(sf::Color(230, 230, 210));
                    choiceTexts[i].setStyle(sf::Text::Regular);
                }

                window.draw(choiceTexts[i]);
            }

            // 3) 属性栏
            window.draw(statsBox);
            window.draw(statsText);

            window.display();
        }
    }

} // namespace CampusSim

int main() {
    std::cout << "这是最新版本CampusSim" << std::endl;
    CampusSim::run();
    return 0;
}
