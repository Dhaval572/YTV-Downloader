#include <imgui.h>
#include <string>
#include <string_view>
#include <array>
#include <algorithm>
#include <thread>
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <system_error> // IWYU pragma: keep
#include "tinyfiledialogs.h"
#include "ImGuiCustomTheme.h"
using namespace std;
namespace fs = filesystem;

namespace State
{
    string status_message;
    bool is_error = false;

    atomic<bool> is_installing{false};
    atomic<bool> is_downloading{false};
    atomic<bool> should_cancel{false};
    atomic<float> install_progress{0.0f};
    string install_package;

    bool show_confirmation = false;
    string pending_url;
    string pending_format;
    string pending_path;
    string pending_filename;
    int pending_res = 1080;

    atomic<bool> is_probing{false};
    string probed_size;
}

namespace Shell
{
#ifdef _WIN32
    constexpr bool kIsWindows = true;
#else
    constexpr bool kIsWindows = false;
#endif

    void SetStatus(string_view msg, bool error = false)
    {
        State::status_message = msg;
        State::is_error = error;
    }

    string Escape(string_view arg)
    {
        string result;
        result.reserve(arg.size() + 2);
        result += '"';
        for (char c : arg)
        {
            if (c == '"') result += '\\';
            result += c;
        }
        result += '"';
        return result;
    }

    string NullDevice()
    {
        return kIsWindows ? "NUL" : "/dev/null";
    }

    bool Exists(string_view cmd)
    {
        if (kIsWindows)
        {
            return system(format("where {} > {} 2>&1", cmd, NullDevice()).c_str()) == 0;
        }

        return system(format("which {} > {} 2>&1", cmd, NullDevice()).c_str()) == 0;
    }

    string GetLinuxPackageManager()
    {
        if (Exists("apt")) return "apt";
        if (Exists("dnf")) return "dnf";
        if (Exists("yum")) return "yum";
        if (Exists("pacman")) return "pacman";
        return "";
    }

    void InstallWithPackageManager(string_view pkg_name)
    {
        if (kIsWindows)
        {
            string pkg_id = (pkg_name == "ffmpeg") ? "Gyan.FFmpeg" : "yt-dlp.yt-dlp";

            if (Exists("winget"))
            {
                string cmd = format(
                    "winget install --id {} -e --silent --accept-package-agreements --accept-source-agreements",
                    pkg_id
                );
                system(format("{} > {} 2>&1", cmd, NullDevice()).c_str());
                return;
            }

            if (Exists("choco"))
            {
                string cmd = format("choco install -y {}", pkg_name);
                system(format("{} > {} 2>&1", cmd, NullDevice()).c_str());
                return;
            }

            if (Exists("scoop"))
            {
                string cmd = format("scoop install {}", pkg_name);
                system(format("{} > {} 2>&1", cmd, NullDevice()).c_str());
                return;
            }

            return;
        }

        string pm = GetLinuxPackageManager();
        string install_cmd;

        if (pm == "apt") install_cmd = format("sudo apt update && sudo apt install -y {}", pkg_name);
        else if (pm == "dnf") install_cmd = format("sudo dnf install -y {}", pkg_name);
        else if (pm == "yum") install_cmd = format("sudo yum install -y {}", pkg_name);
        else if (pm == "pacman") install_cmd = format("sudo pacman -S --noconfirm {}", pkg_name);

        if (!install_cmd.empty())
        {
            system(format("{} > {} 2>&1", install_cmd, NullDevice()).c_str());
        }
    }

    void CancelYtDlp()
    {
        if (kIsWindows)
        {
            system("taskkill /F /IM yt-dlp.exe > NUL 2>&1");
        }
        else
        {
            system("pkill -f yt-dlp > /dev/null 2>&1");
        }
    }
}

namespace Util
{
    string FormatBytes(long long bytes)
    {
        if (bytes <= 0) return "unknown";

        constexpr array<string_view, 4> units = {"B", "KB", "MB", "GB"};
        int i = 0;
        double v = static_cast<double>(bytes);

        while (v >= 1024.0 && i < 3)
        {
            v /= 1024.0;
            ++i;
        }

        return format("~{:.3f} {}", v, units[i]);
    }
}

