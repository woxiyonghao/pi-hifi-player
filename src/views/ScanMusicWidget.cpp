#include "views/ScanMusicWidget.hpp"
#include "imgui.h"
#include "public/AppConfig.hpp"
#include "public/Font.hpp"
#include "public/UIConfig.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <tools/PlayerAdmin.hpp>
ScanMusicWidget::ScanMusicWidget() = default;

namespace {

// ==============================================================================
// 1. 规定的格式展示顺序与板块元数据配置
// ==============================================================================
inline constexpr AudioFormat FORMAT_ORDER[] = {AudioFormat::DSD_DSF, AudioFormat::DSD_DFF, AudioFormat::ALAC,
                                               AudioFormat::FLAC,    AudioFormat::WAV,     AudioFormat::MP3,
                                               AudioFormat::UNKNOWN};
struct SectionMeta {
    const char* title;       // 板块大标题 (如 "DSD 发烧母带 (DSF)")
    const char* subtitle;    // Apple 风格小标题 (如 "SACD 1-bit Direct Stream")
    const char* default_tag; // GridItem 顶层默认标签 (如 "1-bit 纯模拟", "Studio Master")
};
inline SectionMeta getSectionMeta(AudioFormat fmt) {
    switch (fmt) {
    case AudioFormat::DSD_DSF:
        return {"DSD-DSF", "SACD-1-bit", "DSD Direct"};
    case AudioFormat::DSD_DFF:
        return {"DSD-DFF", "SACD", "1-bit Stream"};
    case AudioFormat::ALAC:
        return {"Apple-ALAC", "Apple-Lossless-Audio-Codec ", "Apple 无损"};
    case AudioFormat::FLAC:
        return {"FLAC ", "Free-Lossless-24bit/192kHz ", "Studio Master"};
    case AudioFormat::WAV:
        return {"WAV", "Uncompressed-PCM", "Uncompressed"};
    case AudioFormat::MP3:
        return {"MP3", "MPEG-Audio", "流行热播"};
    default:
        return {"UNKNOWED", "", "本地音频"};
    }
}

// ==============================================================================
// [TODO: UI验证临时模拟数据 - 验证完毕后可整块删除] START
// ==============================================================================
#define ENABLE_UI_MOCK_DATA 0

#if ENABLE_UI_MOCK_DATA
struct MockItemDef {
    const char* title;
    const char* artist;
    const char* album;
    uint32_t sample_rate;
    uint8_t bit_depth;
};

inline void injectMockTracksIfEmpty(std::map<AudioFormat, std::vector<Track>>& format_buckets) {
    // 1. DSD_DSF (20 条发烧母带曲目)
    static const MockItemDef kDsdDsfMocks[20] = {
        {"Symphony No. 9 in D Minor, Op. 125", "Herbert von Karajan", "Beethoven: The 9 Symphonies", 2822400, 1},
        {"So What", "Miles Davis", "Kind of Blue (SACD)", 2822400, 1},
        {"Hotel California (Acoustic Live)", "Eagles", "Hell Freezes Over (DSD)", 5644800, 1},
        {"The Four Seasons: Winter", "Itzhak Perlman", "Vivaldi: Le Quattro Stagioni", 2822400, 1},
        {"Cello Suite No. 1 in G Major", "Yo-Yo Ma", "Bach: The 6 Unaccompanied Suites", 2822400, 1},
        {"Autumn Leaves", "Cannonball Adderley", "Somethin' Else (SACD)", 2822400, 1},
        {"Take Five", "The Dave Brubeck Quartet", "Time Out (DSD Direct)", 5644800, 1},
        {"Clair de Lune", "Alexis Weissenberg", "Debussy: Piano Works", 2822400, 1},
        {"Blue in Green", "Bill Evans Trio", "Portrait in Jazz (DSD)", 2822400, 1},
        {"Nocturne in E-flat Major, Op. 9 No. 2", "Arthur Rubinstein", "Chopin: The Nocturnes", 2822400, 1},
        {"Round Midnight", "Thelonious Monk", "The Best of Blue Note SACD", 2822400, 1},
        {"Sultans of Swing", "Dire Straits", "Dire Straits (SACD Remaster)", 5644800, 1},
        {"Canon in D Major", "Trevor Pinnock", "Pachelbel: Baroque Favorites", 2822400, 1},
        {"My Favorite Things", "John Coltrane", "My Favorite Things (DSD)", 2822400, 1},
        {"Adagio for Strings, Op. 11", "Leonard Bernstein", "Barber: Adagio", 2822400, 1},
        {"What a Wonderful World", "Louis Armstrong", "Pure Gold (SACD)", 2822400, 1},
        {"The Girl from Ipanema", "Stan Getz & Astrud Gilberto", "Getz/Gilberto (DSD 128x)", 5644800, 1},
        {"Goldberg Variations: Aria", "Glenn Gould", "The Goldberg Variations (1981)", 2822400, 1},
        {"St. Thomas", "Sonny Rollins", "Saxophone Colossus (DSD)", 2822400, 1},
        {"Swan Lake Suite, Op. 20a", "André Previn", "Tchaikovsky: Ballets", 2822400, 1}
    };

    // 2. DSD_DFF (20 条发烧直接流曲目)
    static const MockItemDef kDsdDffMocks[20] = {
        {"1-Bit Direct Stream Master 01", "Stereo Sound Reference", "Super Audio CD Sampler Vol.1", 2822400, 1},
        {"Brandenburg Concerto No. 3 in G", "Karl Richter", "Bach: Brandenburg Concertos", 2822400, 1},
        {"A Love Supreme, Pt. I", "John Coltrane", "A Love Supreme (DFF)", 2822400, 1},
        {"Boléro, M. 81", "Charles Dutoit", "Ravel: Orchestral Works", 2822400, 1},
        {"Waltz for Debby (Live)", "Bill Evans Trio", "Waltz for Debby (SACD DFF)", 2822400, 1},
        {"Piano Concerto No. 21: Andante", "Murray Perahia", "Mozart: Piano Concertos", 2822400, 1},
        {"Time After Time", "Chet Baker", "Chet (DFF Master)", 2822400, 1},
        {"Gymnopédie No. 1", "Aldo Ciccolini", "Satie: Piano Works", 2822400, 1},
        {"Cantate Domino (Pipe Organ)", "Torsten Nilsson", "Proprius Audiophile Benchmark", 2822400, 1},
        {"Flamenco A Go-Go", "Steve Stevens", "Acoustic Guitar Showcase", 2822400, 1},
        {"Peer Gynt: Morning Mood", "Herbert von Karajan", "Grieg: Peer Gynt", 2822400, 1},
        {"Night Train", "The Oscar Peterson Trio", "Night Train (1-bit DFF)", 2822400, 1},
        {"Moonlight Sonata: Adagio", "Wilhelm Kempff", "Beethoven: Piano Sonatas", 2822400, 1},
        {"Cheek to Cheek", "Ella Fitzgerald & Louis Armstrong", "Ella and Louis (DFF)", 2822400, 1},
        {"Symphony No. 5: Allegro", "Carlos Kleiber", "Beethoven: Symphonies Nos. 5 & 7", 2822400, 1},
        {"Moanin'", "Art Blakey & The Jazz Messengers", "Moanin' (SACD DFF)", 2822400, 1},
        {"The Planets: Jupiter", "Sir Colin Davis", "Holst: The Planets", 2822400, 1},
        {"Besame Mucho", "Andrea Bocelli", "Amore (Audiophile DSD)", 2822400, 1},
        {"The Carnival of the Animals: The Swan", "Jacqueline du Pré", "Saint-Saëns Masterpieces", 2822400, 1},
        {"Fanfare for the Common Man", "Eiji Oue", "Copland: 100", 2822400, 1}
    };

    // 3. ALAC (20 条 Apple Lossless 曲目)
    static const MockItemDef kAlacMocks[20] = {
        {"Don't Know Why", "Norah Jones", "Come Away With Me (Apple Lossless)", 96000, 24},
        {"Come Together", "The Beatles", "Abbey Road (Apple Digital Master)", 96000, 24},
        {"Rolling in the Deep", "Adele", "21 (ALAC Lossless)", 44100, 16},
        {"Get Lucky", "Daft Punk ft. Pharrell", "Random Access Memories (ALAC 96/24)", 96000, 24},
        {"Dreams", "Fleetwood Mac", "Rumours (Apple Lossless)", 96000, 24},
        {"Billie Jean", "Michael Jackson", "Thriller (Apple Digital Master)", 96000, 24},
        {"Fast Car", "Tracy Chapman", "Tracy Chapman (ALAC)", 44100, 16},
        {"The Sound of Silence", "Simon & Garfunkel", "Wednesday Morning, 3 A.M.", 96000, 24},
        {"Shape of You", "Ed Sheeran", "÷ (Divide) [Apple Lossless]", 44100, 24},
        {"Blinding Lights", "The Weeknd", "After Hours (Spatial Audio Ready)", 48000, 24},
        {"Like a Rolling Stone", "Bob Dylan", "Highway 61 Revisited (ALAC)", 96000, 24},
        {"Hallelujah", "Jeff Buckley", "Grace (Apple Lossless Remaster)", 44100, 16},
        {"Smooth Operator", "Sade", "Diamond Life (ALAC Lossless)", 44100, 16},
        {"Stay With Me", "Sam Smith", "In the Lonely Hour (ALAC 96k)", 96000, 24},
        {"Thinking Out Loud", "Ed Sheeran", "x (Multiply) [Apple Master]", 44100, 24},
        {"Shallow", "Lady Gaga & Bradley Cooper", "A Star Is Born Soundtrack", 48000, 24},
        {"Bad Guy", "Billie Eilish", "WHEN WE ALL FALL ASLEEP", 44100, 24},
        {"Something", "The Beatles", "Abbey Road (Apple Digital Master)", 96000, 24},
        {"Gravity", "John Mayer", "Continuum (Apple Lossless)", 44100, 16},
        {"Watermelon Sugar", "Harry Styles", "Fine Line (ALAC 48/24)", 48000, 24}
    };

    // 4. FLAC (20 条发烧母带无损曲目)
    static const MockItemDef kFlacMocks[20] = {
        {"Hotel California (Hi-Res 192k)", "Eagles", "Hotel California (2013 Remaster)", 192000, 24},
        {"Midnight Sugar (Trio Acoustic)", "Tsuyoshi Yamamoto Trio", "Midnight Sugar (Three Blind Mice)", 192000, 24},
        {"The Look of Love", "Diana Krall", "The Look of Love (Verve 24/96)", 96000, 24},
        {"Spanish Harlem", "Rebecca Pidgeon", "The Raven (Chesky Audiophile)", 192000, 24},
        {"Tin Pan Alley", "Stevie Ray Vaughan", "Couldn't Stand the Weather", 96000, 24},
        {"Keith Don't Go (Acoustic Live)", "Nils Lofgren", "Acoustic Live (FLAC 192/24)", 192000, 24},
        {"Bird on a Wire", "Jennifer Warnes", "Famous Blue Raincoat (24/96)", 96000, 24},
        {"Autumn in Seattle", "Tsuyoshi Yamamoto Trio", "Autumn in Seattle (TBM FLAC)", 192000, 24},
        {"Brothers in Arms", "Dire Straits", "Brothers in Arms (Hi-Res FLAC)", 96000, 24},
        {"A Taste of Honey", "Patricia Barber", "Café Blue (Unmastered 24/192)", 192000, 24},
        {"Little Wing", "Stevie Ray Vaughan", "The Sky Is Crying (FLAC)", 96000, 24},
        {"Fever", "Chie Ayado", "Natural (Japanese Audiophile Vocal)", 96000, 24},
        {"Grandma's Hands", "Livingston Taylor", "Ink (Chesky Records 192k)", 192000, 24},
        {"An Evening in Paris", "Jacintha", "Here's to Ben (Groove Note)", 96000, 24},
        {"Ain't No Sunshine", "Bill Withers", "Just as I Am (FLAC 24/96)", 96000, 24},
        {"Hallelujah (Acoustic Studio)", "K.D. Lang", "Recollection (Hi-Res 192k)", 192000, 24},
        {"Tears in Heaven", "Eric Clapton", "Unplugged (FLAC 96/24)", 96000, 24},
        {"A Case of You", "Joni Mitchell", "Blue (Hi-Res 192kHz/24bit)", 192000, 24},
        {"Way Down Deep", "Jennifer Warnes", "The Hunter (Audiophile FLAC)", 96000, 24},
        {"Walk on the Wild Side", "Lou Reed", "Transformer (FLAC 96/24)", 96000, 24}
    };

    // 5. WAV (20 条未压缩 PCM 母带曲目)
    static const MockItemDef kWavMocks[20] = {
        {"Uncompressed Reference Track 01", "Telarc Classical Collection", "Telarc Digital Soundstage WAV", 96000, 24},
        {"Dark Side of the Moon: Money", "Pink Floyd", "Dark Side of the Moon (Original PCM)", 44100, 16},
        {"The Great Gate of Kiev", "Lorin Maazel & Cleveland Orch", "Mussorgsky: Pictures at an Exhibition", 96000, 24},
        {"1812 Overture: Finale (Live Cannons)", "Erich Kunzel & Cincinnati Pops", "Tchaikovsky: 1812 (Audiophile PCM)", 96000, 24},
        {"Time", "Pink Floyd", "Dark Side of the Moon (WAV Master)", 44100, 16},
        {"Ride of the Valkyries", "Sir Georg Solti", "Wagner: Der Ring des Nibelungen", 44100, 16},
        {"Smoke on the Water (Studio Monitor)", "Deep Purple", "Machine Head (Direct WAV)", 96000, 24},
        {"Stairway to Heaven", "Led Zeppelin", "Led Zeppelin IV (Master PCM)", 44100, 16},
        {"Also sprach Zarathustra: Intro", "Herbert von Karajan", "Strauss: Tone Poems (WAV)", 96000, 24},
        {"Rhapsody in Blue", "Leonard Bernstein & NY Phil", "Gershwin: Rhapsody in Blue", 44100, 16},
        {"Comfortably Numb", "Pink Floyd", "The Wall (Uncompressed WAV)", 44100, 16},
        {"Symphonie Fantastique: Scaffold", "Charles Munch", "Berlioz: Symphonie Fantastique", 44100, 16},
        {"Black Dog", "Led Zeppelin", "Led Zeppelin IV (Master WAV)", 44100, 16},
        {"Carmina Burana: O Fortuna", "Eugen Jochum", "Orff: Carmina Burana (Reference WAV)", 96000, 24},
        {"Wish You Were Here", "Pink Floyd", "Wish You Were Here (WAV)", 44100, 16},
        {"Danse Macabre, Op. 40", "David Zinman", "Saint-Saëns: Orchestral Works", 96000, 24},
        {"Kashmir", "Led Zeppelin", "Physical Graffiti (PCM Master)", 44100, 16},
        {"Scheherazade: Sinbad's Ship", "Fritz Reiner & Chicago Symphony", "Rimsky-Korsakov: Scheherazade", 96000, 24},
        {"The Chain", "Fleetwood Mac", "Rumours (Uncompressed WAV)", 44100, 16},
        {"Brahms: Hungarian Dance No. 5", "Claudio Abbado", "Brahms: 21 Hungarian Dances", 96000, 24}
    };

    auto fill_mock = [&](AudioFormat fmt, const MockItemDef defs[20], uint64_t id_base) {
        auto& list = format_buckets[fmt];
        if (!list.empty()) return; // 若本地已真实扫描出该格式则保留真实数据
        list.reserve(20);
        for (int i = 0; i < 20; ++i) {
            Track t;
            t.id = id_base + i + 1;
            t.format = fmt;
            t.title = defs[i].title;
            t.artist = defs[i].artist;
            t.album = defs[i].album;
            t.sample_rate = defs[i].sample_rate;
            t.bit_depth = defs[i].bit_depth;
            t.duration_sec = 210 + i * 15;
            list.push_back(std::move(t));
        }
    };

    fill_mock(AudioFormat::DSD_DSF, kDsdDsfMocks, 10000);
    fill_mock(AudioFormat::DSD_DFF, kDsdDffMocks, 11000);
    fill_mock(AudioFormat::ALAC,    kAlacMocks,    12000);
    fill_mock(AudioFormat::FLAC,    kFlacMocks,    13000);
    fill_mock(AudioFormat::WAV,     kWavMocks,     14000);
}
#endif
// ==============================================================================
// [TODO: UI验证临时模拟数据 - 验证完毕后可整块删除] END
// ==============================================================================
} // namespace

