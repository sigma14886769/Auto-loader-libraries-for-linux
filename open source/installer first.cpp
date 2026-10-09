// Установщик пакетов и библиотек для C++ под Linux.
// GTK4 + системный пакетный менеджер.

#include <gtk/gtk.h>        // GTK
#include <filesystem>       // файловая система
#include <fstream>          // файлы
#include <sstream>          // строковые потоки
#include <iostream>         // ввод/вывод
#include <string>           // строки
#include <vector>           // вектор
#include <algorithm>        // алгоритмы
#include <cstdlib>          // system()
#include <cctype>           // символы
#include <unistd.h>         // unistd

namespace fs = std::filesystem;

// Поддерживаемые дистрибутивы
enum class Distro { None, Fedora, Arch, Debian };

// Описание одного пакета/приложения
struct Lib {
    std::string name;              // название для пользователя
    std::string fedora;            // имя пакета в Fedora
    std::string arch;              // имя пакета в Arch
    std::string debian;            // имя пакета в Debian
    std::string github_repo = "";  // если ставим с GitHub
    std::string github_file = "";  // имя файла в релизе
    bool base = false;             // включён по умолчанию
    bool is_app = false;           // приложение или библиотека
    GtkWidget* check = nullptr;    // чекбокс
};

// Глобальное состояние
static Distro g_distro = Distro::None;       // выбранный дистрибутив
static std::vector<Lib> g_libs;              // все пакеты
static GtkWindow* g_win = nullptr;           // главное окно
static GtkWidget* g_stack = nullptr;         // переключение экранов
static GtkWidget* g_pwd_entry = nullptr;     // поле пароля
static GtkWidget* g_install_btn = nullptr;   // кнопка установки
static GtkWidget* g_output_view = nullptr;   // вывод
static GtkWidget* g_spinner = nullptr;       // индикатор
static GtkWidget* g_lib_list_box = nullptr;  // список пакетов