namespace Installer
{
    void Install(string_view pkg)
    {
        State::install_package = pkg;
        State::is_installing = true;
        State::install_progress = 0.0f;

        thread([pkg = string(pkg)]()
        {
            string package_to_install;
            if (pkg == "yt-dlp.yt-dlp") package_to_install = "yt-dlp";
            else if (pkg == "ffmpeg") package_to_install = "ffmpeg";
            else package_to_install = pkg;

            Shell::SetStatus(format("Installing {}...", package_to_install));

            for (int i = 0; i < 10; i++)
            {
                this_thread::sleep_for(chrono::milliseconds(300));
                State::install_progress = (i + 1) * 0.1f;
            }

            Shell::InstallWithPackageManager(package_to_install);

            if (!Shell::Exists("yt-dlp") && package_to_install == "yt-dlp")
            {
#ifdef _WIN32
                system("py -m pip install --user -U yt-dlp > NUL 2>&1");
#else
                system("pip3 install --user yt-dlp > /dev/null 2>&1");
#endif
            }

            State::is_installing = false;
            State::install_progress = 0.0f;

            if (!Shell::Exists(package_to_install))
            {
#ifdef _WIN32
                string hint = (package_to_install == "ffmpeg")
                    ? "winget install Gyan.FFmpeg"
                    : "winget install yt-dlp.yt-dlp";
                Shell::SetStatus(format("Failed to install {}. Please install manually:\n  {}", package_to_install, hint), true);
#else
                Shell::SetStatus
                (
                    format
                    (
                        "Failed to install {}. Please install manually:\n"
                        "  Ubuntu/Debian: sudo apt install {}\n"
                        "  Fedora: sudo dnf install {}\n"
                        "  Arch: sudo pacman -S {}",
                        package_to_install, package_to_install, package_to_install, package_to_install
                    ), true
                );
#endif
            }
            else
            {
                Shell::SetStatus(format("{} installed successfully!", package_to_install));
            }
        }).detach();
    }

    void EnsureYtDlp()
    {
        if (!Shell::Exists("yt-dlp") && !State::is_installing)
        {
            Install("yt-dlp.yt-dlp");
        }
    }

    void EnsureFfmpeg()
    {
        if (!Shell::Exists("ffmpeg") && !State::is_installing)
        {
            Install("ffmpeg");
        }
    }
}

struct t_FormatOption
{
    string_view label;
    string_view ext;
    bool audio_only;
};

struct t_ResOption
{
    string_view label;
    int height;
};

static constexpr array<t_FormatOption, 8> kFormats =
{{
    {"MP4  - video", "mp4",  false},
    {"WebM - video", "webm", false},
    {"MKV  - video", "mkv",  false},
    {"MP3  - audio", "mp3",  true},
    {"M4A  - audio", "m4a",  true},
    {"WAV  - audio", "wav",  true},
    {"FLAC - audio", "flac", true},
    {"Opus - audio", "opus", true},
}};

static constexpr array<t_ResOption, 7> kResolutions =
{{
    {"Best available", 0},
    {"4K   - 2160p",   2160},
    {"2K   - 1440p",   1440},
    {"FHD  - 1080p",   1080},
    {"HD   - 720p",    720},
    {"SD   - 480p",    480},
    {"Low  - 360p",    360},
}};

namespace Ytdlp
{
    string BuildSelector(const t_FormatOption& fmt, int height)
    {
        if (fmt.audio_only) return "bestaudio/best";
        string ext(fmt.ext);

        if (height == 0)
        {
            if (ext == "mp4")
            {
                return "bv*[ext=mp4]+ba[ext=m4a]/bv*+ba/b[ext=mp4]/b/best";
            }
            if (ext == "webm")
            {
                return "bv*[ext=webm]+ba[ext=webm]/bv*+ba/b[ext=webm]/b/best";
            }
            return "bv*+ba/b/best";
        }

        string hs = to_string(height);

        if (ext == "mp4")
        {
            return format("bv*[height<={}][ext=mp4]+ba[ext=m4a]/bv*[height<={}]+ba/b[height<={}][ext=mp4]/b[height<={}]/best", hs, hs, hs, hs);
        }
        if (ext == "webm")
        {
            return format("bv*[height<={}][ext=webm]+ba[ext=webm]/bv*[height<={}]+ba/b[height<={}][ext=webm]/b[height<={}]/best", hs, hs, hs, hs);
        }

        return format("bv*[height<={}]+ba/b[height<={}]/best", hs, hs);
    }