void ScanMusicWidget::drawSearchIcon(ImDrawList* dl, ImVec2 center, float radius, float offset_x, float offset_y,
                                     bool is_scanning) {
    // 叠加 2D 上下左右平滑巡游偏移量
    ImVec2 pos(center.x + offset_x, center.y + offset_y);

    // 放大镜透镜中心稍微偏左上方，确保 45° 手柄朝向右下方自然平衡
    ImVec2 lens_c(pos.x - radius * 0.2f, pos.y - radius * 0.2f);
    float lens_r = radius * 0.62f;

    // 1. 镜片半透明微光底板 (通透液态玻璃质感)
    dl->AddCircleFilled(lens_c, lens_r - 2.0f, IM_COL32(255, 255, 255, 12), 48);

    // 2. 外部主镜框 (主题色玫瑰红 + 柔和外发光光圈)
    dl->AddCircle(lens_c, lens_r + 2.5f, IM_COL32(250, 45, 72, 50), 48, 2.0f); // 柔和外发光
    dl->AddCircle(lens_c, lens_r, UIConfig::Color::Accent, 48, 3.5f);          // 玫瑰红金属镜圈

    // 3. 镜片弧光反射 (左上圆弧高光，呈现晶莹剔透感)
    dl->PathArcTo(lens_c, lens_r - 6.0f, -2.4f, -0.9f, 16);
    dl->PathStroke(IM_COL32(255, 255, 255, 140), 0, 2.0f);

    // 4. 镜内探索引导小圆 (渲染主题色玫瑰红 + 柔和内发光)
    dl->AddCircleFilled(lens_c, lens_r * 0.38f, IM_COL32(250, 45, 72, 35), 32);
    dl->AddCircle(lens_c, lens_r * 0.38f, UIConfig::Color::Accent, 32, 2.0f);

    // 扫描态特有增强动效：镜片内部雷达探照波与旋转光针
    if (is_scanning) {
        // 雷达扩散脉冲波
        float ping_r = std::fmod(anim_timer_ * 28.0f, lens_r * 0.75f);
        int ping_alpha = static_cast<int>((1.0f - (ping_r / (lens_r * 0.75f))) * 160.0f);
        dl->AddCircle(lens_c, ping_r, IM_COL32(250, 45, 72, ping_alpha), 24, 1.2f);

        // 旋转扫描光线 (雷达声纳指针)
        float sweep_ang = anim_timer_ * 5.5f;
        ImVec2 sweep_tip(lens_c.x + std::cos(sweep_ang) * (lens_r * 0.72f),
                         lens_c.y + std::sin(sweep_ang) * (lens_r * 0.72f));
        dl->AddLine(lens_c, sweep_tip, UIConfig::Color::Accent, 1.8f);
    }

    // 5. 45度斜向手柄 (指向右下方，圆润手感)
    const float cos45 = 0.7071f;
    const float sin45 = 0.7071f;
    ImVec2 h_start(lens_c.x + lens_r * cos45, lens_c.y + lens_r * sin45);
    ImVec2 h_end(h_start.x + radius * 0.62f, h_start.y + radius * 0.62f);

    // 手柄本体
    dl->AddLine(h_start, h_end, UIConfig::Color::Accent, 5.5f);
    dl->AddCircleFilled(h_end, 2.75f, UIConfig::Color::Accent, 16); // 手柄末端平滑半圆

    // 手柄背脊微光高光线
    dl->AddLine(ImVec2(h_start.x + 1.2f, h_start.y + 1.2f), ImVec2(h_end.x - 2.5f, h_end.y - 2.5f),
                IM_COL32(255, 255, 255, 90), 1.8f);
}