// Заполняем список всего, что можно поставить
static void init_libs() {
    g_libs = {
        // Приложения
        {"Happ (VPN/Proxy)", "", "", "", "Happ-proxy/happ-desktop", "Happ.linux.x64", false, true},
        {"Neovim",       "neovim",     "neovim",     "neovim",     "", "", false, true},
        {"Fastfetch",    "fastfetch",  "fastfetch",  "fastfetch",  "", "", false, true},
        {"Ghostty",      "ghostty",    "ghostty",    "ghostty",    "", "", false, true},
        {"Kitty",        "kitty",      "kitty",      "kitty",      "", "", false, true},
        {"Alacritty",    "alacritty",  "alacritty",  "alacritty",  "", "", false, true},
        {"Zsh",          "zsh",        "zsh",        "zsh",        "", "", false, true},
        {"tmux",         "tmux",       "tmux",       "tmux",       "", "", false, true},
        {"htop",         "htop",       "htop",       "htop",       "", "", false, true},
        {"btop",         "btop",       "btop",       "btop",       "", "", false, true},
        {"curl",         "curl",       "curl",       "curl",       "", "", false, true},
        {"wget",         "wget",       "wget",       "wget",       "", "", false, true},
        {"ripgrep",      "ripgrep",    "ripgrep",    "ripgrep",    "", "", false, true},
        {"fd",           "fd-find",    "fd",         "fd-find",    "", "", false, true},
        {"fzf",          "fzf",        "fzf",        "fzf",        "", "", false, true},

        // Компиляторы и сборка
        {"Компилятор C++", "gcc-c++",           "base-devel",       "build-essential",  "", "", true,  false},
        {"Make",           "make",               "make",             "make",             "", "", true,  false},
        {"CMake",          "cmake",              "cmake",            "cmake",            "", "", true,  false},
        {"Ninja",          "ninja-build",        "ninja",            "ninja-build",      "", "", false, false},
        {"pkg-config",     "pkgconf-pkg-config", "pkgconf",          "pkg-config",       "", "", true,  false},
        {"Git",            "git",                "git",              "git",              "", "", false, false},

        // GUI / графика
        {"GTK4",           "gtk4-devel",         "gtk4",             "libgtk-4-dev",     "", "", false, false},
        {"Qt6",            "qt6-qtbase-devel",   "qt6-base",         "qt6-base-dev",     "", "", false, false},
        {"SDL2",           "SDL2-devel",         "sdl2",             "libsdl2-dev",      "", "", false, false},
        {"SFML",           "SFML-devel",         "sfml",             "libsfml-dev",      "", "", false, false},
        {"GLFW",           "glfw-devel",         "glfw",             "libglfw3-dev",     "", "", false, false},
        {"OpenGL",         "mesa-libGL-devel",   "mesa",             "libgl1-mesa-dev",  "", "", false, false},
        {"Vulkan",         "vulkan-loader-devel","vulkan-icd-loader","libvulkan-dev",    "", "", false, false},

        // Часто нужные библиотеки
        {"ncurses",        "ncurses-devel",      "ncurses",          "libncurses-dev",   "", "", false, false},
        {"OpenSSL",        "openssl-devel",      "openssl",          "libssl-dev",       "", "", false, false},
        {"Boost",          "boost-devel",        "boost",            "libboost-all-dev", "", "", false, false},
        {"SQLite3",        "sqlite-devel",       "sqlite",           "libsqlite3-dev",   "", "", false, false},
        {"libcurl-dev",    "libcurl-devel",      "curl",             "libcurl4-openssl-dev","","",false,false},

        // Изображения, звук, медиа
        {"libpng",         "libpng-devel",       "libpng",           "libpng-dev",       "", "", false, false},
        {"libjpeg",        "libjpeg-turbo-devel","libjpeg-turbo",    "libjpeg-dev",      "", "", false, false},
        {"zlib",           "zlib-devel",         "zlib",             "zlib1g-dev",       "", "", false, false},
        {"FFmpeg",         "ffmpeg-devel",       "ffmpeg",           "libavcodec-dev",   "", "", false, false},
        {"ALSA",           "alsa-lib-devel",     "alsa-lib",         "libasound2-dev",   "", "", false, false},
        {"PulseAudio",     "pulseaudio-libs-devel","libpulse",       "libpulse-dev",     "", "", false, false},

        // Графика и шрифты
        {"X11",            "libX11-devel",       "libx11",           "libx11-dev",       "", "", false, false},
        {"Wayland",        "wayland-devel",      "wayland",          "libwayland-dev",   "", "", false, false},
        {"Freetype",       "freetype-devel",     "freetype2",        "libfreetype6-dev", "", "", false, false},
        {"HarfBuzz",       "harfbuzz-devel",     "harfbuzz",         "libharfbuzz-dev",  "", "", false, false},
        {"Cairo",          "cairo-devel",        "cairo",            "libcairo2-dev",    "", "", false, false},
        {"Pango",          "pango-devel",        "pango",            "libpango1.0-dev",  "", "", false, false},

        // Системные и утилитарные
        {"GLib",           "glib2-devel",        "glib2",            "libglib2.0-dev",   "", "", false, false},
        {"D-Bus",          "dbus-devel",         "dbus",             "libdbus-1-dev",    "", "", false, false},
        {"libnotify",      "libnotify-devel",    "libnotify",        "libnotify-dev",    "", "", false, false},

        // Популярные C++ библиотеки
        {"nlohmann-json",  "json-devel",         "nlohmann-json",    "nlohmann-json3-dev","","",false, false},
        {"fmt",            "fmt-devel",          "fmt",              "libfmt-dev",       "", "", false, false},
        {"spdlog",         "spdlog-devel",       "spdlog",           "libspdlog-dev",    "", "", false, false},
        {"yaml-cpp",       "yaml-cpp-devel",     "yaml-cpp",         "libyaml-cpp-dev",  "", "", false, false},
        {"protobuf",       "protobuf-devel",     "protobuf",         "libprotobuf-dev",  "", "", false, false},

        // Тесты
        {"GTest",          "gtest-devel",        "gtest",            "libgtest-dev",     "", "", false, false},
        {"Catch2",         "catch2-devel",       "catch2",           "catch2",           "", "", false, false},
    };
}