    string BuildCommand(string_view url, const t_FormatOption& fmt, int height, string_view out_path, string_view custom_filename = "")
    {
        string out_template;

        if (!custom_filename.empty())
        {
            // Convert string_view to string for processing
            string filename(custom_filename);

            // remove invalid characters
            for (char& c : filename)
            {
                if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
                    c == '"' || c == '<' || c == '>' || c == '|')
                {
                    c = '_';
                }
            }
            out_template = format(" -o \"{}/{}.{}\"", out_path, filename, fmt.ext);
        }
        else
        {
            out_template = format(" -o \"{}/%(title)s.%(ext)s\"", out_path);
        }

        if (fmt.audio_only)
        {
            return format
            (
                "yt-dlp -x --audio-format {} --audio-quality 0{} {}",
                fmt.ext,
                out_template,
                Shell::Escape(url)
            );
        }

        return format
        (
            "yt-dlp -f \"{}\" --merge-output-format {}{} {}",
            BuildSelector(fmt, height), fmt.ext, out_template, Shell::Escape(url)
        );
    }

    void ProbeSize(string_view url, const t_FormatOption& fmt, int height)
    {
        if (State::is_probing) return;

        State::probed_size = "Probing...";
        State::is_probing = true;

        thread([url = string(url), fmt, height]()
        {
            fs::path tmp_path = fs::temp_directory_path() / "yt_size_tmp.txt";
            string cmd = format
            (
                "yt-dlp -f \"{}\" --print \"%(filesize,filesize_approx)s\" --no-download {} > {} 2>{}",
                BuildSelector(fmt, height),
                Shell::Escape(url),
                Shell::Escape(tmp_path.string()),
                Shell::NullDevice()
            );

            system(cmd.c_str());

            long long bytes = 0;
            bool got = false;

            if (ifstream file(tmp_path); file.is_open())
            {
                string line;
                if (getline(file, line))
                {
                    line.erase(line.find_last_not_of(" \r\n\t") + 1);
                    if (!line.empty() && line != "NA" && line != "none")
                    {
                        try
                        {
                            bytes = stoll(line);
                            got = true;
                        }
                        catch (...) {}
                    }
                }
                file.close();
                fs::remove(tmp_path);
            }

            State::probed_size = got ? Util::FormatBytes(bytes) : "Size unavailable";
            State::is_probing = false;
        }).detach();
    }

    void Download(string_view url, const t_FormatOption& fmt, int height, string_view path, string_view custom_filename = "")
    {
        State::is_downloading = true;
        State::should_cancel = false;
        Shell::SetStatus("Preparing download…");

        thread([url = string(url), fmt, height, path = string(path), custom_filename = string(custom_filename)]()
        {
            if (!Shell::Exists("yt-dlp"))
            {
                Shell::SetStatus("Error: yt-dlp not found.", true);
                State::is_downloading = false;
                return;
            }

            string cmd = BuildCommand(url, fmt, height, path, custom_filename);
            Shell::SetStatus("Downloading…");

            int exit_code = system(cmd.c_str());

            if (State::should_cancel)
            {
                Shell::SetStatus("Download canceled.", true);
            }
            else if (exit_code == 0)
            {
                Shell::SetStatus("Download completed successfully!");
            }
            else
            {
                Shell::SetStatus("Download failed.", true);
            }

            State::is_downloading = false;
        }).detach();
    }
}