void ScanMusicWidget::render(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, std::vector<Playlist>& playlists) {
    anim_timer_ += 0.035f; // 约 60fps 时间步长推进

    // 计算卡片中心点
    ImVec2 center((p_min.x + p_max.x) * 0.5f, (p_min.y + p_max.y) * 0.5f);

    ScanState state = MusicScanManager::getInstance().getState();

    // 状态机分发
    if (state == ScanState::Scanning) {
        renderScanningState(dl, p_min, p_max, center);
    } else if (state == ScanState::Completed) {
        renderCompletedState(dl, p_min, p_max, playlists);
    } else {
        // Idle 或 Cancelled/Failed 状态均呈现样式 1 (待机态)
        renderIdleState(dl, center, playlists);
    }
}

void ScanMusicWidget::renderIdleState(ImDrawList* dl, ImVec2 center,
                                      [[maybe_unused]] std::vector<Playlist>& playlists) {
    // 1. 2D 上下左右平滑巡游动效：利用不同频率的正弦/余弦实现柔和的多维空间漫游漂浮
    float offset_x = std::cos(anim_timer_ * 1.5f) * 12.0f;
    float offset_y = std::sin(anim_timer_ * 2.2f) * 9.0f;

    // 绘制大号 🔍 矢量放大镜 (半径 48px，位置在中心偏上，消除中间文本留出呼吸空间)
    ImVec2 icon_center(center.x, center.y - 35.0f);
    drawSearchIcon(dl, icon_center, 48.0f, offset_x, offset_y);

    // 2. 居中发烧级胶囊扫描按钮
    const float btn_w = 190.0f;
    const float btn_h = 42.0f;
    const float btn_rounding = btn_h * 0.5f; // 21px 纯半圆胶囊

    ImVec2 btn_p0(center.x - btn_w * 0.5f, center.y + 60.0f);
    ImVec2 btn_p1(btn_p0.x + btn_w, btn_p0.y + btn_h);

    // 判定鼠标悬停与点击状态
    bool is_hovered = ImGui::IsMouseHoveringRect(btn_p0, btn_p1);
    bool is_clicked = is_hovered && ImGui::IsMouseClicked(0);

    // 按钮渲染层次结构：
    // Blur 态：具有清晰半透明 Alpha 质感，无外发光，不抢视觉重心
    // Onhover 态：完全对齐左侧边栏「扫描音乐」主题色胶囊，环境辉光绽放 + 1px 折射微光边
    if (is_hovered) {
        // ---------------- Hover 态 (onhover) ----------------
        // 1. 发光辉光层 (双层微光扩散)
        dl->AddRectFilled(ImVec2(btn_p0.x - 5.0f, btn_p0.y - 5.0f), ImVec2(btn_p1.x + 5.0f, btn_p1.y + 5.0f),
                          IM_COL32(250, 45, 72, 30), btn_rounding + 4.0f);
        dl->AddRectFilled(ImVec2(btn_p0.x - 2.5f, btn_p0.y - 2.5f), ImVec2(btn_p1.x + 2.5f, btn_p1.y + 2.5f),
                          IM_COL32(250, 45, 72, 60), btn_rounding + 2.0f);

        // 2. 严格对齐左侧边栏选中的主题色液态玻璃配方 (对齐后与侧边栏完全一致)
        dl->AddRectFilled(btn_p0, btn_p1, IM_COL32(250, 45, 72, 85), btn_rounding);
        dl->AddRectFilled(btn_p0, btn_p1, UIConfig::Color::GlassActive, btn_rounding); // 30 Alpha 磨砂白

        // 3. 1px 微光折射圆角边框
        dl->AddRect(btn_p0, btn_p1, UIConfig::Color::GlassBorder, btn_rounding, 0, 1.0f); // 50 Alpha 边框
    } else {
        // ---------------- Blur 态 (未悬停) ----------------
        // 1. 无外发光晕，消除光晕带来的膨胀感与过亮感
        // 2. 带有轻盈 Alpha 的半透明玫瑰红底板 + 微量通透层，清晰透出暗色背景
        dl->AddRectFilled(btn_p0, btn_p1, IM_COL32(250, 45, 72, 65), btn_rounding);
        dl->AddRectFilled(btn_p0, btn_p1, IM_COL32(255, 255, 255, 12), btn_rounding);

        // 3. 极细柔和边缘轮廓
        dl->AddRect(btn_p0, btn_p1, IM_COL32(255, 255, 255, 35), btn_rounding, 0, 1.0f);
    }

    ImU32 text_col = is_hovered ? UIConfig::Color::TextActive   // 悬停时：纯白 100%
                                : IM_COL32(215, 222, 235, 210); // 未悬停时：柔和浅白 (带适度透感)

    // 按钮内部居中文本
    const char* btn_label = "全盘检索";
    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    ImVec2 label_sz = ImGui::CalcTextSize(btn_label);
    dl->AddText(ImVec2(btn_p0.x + (btn_w - label_sz.x) * 0.5f, btn_p0.y + (btn_h - label_sz.y) * 0.5f), text_col,
                btn_label);
    if (Fonts::Regular) ImGui::PopFont();

    // 点击启动异步扫描，状态将自动切入 Scanning！
    if (is_clicked) {
        std::error_code ec;
        const std::string& scan_path = AppConfig::Path::getMusicDir();
        if (!std::filesystem::exists(scan_path, ec)) {
            std::filesystem::create_directories(scan_path, ec);
        }
        MusicScanManager::getInstance().startScan(scan_path);
    }
}