// Имя пакета для текущего дистрибутива
static std::string pkg_name(const Lib& l) {
    switch (g_distro) {
        case Distro::Fedora: return l.fedora;
        case Distro::Arch:   return l.arch;
        case Distro::Debian: return l.debian;
        default:             return "";
    }
}

// Задание на установку
struct InstallJob {
    std::string command;
    std::string logfile;
};

// После завершения установки показываем лог и включаем кнопки
static gboolean install_done_idle(gpointer data) {
    auto* job = static_cast<InstallJob*>(data);

    std::ifstream f(job->logfile);
    std::stringstream ss;
    ss << f.rdbuf();
    f.close();

    GtkTextBuffer* tb = gtk_text_view_get_buffer(GTK_TEXT_VIEW(g_output_view));
    gtk_text_buffer_set_text(tb, ss.str().c_str(), -1);

    GtkTextIter end;
    gtk_text_buffer_get_end_iter(tb, &end);
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(g_output_view), &end,
                                 0.0, FALSE, 0.0, 0.0);

    gtk_widget_set_visible(g_spinner, FALSE);
    gtk_widget_set_sensitive(g_install_btn, TRUE);
    gtk_widget_set_sensitive(g_pwd_entry, TRUE);

    delete job;
    return G_SOURCE_REMOVE;
}

// Поток установки: чтобы окно не зависало
static gpointer install_thread(gpointer data) {
    auto* job = static_cast<InstallJob*>(data);
    std::system(job->command.c_str());
    g_idle_add(install_done_idle, job);
    return nullptr;
}

// Кнопка "Установить"
static void on_install_clicked(GtkButton*, gpointer) {
    std::string pkg_prefix, file_prefix, file_ext;
    switch (g_distro) {
        case Distro::Fedora:
            pkg_prefix  = "dnf install -y";
            file_prefix = "dnf install -y";
            file_ext    = ".rpm";
            break;
        case Distro::Arch:
            pkg_prefix  = "pacman -Sy --noconfirm";
            file_prefix = "pacman -U --noconfirm";
            file_ext    = ".pkg.tar.zst";
            break;
        case Distro::Debian:
            pkg_prefix  = "apt-get update && apt-get install -y";
            file_prefix = "apt-get install -y";
            file_ext    = ".deb";
            break;
        default: return;
    }

    std::vector<std::string> standard_pkgs;
    std::vector<std::pair<std::string,std::string>> gh_downloads;

    // Собираем выбранное
    for (auto& l : g_libs) {
        if (!l.check || !gtk_check_button_get_active(GTK_CHECK_BUTTON(l.check))) continue;

        if (!l.github_repo.empty()) {
            std::string filename = l.github_file + file_ext;
            std::string url = "https://github.com/" + l.github_repo +
                              "/releases/latest/download/" + filename;
            std::string tmp = "/tmp/" + filename;
            gh_downloads.push_back({url, tmp});
        } else {
            std::string p = pkg_name(l);
            if (!p.empty() && std::find(standard_pkgs.begin(), standard_pkgs.end(), p)
                              == standard_pkgs.end())
                standard_pkgs.push_back(p);
        }
    }

    if (standard_pkgs.empty() && gh_downloads.empty()) {
        GtkAlertDialog* dlg = gtk_alert_dialog_new("Ничего не выбрано");
        gtk_alert_dialog_set_detail(dlg, "Отметь хотя бы один пункт.");
        gtk_alert_dialog_show(dlg, g_win);
        g_object_unref(dlg);
        return;
    }

    const char* pwd_c = gtk_editable_get_text(GTK_EDITABLE(g_pwd_entry));
    if (!pwd_c || !*pwd_c) {
        GtkAlertDialog* dlg = gtk_alert_dialog_new("Нет пароля");
        gtk_alert_dialog_set_detail(dlg, "Введи пароль sudo сверху.");
        gtk_alert_dialog_show(dlg, g_win);
        g_object_unref(dlg);
        return;
    }
    std::string password = pwd_c;

    // Собираем shell-команду
    std::string inner;
    if (!standard_pkgs.empty()) {
        inner += "echo Installing standard packages... ; ";
        inner += pkg_prefix;
        for (auto& p : standard_pkgs) inner += " " + p;
        inner += " ; ";
    }
    for (auto& [url, tmp] : gh_downloads) {
        inner += "echo Downloading " + url + " ; ";
        inner += "curl -Lf --show-error -o " + tmp + " " + url + " ; ";
        inner += file_prefix + " " + tmp + " ; ";
        inner += "rm -f " + tmp + " ; ";
    }
    if (inner.size() > 3) inner = inner.substr(0, inner.size() - 3);

    std::string logfile = "/tmp/lib-installer.log";

    // Экранируем одинарные кавычки
    auto escape_sq = [](const std::string& s) {
        std::string out;
        for (char c : s) {
            if (c == '\'') out += "'\\''";
            else out += c;
        }
        return out;
    };
    std::string esc_pwd   = escape_sq(password);
    std::string esc_inner = escape_sq(inner);

    std::string cmd =
        "FF_PWD='" + esc_pwd + "' "
        "bash -c 'echo \"$FF_PWD\" | "
        "sudo -S -p \"\" sh -c \"" + esc_inner + "\" "
        "> " + logfile + " 2>&1'";

    gtk_widget_set_sensitive(g_install_btn, FALSE);
    gtk_widget_set_sensitive(g_pwd_entry, FALSE);
    gtk_widget_set_visible(g_spinner, TRUE);

    GtkTextBuffer* tb = gtk_text_view_get_buffer(GTK_TEXT_VIEW(g_output_view));
    gtk_text_buffer_set_text(tb, "Установка началась...\n\n", -1);

    auto* job = new InstallJob{cmd, logfile};
    g_thread_unref(g_thread_new("installer", install_thread, job));
}