namespace UI
{
    void ApplyTheme(ImGuiStyle& s)
    {
        s.WindowBorderSize = 0.0f;
        s.FrameBorderSize = 0.0f;
        s.WindowRounding = 0.0f;
        s.FrameRounding = 6.0f;
        s.ScrollbarRounding = 4.0f;
        s.GrabRounding = 4.0f;
        s.PopupRounding = 6.0f;
        s.ItemSpacing = ImVec2(10, 9);
        s.FramePadding = ImVec2(10, 7);

        auto& c = s.Colors;
        c[ImGuiCol_WindowBg]             = ImVec4(0.10f, 0.10f, 0.12f, 1.f);
        c[ImGuiCol_ChildBg]              = ImVec4(0.12f, 0.12f, 0.15f, 1.f);
        c[ImGuiCol_PopupBg]              = ImVec4(0.13f, 0.13f, 0.17f, 1.f);
        c[ImGuiCol_Border]               = ImVec4(0.26f, 0.26f, 0.32f, 1.f);
        c[ImGuiCol_FrameBg]              = ImVec4(0.17f, 0.17f, 0.21f, 1.f);
        c[ImGuiCol_FrameBgHovered]       = ImVec4(0.21f, 0.21f, 0.27f, 1.f);
        c[ImGuiCol_FrameBgActive]        = ImVec4(0.24f, 0.24f, 0.30f, 1.f);
        c[ImGuiCol_Header]               = ImVec4(0.20f, 0.42f, 0.68f, 1.f);
        c[ImGuiCol_HeaderHovered]        = ImVec4(0.24f, 0.52f, 0.82f, 1.f);
        c[ImGuiCol_HeaderActive]         = ImVec4(0.15f, 0.35f, 0.58f, 1.f);
        c[ImGuiCol_Separator]            = ImVec4(0.24f, 0.24f, 0.30f, 1.f);
        c[ImGuiCol_SeparatorHovered]     = ImVec4(0.40f, 0.40f, 0.50f, 1.f);
        c[ImGuiCol_ScrollbarBg]          = ImVec4(0.10f, 0.10f, 0.12f, 1.f);
        c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.28f, 0.28f, 0.36f, 1.f);
        c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.36f, 0.36f, 0.46f, 1.f);
        c[ImGuiCol_Text]                 = ImVec4(0.90f, 0.90f, 0.92f, 1.f);
        c[ImGuiCol_TextDisabled]         = ImVec4(0.38f, 0.38f, 0.42f, 1.f);
        c[ImGuiCol_Button]               = ImVec4(0.22f, 0.22f, 0.28f, 1.f);
        c[ImGuiCol_ButtonHovered]        = ImVec4(0.30f, 0.30f, 0.38f, 1.f);
        c[ImGuiCol_ButtonActive]         = ImVec4(0.17f, 0.17f, 0.22f, 1.f);
        c[ImGuiCol_CheckMark]            = ImVec4(0.30f, 0.90f, 0.52f, 1.f);
        c[ImGuiCol_PlotHistogram]        = ImVec4(0.18f, 0.62f, 0.28f, 1.f);
        c[ImGuiCol_PlotHistogramHovered] = ImVec4(0.22f, 0.78f, 0.35f, 1.f);
    }

    // Helper function for string input with ImGui
    static bool InputText(const char* label, string& str, float width = 0.0f)
    {
        if (width > 0.0f)
        {
            ImGui::SetNextItemWidth(width);
        }

        // Create a buffer with extra space
        array<char, 4096> buffer{};
        str.copy(buffer.data(), buffer.size() - 1);
        buffer[str.size()] = '\0';

        if (ImGui::InputText(label, buffer.data(), buffer.size()))
        {
            str = buffer.data();
            return true;
        }
        return false;
    }

    void Draw(string& url_buf, int& fmt_idx, int& res_idx)
    {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();

        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(static_cast<float>(sw), static_cast<float>(sh)), ImGuiCond_Always);

        ImGuiWindowFlags root_flags =
            ImGuiWindowFlags_NoTitleBar  |
            ImGuiWindowFlags_NoResize    |
            ImGuiWindowFlags_NoMove      |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoCollapse  |
            ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28, 22));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(10, 10));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(10, 7));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

        ImGui::Begin("##root", nullptr, root_flags);

        float W = ImGui::GetContentRegionAvail().x;
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.84f, 1.0f, 1.f));
        ImGui::Text("Video URL");
        ImGui::PopStyleColor();

        ImGui::SetNextItemWidth(W);
        InputText("##url", url_buf);

        ImGui::Spacing();

        const t_FormatOption& cur_fmt = kFormats[fmt_idx];
        bool audio_only = cur_fmt.audio_only;

        float col_w = (W - 10) * 0.5f;

        // Create combo items on the stack
        auto fmt_items = []()
        {
            array<const char*, kFormats.size()> arr{};
            for (size_t i = 0; i < kFormats.size(); ++i)
                arr[i] = kFormats[i].label.data();
            return arr;
        }();

        auto res_items = []()
        {
            array<const char*, kResolutions.size()> arr{};
            for (size_t i = 0; i < kResolutions.size(); ++i)
                arr[i] = kResolutions[i].label.data();
            return arr;
        }();

        // Labels row — both on the exact same line
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.84f, 1.0f, 1.f));
        ImGui::Text("Format");
        ImGui::SameLine(col_w + 10.0f + ImGui::GetStyle().WindowPadding.x);
        ImGui::Text("Resolution");
        ImGui::PopStyleColor();

        // Combos row
        ImGui::SetNextItemWidth(col_w);
        if (ImGui::Combo("##format", &fmt_idx, fmt_items.data(), static_cast<int>(fmt_items.size())))
        {
            State::probed_size.clear();
        }

        ImGui::SameLine(0, 10);
        ImGui::SetNextItemWidth(col_w);

        if (audio_only)
        {
            ImGui::BeginDisabled();
            int dummy = 0;
            const array<const char*, 1> na = {"N/A  (audio only)"};
            ImGui::Combo("##res_na", &dummy, na.data(), static_cast<int>(na.size()));
            ImGui::EndDisabled();
        }
        else
        {
            if (ImGui::Combo("##res", &res_idx, res_items.data(), static_cast<int>(res_items.size())))
            {
                State::probed_size.clear();
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        bool url_ok = !url_buf.empty();
        int res_px = audio_only ? 0 : kResolutions[res_idx].height;

        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.18f, 0.38f, 0.60f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.50f, 0.80f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.12f, 0.28f, 0.48f, 1.f));

        bool size_check_disabled = (!url_ok || State::is_probing);
        if (size_check_disabled)
        {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("  Check File Size  "))
        {
            Ytdlp::ProbeSize(url_buf, cur_fmt, res_px);
        }
        if (size_check_disabled)
        {
            ImGui::EndDisabled();
        }

        ImGui::PopStyleColor(3);
        ImGui::SameLine(0, 14);

        if (State::is_probing)
        {
            ImGui::TextColored(ImVec4(1.f, 0.88f, 0.2f, 1.f), "Probing, please wait…");
        }
        else if (!State::probed_size.empty())
        {
            ImGui::TextColored
            (
                ImVec4(0.30f, 0.92f, 0.60f, 1.f),
                "Estimated size:  %s", State::probed_size.c_str()
            );
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.40f, 0.46f, 1.f));
            ImGui::Text("Enter a URL then click to estimate size");
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();

        float btn_w = min(W, 220.f);
        float btn_x = (W - btn_w) * 0.5f + ImGui::GetStyle().WindowPadding.x;
        ImGui::SetCursorPosX(btn_x);

        bool can_dl = !State::is_installing && !State::is_downloading && url_ok;

        ImGui::PushStyleColor(ImGuiCol_Button,
            can_dl ? ImVec4(0.13f, 0.58f, 0.24f, 1.f)
                   : ImVec4(0.20f, 0.20f, 0.24f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.74f, 0.32f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.09f, 0.42f, 0.17f, 1.f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20, 11));

        bool download_disabled = !can_dl;
        if (download_disabled) ImGui::BeginDisabled();

        if (ImGui::Button("  Download  ", ImVec2(btn_w, 0)))
        {
            if (!Shell::Exists("yt-dlp") && !State::is_installing)
            {
                Installer::EnsureYtDlp();
                Shell::SetStatus("Please wait – installing yt-dlp…");
            }
            else
            {
                const char* sel = tinyfd_saveFileDialog
                (
                    "Save file as",
                    cur_fmt.ext.data(),
                    0,
                    nullptr,
                    nullptr
                );

                if (sel)
                {
                    if (!audio_only && !Shell::Exists("ffmpeg") && !State::is_installing)
                    {
                        Installer::EnsureFfmpeg();
                    }

                    string fp(sel);
                    fs::path selected_path(fp);
                    string dir = selected_path.has_parent_path() ? selected_path.parent_path().string() : ".";

                    State::pending_url = url_buf;
                    State::pending_format = string(cur_fmt.ext);
                    State::pending_path = dir;
                    State::pending_res = res_px;
                    State::show_confirmation = true;
                }
                else
                {
                    Shell::SetStatus("Save location not selected.", true);
                }
            }
        }

        if (download_disabled) ImGui::EndDisabled();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        if (State::show_confirmation)
        {
            ImGui::OpenPopup("Confirm Download");
            ImGui::SetNextWindowPos
            (
                ImVec2(sw * 0.5f, sh * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f)
            );
        }

        static string custom_filename;  // Persistent across frames

        if
        (
            ImGui::BeginPopupModal
            (
                "Confirm Download",
                nullptr,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove
            )
        )
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.75f, 0.20f, 1.f));
            ImGui::Text("Ready to download?");
            ImGui::PopStyleColor();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Text("Format   :  %s", State::pending_format.c_str());

            if (!audio_only)
            {
                string res_label = State::pending_res == 0
                    ? "Best available"
                    : to_string(State::pending_res) + "p";
                ImGui::Text("Max Res  :  %s", res_label.c_str());
            }

            if (!State::probed_size.empty() && State::probed_size != "Probing…")
            {
                ImGui::Text("Est. size:  %s", State::probed_size.c_str());
            }

            ImGui::Text("Save to  :  %s", State::pending_path.c_str());

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Add filename input
            ImGui::Text("Filename (without extension):");

            array<char, 256> filename_buf{};
            strcpy(filename_buf.data(), custom_filename.c_str());

            ImGui::SetNextItemWidth(300);
            if (ImGui::InputText("##filename", filename_buf.data(), filename_buf.size()))
            {
                custom_filename = filename_buf.data();
            }

            ImGui::TextDisabled("Leave empty to use video title");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.13f, 0.58f, 0.24f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.74f, 0.32f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.09f, 0.42f, 0.17f, 1.f));

            if (ImGui::Button("  Start Download  ", ImVec2(160, 0)))
            {
                State::pending_filename = custom_filename;
                Ytdlp::Download
                (
                    State::pending_url,
                    kFormats[fmt_idx],
                    State::pending_res,
                    State::pending_path,
                    State::pending_filename
                );
                State::show_confirmation = false;
                custom_filename.clear();  // Reset for next download
                ImGui::CloseCurrentPopup();
            }

            ImGui::PopStyleColor(3);
            ImGui::SameLine(0, 12);

            if (ImGui::Button("  Cancel  ", ImVec2(100, 0)))
            {
                State::show_confirmation = false;
                custom_filename.clear();
                Shell::SetStatus("Download canceled.", true);
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (State::is_installing)
        {
            ImGui::TextColored
            (
                ImVec4(1.f, 0.90f, 0.2f, 1.f),
                "Installing %s…", State::install_package.c_str()
            );
            ImGui::ProgressBar(State::install_progress, ImVec2(W, 0));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.58f, 1.f));
            ImGui::Text("This may take a few minutes…");
            ImGui::PopStyleColor();
        }
        else if (State::is_downloading)
        {
            ImGui::TextColored
            (
                ImVec4(0.28f, 0.95f, 0.50f, 1.f),
                "Download in progress – check the terminal window."
            );

            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.60f, 0.13f, 0.13f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.78f, 0.18f, 0.18f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.45f, 0.09f, 0.09f, 1.f));

            if (ImGui::Button("  Cancel Download  "))
            {
                State::should_cancel = true;
                Shell::CancelYtDlp();
                Shell::SetStatus("Canceling download…", true);
            }

            ImGui::PopStyleColor(3);
        }

        if (!State::status_message.empty())
        {
            ImGui::Spacing();
            ImVec4 col = State::is_error
                ? ImVec4(1.f, 0.32f, 0.32f, 1.f)
                : ImVec4(0.28f, 0.95f, 0.50f, 1.f);
            ImGui::TextColored(col, "%s", State::status_message.c_str());
        }

        float line_h = ImGui::GetTextLineHeightWithSpacing();
        float footer_y = static_cast<float>(sh)
            - ImGui::GetStyle().WindowPadding.y
            - line_h - 4.f;

        if (ImGui::GetCursorPosY() < footer_y)
        {
            ImGui::SetCursorPosY(footer_y);
        }

        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.30f, 0.30f, 0.34f, 1.f));
        ImGui::Text("  Drag window edges to resize  ");
        ImGui::PopStyleColor();

        ImGui::End();
        ImGui::PopStyleVar(4);
    }
}

int main()
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(860, 520, "Youtube Video Downloader");
    SetWindowMinSize(540, 360);
    SetTargetFPS(60);

    rlImGuiSetup(true);
    ImCustomTheme();
    UI::ApplyTheme(ImGui::GetStyle());

    string url_buf;
    int fmt_idx = 0;
    int res_idx = 3;

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground({26, 26, 31, 255});

        rlImGuiBegin();
        UI::Draw(url_buf, fmt_idx, res_idx);
        rlImGuiEnd();

        EndDrawing();
    }

    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