namespace {

// 绘制高保真纯净太阳光球与多层辐射日冕 (纯净球形光晕，去除多余白色线条)
void drawSmoothSolarSphere(ImDrawList* dl, ImVec2 center, float pulse) {
    // 1. 广域深空外日冕柔和微光辉晕 (由 36px 向外平滑渐隐入深空背景，填补中心区域)
    dl->AddCircleFilled(center, 36.0f * pulse, IM_COL32(250, 45, 72, 20), 48);
    dl->AddCircleFilled(center, 28.0f * pulse, IM_COL32(250, 45, 72, 45), 48);
    dl->AddCircleFilled(center, 21.0f * pulse, IM_COL32(250, 45, 72, 85), 48);
    dl->AddCircleFilled(center, 15.0f * pulse, IM_COL32(250, 45, 72, 140), 48);

    // 2. 高温过渡色球层 (由玫瑰红经由暖粉过渡至白炽)
    dl->AddCircleFilled(center, 11.0f * pulse, IM_COL32(255, 120, 145, 190), 48);
    dl->AddCircleFilled(center, 7.5f * pulse, IM_COL32(255, 185, 205, 230), 40);
    dl->AddCircleFilled(center, 5.0f * pulse, IM_COL32(255, 235, 245, 250), 36);

    // 3. 极热白炽恒星核 (100% 纯净白炽圆球)
    dl->AddCircleFilled(center, 3.2f * pulse, IM_COL32(255, 255, 255, 255), 32);
}

// 绘制纯矢量 32px 刷新图标 (1:1 还原截图二逆时针带圆头循环箭头)
void drawRefreshIcon(ImDrawList* dl, ImVec2 center, float size, ImU32 col, float rotation_rad = 0.0f) {
    const float arc_r = size * 0.25f; // 半径约 8px
    const float thickness = 2.2f;

    // 旋转偏移量 (悬停动效或静态 0)
    float base_a_min = -1.95f + rotation_rad;
    float base_a_max = 2.45f + rotation_rad;

    // 1. 252° 圆弧段
    dl->PathArcTo(center, arc_r, base_a_min, base_a_max, 28);
    dl->PathStroke(col, 0, thickness);

    // 2. 尾端平滑圆头 (7:30 位置)
    ImVec2 tail_pos(center.x + arc_r * std::cos(base_a_max), center.y + arc_r * std::sin(base_a_max));
    dl->AddCircleFilled(tail_pos, thickness * 0.5f, col, 12);

    // 3. 顶端逆时针箭头 (10:30 位置，指向左下方)
    float head_ang = base_a_min;
    ImVec2 head_pos(center.x + arc_r * std::cos(head_ang), center.y + arc_r * std::sin(head_ang));

    // 切线向量 (逆时针朝向) 与法线向量 (径向向外)
    float cos_a = std::cos(head_ang);
    float sin_a = std::sin(head_ang);
    ImVec2 dir_tan(sin_a, -cos_a);   // 逆时针切线
    ImVec2 dir_norm(cos_a, sin_a);  // 径向外法线

    const float arrow_len = 5.2f;
    const float arrow_half_w = 3.6f;

    ImVec2 tip(head_pos.x + dir_tan.x * (arrow_len * 0.65f),
               head_pos.y + dir_tan.y * (arrow_len * 0.65f));
    ImVec2 back_c(head_pos.x - dir_tan.x * (arrow_len * 0.35f),
                  head_pos.y - dir_tan.y * (arrow_len * 0.35f));
    ImVec2 w1(back_c.x + dir_norm.x * arrow_half_w, back_c.y + dir_norm.y * arrow_half_w);
    ImVec2 w2(back_c.x - dir_norm.x * arrow_half_w, back_c.y - dir_norm.y * arrow_half_w);

    dl->AddTriangleFilled(tip, w1, w2, col);
}

} // anonymous namespace