// Выбрать всё
static void on_select_all(GtkButton*, gpointer) {
    for (auto& l : g_libs)
        if (l.check) gtk_check_button_set_active(GTK_CHECK_BUTTON(l.check), TRUE);
}

// Снять всё
static void on_unselect_all(GtkButton*, gpointer) {
    for (auto& l : g_libs)
        if (l.check) gtk_check_button_set_active(GTK_CHECK_BUTTON(l.check), FALSE);
}

// Строка списка: чекбокс + имя пакета
static GtkWidget* make_lib_row(Lib& lib) {
    GtkWidget* row = gtk_list_box_row_new();

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_top(box, 4);
    gtk_widget_set_margin_bottom(box, 4);
    gtk_widget_set_margin_start(box, 8);
    gtk_widget_set_margin_end(box, 8);

    GtkWidget* check = gtk_check_button_new_with_label(lib.name.c_str());
    gtk_check_button_set_active(GTK_CHECK_BUTTON(check), lib.base);
    gtk_widget_set_hexpand(check, TRUE);
    gtk_box_append(GTK_BOX(box), check);
    lib.check = check;

    std::string info;
    if (!lib.github_repo.empty()) {
        info = "GitHub: " + lib.github_repo;
    } else {
        info = pkg_name(lib);
    }

    GtkWidget* pkg_label = gtk_label_new(info.c_str());
    gtk_widget_add_css_class(pkg_label, "dim-label");
    gtk_label_set_xalign(GTK_LABEL(pkg_label), 1.0);
    gtk_box_append(GTK_BOX(box), pkg_label);

    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
    return row;
}