void ScanMusicWidget::drawLaserWarpAnimation(ImDrawList* dl, ImVec2 emitter_pos, ImVec2 p_min, ImVec2 p_max) {
    // 裁剪在卡片矩形内部，防止满屏激光与粒子溢出主舞台卡片
    dl->PushClipRect(p_min, p_max, true);

    // 计算从发射中心到卡片最远顶角的物理距离，确保 100% 满屏无死角穿透至四角与边缘
    float max_dist = std::hypot(std::max(emitter_pos.x - p_min.x, p_max.x - emitter_pos.x),
                                std::max(emitter_pos.y - p_min.y, p_max.y - emitter_pos.y)) +
                     40.0f;

    // =========================================================================
    // 1. 密集发丝级径向星芒激光流 (360 束，严格保持原本的直线曲速跃迁动画路线)
    // =========================================================================
    constexpr int NUM_RAYS = 360;
    for (int i = 0; i < NUM_RAYS; ++i) {
        // 黄金分割角 (137.5°) 均匀环形铺展
        float angle = i * 2.3999632f;

        // 确定性随机相位与多频流动速度
        float phase = std::fmod((float)(i * 17 + 31) * 0.0137f, 1.0f);
        float speed = 0.18f + std::fmod((float)(i * 7) * 0.019f, 0.12f);
        float progress = std::fmod(anim_timer_ * speed + phase, 1.0f);

        // 连续长光束延伸 (原本的直尺径向发射轨迹)
        float r_start = 18.0f + progress * 50.0f;
        float r_end = std::min(max_dist, r_start + 80.0f + (progress * progress) * (max_dist - 80.0f));

        float x1 = emitter_pos.x + std::cos(angle) * r_start;
        float y1 = emitter_pos.y + std::sin(angle) * (r_start * 0.90f);
        float x2 = emitter_pos.x + std::cos(angle) * r_end;
        float y2 = emitter_pos.y + std::sin(angle) * (r_end * 0.90f);

        // 多层通透微光：大部分为细腻幽光 (Alpha 25~120)，少数高亮主光束 (Alpha 180~220)
        int alpha = static_cast<int>(25.0f + progress * 95.0f);
        if (i % 8 == 0) {
            alpha = std::min(225, alpha + 90);
        }

        // 1.0px 发丝级细腻线条
        dl->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), IM_COL32(250, 45, 72, alpha), 1.0f);

        // 高亮主光束核心叠加入射白炽微光
        if (alpha > 150) {
            ImVec2 mid(x1 + (x2 - x1) * 0.5f, y1 + (y2 - y1) * 0.5f);
            dl->AddLine(mid, ImVec2(x2, y2), IM_COL32(255, 210, 225, static_cast<int>(alpha * 0.65f)), 1.0f);
        }
    }

    // =========================================================================
    // 2. 漫天星尘与闪烁十字星芒粒子系统 (240 颗，严格保持原本的径向飞跃路线)
    // =========================================================================
    constexpr int NUM_PARTICLES = 240;
    for (int j = 0; j < NUM_PARTICLES; ++j) {
        float p_angle = (float)(j * 2.3999632f) + std::fmod((float)(j * 13) * 0.021f, 0.25f);
        float p_phase = std::fmod((float)(j * 29 + 11) * 0.0173f, 1.0f);
        float p_speed = 0.16f + std::fmod((float)(j * 11) * 0.013f, 0.10f);
        float p_prog = std::fmod(anim_timer_ * p_speed + p_phase, 1.0f);

        float p_curve = p_prog * p_prog; // 二次加速，呈现远小近大的空间透视
        float p_r = 16.0f + p_curve * (max_dist - 16.0f);

        float px = emitter_pos.x + std::cos(p_angle) * p_r;
        float py = emitter_pos.y + std::sin(p_angle) * (p_r * 0.90f);

        // 渐入渐出透明度包络
        float fade = (p_prog < 0.12f) ? (p_prog / 0.12f) : (1.0f - p_curve * 0.65f);
        int p_alpha = std::clamp(static_cast<int>(fade * 255.0f), 0, 255);
        float p_size = 0.8f + p_curve * 2.2f;

        int p_type = j % 12;
        if (p_type < 7) {
            // (1) 普通深空微光星尘：细微点缀
            dl->AddCircleFilled(ImVec2(px, py), p_size, IM_COL32(255, 195, 210, p_alpha), 10);
        } else if (p_type < 10) {
            // (2) 带有主题色柔和光晕的恒星粒子：双层微发光
            float halo_r = p_size * 2.6f;
            dl->AddCircleFilled(ImVec2(px, py), halo_r, IM_COL32(250, 45, 72, static_cast<int>(p_alpha * 0.35f)), 16);
            dl->AddCircleFilled(ImVec2(px, py), p_size, IM_COL32(255, 255, 255, p_alpha), 12);
        } else {
            // (3) 星球大战原版同款 4 芒十字衍射星芒 (Cross Star Flares)
            float flare_len = 4.0f + p_curve * 10.0f;
            dl->AddCircleFilled(ImVec2(px, py), p_size * 2.0f, IM_COL32(250, 45, 72, static_cast<int>(p_alpha * 0.45f)),
                                16);
            dl->AddLine(ImVec2(px - flare_len, py), ImVec2(px + flare_len, py),
                        IM_COL32(255, 235, 245, static_cast<int>(p_alpha * 0.85f)), 1.0f);
            dl->AddLine(ImVec2(px, py - flare_len), ImVec2(px, py + flare_len),
                        IM_COL32(255, 235, 245, static_cast<int>(p_alpha * 0.85f)), 1.0f);
            dl->AddCircleFilled(ImVec2(px, py), 1.5f, IM_COL32(255, 255, 255, 255), 8);
        }
    }

    // =========================================================================
    // 3. 核心太阳光球 (纯 GPU 硬件顶点色连续插值渐变，0 色阶硬边，与深空自然融合)
    // =========================================================================
    float pulse = 1.0f + 0.03f * std::sin(anim_timer_ * 2.5f);
    drawSmoothSolarSphere(dl, emitter_pos, pulse);

    dl->PopClipRect();
}

void ScanMusicWidget::renderScanningState(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, ImVec2 center) {
    // 根据用户明确指示：搜索中除了背景动画，其他的全部不要（包括放大镜、状态文字、计数、终止按钮），连终止也不需要
    // 呈现完全纯净、满屏通透沉浸的太阳日冕等离子背景动画
    drawLaserWarpAnimation(dl, center, p_min, p_max);
}

void ScanMusicWidget::drawSectionHeader(ImDrawList* dl, ImVec2 pos, const char* title, const char* subtitle,
                                        size_t track_count) {
    float cur_y = pos.y;

    if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
    dl->AddText(ImVec2(pos.x, cur_y), UIConfig::Color::TextActive, title);
    if (Fonts::Medium) ImGui::PopFont();
    cur_y += 24.0f;
    std::string sub_str = std::string(subtitle) + " · 共 " + std::to_string(track_count) + " 首";
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(pos.x, cur_y), UIConfig::Color::TextMuted, sub_str.c_str());
    if (Fonts::Small) ImGui::PopFont();
}

void ScanMusicWidget::renderCompletedState([[maybe_unused]] ImDrawList* dl, ImVec2 p_min, ImVec2 p_max,
                                           std::vector<Playlist>& playlists) {
    auto& scanner = MusicScanManager::getInstance();
    const auto scanned_tracks = scanner.getScannedTracks();

    // 1. 按格式分流曲目
    std::map<AudioFormat, std::vector<Track>> format_buckets;
    for (const auto& track : scanned_tracks) {
        format_buckets[track.format].push_back(track);
    }

#if ENABLE_UI_MOCK_DATA
    // [TODO: UI验证临时模拟数据] 注入除 MP3 外每种发烧音频格式 20 条模拟数据，供多行纵向与横向滚动排版验证
    injectMockTracksIfEmpty(format_buckets);
#endif

    const float pad_x = 24.0f;
    const float pad_y = 18.0f;
    const float content_w = (p_max.x - p_min.x) - pad_x * 2.0f;
    const float content_h = (p_max.y - p_min.y) - pad_y * 2.0f;
    const float card_w = MusicItem::DefaultWidth;
    const float card_h = MusicItem::DefaultHeight;
    const float item_gap_x = 16.0f;
    const float section_gap_y = 36.0f;

    ImGui::SetCursorScreenPos(ImVec2(p_min.x + pad_x, p_min.y + pad_y));

    // 开启主纵向滚动容器
    ImGui::BeginChild("##ScanCompletedScrollRoot", ImVec2(content_w, content_h), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);

    // 按照指定格式顺序依次渲染各个板块
    for (AudioFormat fmt : FORMAT_ORDER) {
        auto it = format_buckets.find(fmt);
        if (it == format_buckets.end() || it->second.empty()) continue;

        const auto& tracks_in_fmt = it->second;
        SectionMeta meta = getSectionMeta(fmt);

        // Header
        ImVec2 header_pos = ImGui::GetCursorScreenPos();
        drawSectionHeader(ImGui::GetWindowDrawList(), header_pos, meta.title, meta.subtitle, tracks_in_fmt.size());
        ImGui::Dummy(ImVec2(0.0f, 48.0f));

        // 卡片横向滚动行
        std::string row_id = "##FormatRow_" + std::to_string(static_cast<int>(fmt));

        // 1. 显式告知 ImGui 此行的实际总内容宽度，确保滚动范围绝对精准且在首帧立即可用
        float total_row_w = static_cast<float>(tracks_in_fmt.size()) * (card_w + item_gap_x);
        ImGui::SetNextWindowContentSize(ImVec2(total_row_w, 0.0f));

        // 2. 将滚动条尺寸临时推入为 0 (完全隐藏横向滚动条，不绘制灰色轨道且不占高度)
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.0f);

        // 3. 启用 ImGuiWindowFlags_HorizontalScrollbar 允许横向排版与计算滚动范围
        //    添加 ImGuiWindowFlags_NoScrollWithMouse 禁止滚轮控制横向滚动，使鼠标滚轮自然冒泡向上控制纵向主页面滚动
        ImGui::BeginChild(row_id.c_str(), ImVec2(content_w, card_h + 12.0f), false,
                          ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoBackground |
                              ImGuiWindowFlags_NoScrollWithMouse);

        // 4. 支持鼠标拖拽、触控屏手指滑动 (Touch & Mouse Drag Scrolling)
        static bool is_dragging_row = false;
        static std::string active_drag_row_id = "";

        bool is_row_hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        if (is_row_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            is_dragging_row = true;
            active_drag_row_id = row_id;
        }

        if (is_dragging_row && active_drag_row_id == row_id) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                float delta_x = ImGui::GetIO().MouseDelta.x;
                if (delta_x != 0.0f) {
                    ImGui::SetScrollX(ImGui::GetScrollX() - delta_x);
                }
            } else {
                is_dragging_row = false;
                active_drag_row_id = "";
            }
        }

        for (size_t i = 0; i < tracks_in_fmt.size(); ++i) {
            if (i > 0) ImGui::SameLine(0.0f, item_gap_x);
            if (MusicItem::render(ImGui::GetWindowDrawList(), ImVec2(card_w, card_h), tracks_in_fmt[i],
                                  meta.default_tag)) {
                if (!playlists.empty()) {
                    playlists[0].addTrack(tracks_in_fmt[i]);
                }
                PlayerAdmin::getInstance().playTracks(tracks_in_fmt, i);
            }
        }
        ImGui::EndChild();

        // 6. 弹出样式变量，恢复全局设置
        ImGui::PopStyleVar();

        ImGui::Dummy(ImVec2(0.0f, section_gap_y));
    }

    ImGui::EndChild();

    // =========================================================================
    // 7. 右上角发烧级 32px 矢量刷新 (Rescan) 按钮
    // 距顶 12px，距右 12px，仅在扫描完成界面显示，配色严格适配 App 发烧液态玻璃规范
    // =========================================================================
    const float btn_size = 32.0f;
    ImVec2 btn_p0(p_max.x - 12.0f - btn_size, p_min.y + 12.0f);
    ImVec2 btn_p1(btn_p0.x + btn_size, btn_p0.y + btn_size);
    ImVec2 btn_c(btn_p0.x + btn_size * 0.5f, btn_p0.y + btn_size * 0.5f);

    // 采用屏幕坐标直接判定，杜绝由于外部 BeginChild 嵌套导致的事件命中丢失
    bool hov_refresh = ImGui::IsMouseHoveringRect(btn_p0, btn_p1, false);
    bool act_refresh = hov_refresh && ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool clicked_refresh = hov_refresh && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

    if (hov_refresh) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    // 点击刷新图标：弹出二次确认框，询问用户是否重新扫描歌曲
    if (clicked_refresh) {
        ImGui::GetIO().MouseClicked[0] = false;
        show_rescan_confirm_modal_ = true;
    }

    const ImU32 accent = UIConfig::Color::Accent;
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    if (hov_refresh || act_refresh) {
        // 悬停态：主题色环境光晕 Bloom + 玫瑰红液态玻璃底板 + 1px 折射微光边
        dl->AddCircleFilled(btn_c, 19.0f, IM_COL32(r, g, b, 45), 32);
        dl->AddCircleFilled(btn_c, 16.0f, IM_COL32(r, g, b, 85), 32);
        dl->AddCircleFilled(btn_c, 16.0f, UIConfig::Color::GlassActive, 32);
        dl->AddCircle(btn_c, 16.0f, accent, 32, 1.0f);
    } else {
        // Blur 态：深空曜石液态玻璃底板 + 极细通透光边 (适配截图二圆黑底并融入数播暗夜主题)
        dl->AddCircleFilled(btn_c, 16.0f, IM_COL32(22, 26, 36, 220), 32);
        dl->AddCircle(btn_c, 16.0f, IM_COL32(255, 255, 255, 35), 32, 1.0f);
    }

    ImU32 icon_col = (hov_refresh || act_refresh) ? IM_COL32(255, 255, 255, 255)
                                                   : IM_COL32(215, 222, 235, 190);
    float rot = hov_refresh ? -std::fmod(static_cast<float>(ImGui::GetTime()) * 4.0f, 6.2831853f) : 0.0f;
    drawRefreshIcon(dl, btn_c, btn_size, icon_col, rot);

    // =========================================================================
    // 8. 重新扫描二次确认模态弹窗
    // =========================================================================
    if (show_rescan_confirm_modal_) {
        renderRescanConfirmModal(ImVec2((p_min.x + p_max.x) * 0.5f, (p_min.y + p_max.y) * 0.5f));
    }
}