// Когда выбран дистрибутив — строим списки
static void on_distro_chosen(GtkButton*, gpointer data) {
    g_distro = static_cast<Distro>(GPOINTER_TO_INT(data));

    // Чистим старый список
    GtkWidget* child = gtk_widget_get_first_child(g_lib_list_box);
    while (child) {
        GtkWidget* next = gtk_widget_get_next_sibling(child);
        gtk_list_box_remove(GTK_LIST_BOX(g_lib_list_box), child);
        child = next;
    }
    for (auto& l : g_libs) l.check = nullptr;

    // Приложения
    {
        GtkWidget* header = gtk_label_new(nullptr);
        gtk_label_set_markup(GTK_LABEL(header), "<b>Приложения</b>");
        gtk_widget_set_halign(header, GTK_ALIGN_START);
        gtk_widget_set_margin_top(header, 8);
        gtk_widget_set_margin_start(header, 8);

        GtkWidget* row = gtk_list_box_row_new();
        gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(row), FALSE);
        gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), FALSE);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), header);
        gtk_list_box_append(GTK_LIST_BOX(g_lib_list_box), row);
    }
    for (auto& lib : g_libs) {
        if (!lib.is_app) continue;
        gtk_list_box_append(GTK_LIST_BOX(g_lib_list_box), make_lib_row(lib));
    }

    // Библиотеки C++
    {
        GtkWidget* header = gtk_label_new(nullptr);
        gtk_label_set_markup(GTK_LABEL(header), "<b>Библиотеки C++</b>");
        gtk_widget_set_halign(header, GTK_ALIGN_START);
        gtk_widget_set_margin_top(header, 16);
        gtk_widget_set_margin_start(header, 8);

        GtkWidget* row = gtk_list_box_row_new();
        gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(row), FALSE);
        gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), FALSE);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), header);
        gtk_list_box_append(GTK_LIST_BOX(g_lib_list_box), row);
    }
    for (auto& lib : g_libs) {
        if (lib.is_app) continue;
        gtk_list_box_append(GTK_LIST_BOX(g_lib_list_box), make_lib_row(lib));
    }

    gtk_stack_set_visible_child_name(GTK_STACK(g_stack), "install");
}

// Экран выбора дистрибутива
static GtkWidget* build_choose_screen() {
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_top(vbox, 40);
    gtk_widget_set_margin_bottom(vbox, 40);
    gtk_widget_set_margin_start(vbox, 40);
    gtk_widget_set_margin_end(vbox, 40);

    GtkWidget* title = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(title),
        "<span size='xx-large' weight='bold'>Выбери дистрибутив</span>");
    gtk_box_append(GTK_BOX(vbox), title);

    GtkWidget* sub = gtk_label_new("От этого зависят имена пакетов");
    gtk_widget_add_css_class(sub, "dim-label");
    gtk_box_append(GTK_BOX(vbox), sub);

    GtkWidget* spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(vbox), spacer);

    struct { const char* label; Distro d; const char* icon; } btns[] = {
        {"Fedora / RHEL / CentOS",       Distro::Fedora, "fedora-logo-icon"},
        {"Arch / Manjaro / EndeavourOS", Distro::Arch,   "distributor-logo-archlinux"},
        {"Debian / Ubuntu / Mint",       Distro::Debian, "distributor-logo-debian"},
    };

    for (auto& b : btns) {
        GtkWidget* btn = gtk_button_new();
        gtk_widget_set_size_request(btn, -1, 60);

        GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_set_halign(hbox, GTK_ALIGN_CENTER);
        GtkWidget* icon = gtk_image_new_from_icon_name(b.icon);
        gtk_image_set_pixel_size(GTK_IMAGE(icon), 32);
        GtkWidget* lbl = gtk_label_new(b.label);
        gtk_widget_add_css_class(lbl, "title-3");
        gtk_box_append(GTK_BOX(hbox), icon);
        gtk_box_append(GTK_BOX(hbox), lbl);

        gtk_button_set_child(GTK_BUTTON(btn), hbox);
        g_signal_connect(btn, "clicked", G_CALLBACK(on_distro_chosen),
                         GINT_TO_POINTER(static_cast<int>(b.d)));
        gtk_box_append(GTK_BOX(vbox), btn);
    }

    GtkWidget* spacer2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(spacer2, TRUE);
    gtk_box_append(GTK_BOX(vbox), spacer2);

    return vbox;
}

// Экран установки: пароль, список, кнопки, вывод
static GtkWidget* build_install_screen() {
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_top(vbox, 14);
    gtk_widget_set_margin_bottom(vbox, 14);
    gtk_widget_set_margin_start(vbox, 14);
    gtk_widget_set_margin_end(vbox, 14);

    GtkWidget* top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget* pwd_label = gtk_label_new("Пароль sudo:");
    gtk_box_append(GTK_BOX(top), pwd_label);

    g_pwd_entry = gtk_password_entry_new();
    gtk_password_entry_set_show_peek_icon(GTK_PASSWORD_ENTRY(g_pwd_entry), TRUE);
    gtk_widget_set_hexpand(g_pwd_entry, TRUE);
    gtk_box_append(GTK_BOX(top), g_pwd_entry);
    gtk_box_append(GTK_BOX(vbox), top);

    GtkWidget* tools = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* all = gtk_button_new_with_label("Выбрать всё");
    GtkWidget* none = gtk_button_new_with_label("Снять всё");
    g_signal_connect(all, "clicked", G_CALLBACK(on_select_all), nullptr);
    g_signal_connect(none, "clicked", G_CALLBACK(on_unselect_all), nullptr);
    gtk_box_append(GTK_BOX(tools), all);
    gtk_box_append(GTK_BOX(tools), none);
    gtk_box_append(GTK_BOX(vbox), tools);

    GtkWidget* scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);

    g_lib_list_box = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(g_lib_list_box), GTK_SELECTION_NONE);
    gtk_widget_add_css_class(g_lib_list_box, "boxed-list");

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), g_lib_list_box);
    gtk_box_append(GTK_BOX(vbox), scroll);

    GtkWidget* bottom = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);

    g_spinner = gtk_spinner_new();
    gtk_widget_set_visible(g_spinner, FALSE);
    gtk_box_append(GTK_BOX(bottom), g_spinner);

    g_install_btn = gtk_button_new_with_label("Установить");
    gtk_widget_add_css_class(g_install_btn, "suggested-action");
    gtk_widget_set_hexpand(g_install_btn, TRUE);
    gtk_widget_set_size_request(g_install_btn, -1, 48);
    g_signal_connect(g_install_btn, "clicked",
                     G_CALLBACK(on_install_clicked), nullptr);
    gtk_box_append(GTK_BOX(bottom), g_install_btn);
    gtk_box_append(GTK_BOX(vbox), bottom);

    GtkWidget* out_frame = gtk_frame_new("Вывод установки");
    GtkWidget* out_scroll = gtk_scrolled_window_new();
    gtk_widget_set_size_request(out_frame, -1, 220);

    g_output_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(g_output_view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(g_output_view), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(g_output_view), GTK_WRAP_WORD_CHAR);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(out_scroll), g_output_view);
    gtk_frame_set_child(GTK_FRAME(out_frame), out_scroll);
    gtk_box_append(GTK_BOX(vbox), out_frame);

    return vbox;
}

// Точка входа GTK: создаём окно
static void activate(GtkApplication* app, gpointer) {
    g_win = GTK_WINDOW(gtk_application_window_new(app));
    gtk_window_set_title(g_win, "C++ Libraries & Apps Installer");
    gtk_window_set_default_size(g_win, 850, 820);

    g_stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(g_stack),
        GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);

    gtk_stack_add_named(GTK_STACK(g_stack), build_choose_screen(), "choose");
    gtk_stack_add_named(GTK_STACK(g_stack), build_install_screen(), "install");
    gtk_stack_set_visible_child_name(GTK_STACK(g_stack), "choose");

    gtk_window_set_child(g_win, g_stack);
    gtk_window_present(g_win);
}

// main
int main(int argc, char** argv) {
    init_libs();

    GtkApplication* app = gtk_application_new(
        "org.local.CppLibInstaller",
        G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), nullptr);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