void ScanMusicWidget::renderRescanConfirmModal(ImVec2 center) {
    // 1. 全屏柔焦半透明遮罩，阻断下层鼠标与手势穿透
    ImDrawList* fg_dl = ImGui::GetForegroundDrawList();
    ImGuiIO& io = ImGui::GetIO();
    fg_dl->AddRectFilled(ImVec2(0.0f, 0.0f), io.DisplaySize, IM_COL32(0, 0, 0, 160));

    // 2. 居中模态对话框尺寸与几何排版
    const float modal_w = 400.0f;
    const float modal_h = 186.0f;
    const float modal_x = center.x - modal_w * 0.5f;
    const float modal_y = center.y - modal_h * 0.5f;

    ImGui::SetNextWindowPos(ImVec2(modal_x, modal_y));
    ImGui::SetNextWindowSize(ImVec2(modal_w, modal_h));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoCollapse;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(22, 26, 36, 252));
    ImGui::PushStyleColor(ImGuiCol_Border, UIConfig::Color::GlassBorder);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f, 20.0f));

    if (ImGui::Begin("##RescanConfirmModalDialog", nullptr, flags)) {
        // 标题
        if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "重新扫描本地歌曲");
        if (Fonts::Medium) ImGui::PopFont();

        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        // 提示说明文案
        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        ImGui::TextColored(ImVec4(0.85f, 0.88f, 0.94f, 0.95f), "确定要重新扫描本地歌曲库吗？");
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.6f, 0.65f, 0.75f, 0.85f), "重新扫描将清空当前结果，并停止当前正在播放的曲目。");
        if (Fonts::Small) ImGui::PopFont();

        ImGui::Dummy(ImVec2(0.0f, 16.0f));

        // 底部按钮：取消 / 确认扫描
        const float btn_w = 110.0f;
        const float btn_h = 34.0f;
        ImGui::SetCursorPosX(modal_w - 24.0f - btn_w * 2.0f - 12.0f);

        // 1. 取消按钮
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(40, 46, 60, 180));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(52, 60, 78, 220));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(65, 75, 96, 250));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        if (ImGui::Button("取消", ImVec2(btn_w, btn_h)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            show_rescan_confirm_modal_ = false;
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        ImGui::SameLine(0.0f, 12.0f);

        // 2. 确认扫描按钮
        ImGui::PushStyleColor(ImGuiCol_Button, UIConfig::Color::Accent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(255, 65, 95, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(230, 30, 60, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

        if (ImGui::Button("确认扫描", ImVec2(btn_w, btn_h)) || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            show_rescan_confirm_modal_ = false;
            PlayerAdmin::getInstance().stop();

            std::error_code ec;
            const std::string& scan_path = AppConfig::Path::getMusicDir();
            if (!std::filesystem::exists(scan_path, ec)) {
                std::filesystem::create_directories(scan_path, ec);
            }
            MusicScanManager::getInstance().startScan(scan_path);
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
    }
    ImGui::End();

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}