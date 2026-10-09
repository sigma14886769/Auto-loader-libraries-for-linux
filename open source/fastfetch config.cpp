// Установщик конфигов Fastfetch.
// Раздаёт файлы из папки files/ по местам, пути можно менять и сохранять.

#include <gtk/gtk.h>        // GTK
#include <filesystem>       // файловая система
#include <fstream>          // файлы
#include <sstream>          // строковые потоки
#include <iostream>         // ввод/вывод
#include <string>           // строки
#include <vector>           // вектор
#include <map>              // словарь
#include <algorithm>        // алгоритмы
#include <cstdlib>          // getenv
#include <cctype>           // символы
#include <unistd.h>         // readlink

namespace fs = std::filesystem;

// Один файл из папки files/
struct FileEntry {
    std::string source;    // откуда копируем
    std::string relpath;   // путь относительно files/ (ключ в настройках)
    std::string filename;  // имя файла
    std::string ext;       // расширение (в нижнем регистре)
    GtkWidget* entry = nullptr;  // поле с путём назначения
};

static std::vector<FileEntry> g_files;  // все найденные файлы
static GtkWindow* g_win = nullptr;      // главное окно

// Папка, где лежит сам exe
static fs::path get_exe_dir() {
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n < 0) return fs::current_path();
    buf[n] = '\0';
    return fs::path(buf).parent_path();
}

// Домашняя папка пользователя
static std::string home_dir() {
    const char* h = getenv("HOME");
    return h ? h : "/tmp";
}

// Путь к файлу настроек — рядом с exe
static std::string settings_path() {
    return (get_exe_dir() / "settings.conf").string();
}

// Читаем settings.conf в map "относительный_путь = куда_ставить"
static std::map<std::string, std::string> load_settings() {
    std::map<std::string, std::string> out;
    std::ifstream f(settings_path());
    if (!f.is_open()) return out;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;  // пропуск комментариев
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq);
        std::string v = line.substr(eq + 1);
        // обрезаем пробелы по краям
        while (!k.empty() && isspace((unsigned char)k.back())) k.pop_back();
        while (!v.empty() && isspace((unsigned char)v.front())) v.erase(v.begin());
        out[k] = v;
    }
    return out;
}

// Сохраняем текущие пути в settings.conf
static void save_settings() {
    std::ofstream f(settings_path(), std::ios::trunc);
    if (!f.is_open()) {
        std::cerr << "Не могу записать " << settings_path() << "\n";
        return;
    }
    f << "# Fastfetch Installer — сохранённые пути\n";
    f << "# Формат: относительный_путь=путь_назначения\n";
    f << "# Удали строку — сбросится на дефолт\n\n";
    for (auto& fe : g_files) {
        const char* c = gtk_editable_get_text(GTK_EDITABLE(fe.entry));
        if (c && *c) {
            f << fe.relpath << "=" << c << "\n";
        }
    }
}

// Дефолтный путь для файла по имени/расширению
static std::string default_dest_for(const std::string& filename) {
    std::string h = home_dir();
    std::string ext;
    auto pos = filename.rfind('.');
    if (pos != std::string::npos) {
        ext = filename.substr(pos);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }

    if (filename == "config.jsonc" || filename == "config.json5")
        return h + "/.config/fastfetch/" + filename;
    if (ext == ".sh" || ext == ".bash" || ext == ".py")
        return h + "/.local/bin/" + filename;
    if (ext == ".desktop")
        return h + "/.local/share/applications/" + filename;
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg"
        || ext == ".webp" || ext == ".txt" || ext == ".ascii")
        return h + "/.config/fastfetch/" + filename;
    return h + "/.config/fastfetch/" + filename;
}

// Кнопка "…" — выбрать папку для файла
static void on_browse_folder(GtkButton*, gpointer user_data) {
    GtkWidget* entry = GTK_WIDGET(user_data);

    GtkFileDialog* dlg = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dlg, "Выбери папку для установки");
    gtk_file_dialog_set_modal(dlg, TRUE);

    // Открываем диалог в текущей папке файла
    const char* cur = gtk_editable_get_text(GTK_EDITABLE(entry));
    if (cur && *cur) {
        fs::path p(cur);
        if (p.has_parent_path()) {
            GFile* gf = g_file_new_for_path(p.parent_path().c_str());
            gtk_file_dialog_set_initial_folder(dlg, gf);
            g_object_unref(gf);
        }
    }

    gtk_file_dialog_select_folder(
        dlg, g_win, nullptr,
        // Асинхронный колбэк: получаем выбранную папку и подставляем путь
        +[](GObject* src, GAsyncResult* res, gpointer user_data) {
            GtkFileDialog* d = GTK_FILE_DIALOG(src);
            GtkWidget* entry = GTK_WIDGET(user_data);
            GError* err = nullptr;
            GFile* folder = gtk_file_dialog_select_folder_finish(d, res, &err);
            if (folder) {
                char* path = g_file_get_path(folder);
                if (path) {
                    // Сохраняем имя файла, меняем только папку
                    const char* cur = gtk_editable_get_text(GTK_EDITABLE(entry));
                    std::string fname;
                    if (cur && *cur) fname = fs::path(cur).filename().string();
                    std::string newpath = std::string(path) + "/" + fname;
                    gtk_editable_set_text(GTK_EDITABLE(entry), newpath.c_str());
                    g_free(path);
                }
                g_object_unref(folder);
            }
            if (err) g_error_free(err);
        },
        entry);
}

// "Сбросить пути" — вернуть всё к дефолту и удалить settings.conf
static void on_reset_clicked(GtkButton*, gpointer) {
    for (auto& f : g_files) {
        std::string def = default_dest_for(f.filename);
        gtk_editable_set_text(GTK_EDITABLE(f.entry), def.c_str());
    }
    std::error_code ec;
    fs::remove(settings_path(), ec);

    GtkAlertDialog* dlg = gtk_alert_dialog_new("Пути сброшены");
    gtk_alert_dialog_set_detail(dlg,
        "Все пути вернулись к значениям по умолчанию.\n"
        "Файл settings.conf удалён.");
    gtk_alert_dialog_show(dlg, g_win);
    g_object_unref(dlg);
}

// "Установить" — копируем все файлы по указанным путям
static void on_install_clicked(GtkButton*, gpointer user_data) {
    GtkWindow* win = GTK_WINDOW(user_data);

    int ok = 0, fail = 0;
    std::string errs;

    for (auto& f : g_files) {
        const char* dest_c = gtk_editable_get_text(GTK_EDITABLE(f.entry));
        if (!dest_c || !*dest_c) continue;
        std::string dest = dest_c;

        try {
            fs::path p(dest);
            // Создаём папки, если их нет
            if (p.has_parent_path()) fs::create_directories(p.parent_path());
            fs::copy_file(f.source, dest, fs::copy_options::overwrite_existing);

            // Скрипты делаем исполняемыми
            if (f.ext == ".sh" || f.ext == ".bash") {
                fs::permissions(dest,
                    fs::perms::owner_exec | fs::perms::group_exec |
                    fs::perms::others_exec,
                    fs::perm_options::add);
            }
            ok++;
        } catch (const std::exception& e) {
            fail++;
            errs += std::string("• ") + f.relpath + ": " + e.what() + "\n";
        }
    }

    save_settings();

    std::string msg = "Установлено: " + std::to_string(ok)
                    + "\nОшибок: " + std::to_string(fail)
                    + "\n\nПути сохранены в:\n" + settings_path();
    if (!errs.empty()) msg += "\n\nОшибки:\n" + errs;

    GtkAlertDialog* dlg = gtk_alert_dialog_new(
        fail == 0 ? "Готово!" : "Завершено с ошибками");
    gtk_alert_dialog_set_detail(dlg, msg.c_str());
    gtk_alert_dialog_show(dlg, win);
    g_object_unref(dlg);
}

// Предварительное объявление — рекурсивная функция
static GtkWidget* build_dir_contents(const fs::path& dir,
                                     const fs::path& base,
                                     const std::map<std::string,std::string>& saved);

// Строка с одним файлом: иконка, имя, путь, кнопка "..."
static GtkWidget* make_file_row(const fs::path& file, const fs::path& rel,
                                const std::map<std::string,std::string>& saved) {
    FileEntry fe;
    fe.source   = file.string();
    fe.relpath  = rel.string();
    fe.filename = file.filename().string();
    auto pos = fe.filename.rfind('.');
    if (pos != std::string::npos) {
        fe.ext = fe.filename.substr(pos);
        std::transform(fe.ext.begin(), fe.ext.end(), fe.ext.begin(), ::tolower);
    }

    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_top(row, 3);
    gtk_widget_set_margin_bottom(row, 3);
    gtk_widget_set_margin_start(row, 8);
    gtk_widget_set_margin_end(row, 8);

    GtkWidget* icon = gtk_image_new_from_icon_name("text-x-generic-symbolic");
    gtk_box_append(GTK_BOX(row), icon);

    GtkWidget* label = gtk_label_new(fe.filename.c_str());
    gtk_widget_set_size_request(label, 180, -1);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_MIDDLE);
    gtk_box_append(GTK_BOX(row), label);

    // Путь: из сохранённых или дефолт
    GtkWidget* entry = gtk_entry_new();
    std::string path;
    auto it = saved.find(fe.relpath);
    if (it != saved.end() && !it->second.empty()) path = it->second;
    else path = default_dest_for(fe.filename);
    gtk_editable_set_text(GTK_EDITABLE(entry), path.c_str());
    gtk_widget_set_hexpand(entry, TRUE);
    gtk_box_append(GTK_BOX(row), entry);

    GtkWidget* browse = gtk_button_new_with_label("…");
    gtk_widget_set_tooltip_text(browse, "Выбрать папку");
    gtk_widget_set_size_request(browse, 40, -1);
    g_signal_connect(browse, "clicked", G_CALLBACK(on_browse_folder), entry);
    gtk_box_append(GTK_BOX(row), browse);

    fe.entry = entry;
    g_files.push_back(fe);

    return row;
}

// Рекурсивно обходим папку: подпапки — в expander, файлы — строками
static GtkWidget* build_dir_contents(const fs::path& dir,
                                     const fs::path& base,
                                     const std::map<std::string,std::string>& saved) {
    std::vector<fs::path> dirs, files;
    std::error_code ec;
    for (auto& e : fs::directory_iterator(dir, ec)) {
        if (e.is_directory()) dirs.push_back(e.path());
        else if (e.is_regular_file()) files.push_back(e.path());
    }
    std::sort(dirs.begin(), dirs.end());
    std::sort(files.begin(), files.end());

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_margin_start(box, 6);

    // Подпапки — раскрывающиеся
    for (auto& d : dirs) {
        GtkWidget* expander = gtk_expander_new(nullptr);

        GtkWidget* lbl_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        GtkWidget* folder_icon = gtk_image_new_from_icon_name("folder-symbolic");
        GtkWidget* label = gtk_label_new(d.filename().string().c_str());
        gtk_label_set_xalign(GTK_LABEL(label), 0.0);
        gtk_box_append(GTK_BOX(lbl_box), folder_icon);
        gtk_box_append(GTK_BOX(lbl_box), label);
        gtk_expander_set_label_widget(GTK_EXPANDER(expander), lbl_box);

        GtkWidget* inner = build_dir_contents(d, base, saved);
        gtk_expander_set_child(GTK_EXPANDER(expander), inner);

        gtk_box_append(GTK_BOX(box), expander);
    }

    // Файлы
    for (auto& f : files) {
        fs::path rel = fs::relative(f, base);
        GtkWidget* row = make_file_row(f, rel, saved);
        gtk_box_append(GTK_BOX(box), row);
    }

    return box;
}

// Создаём окно
static void activate(GtkApplication* app, gpointer) {
    GtkWidget* win = gtk_application_window_new(app);
    g_win = GTK_WINDOW(win);
    gtk_window_set_title(GTK_WINDOW(win), "Fastfetch Installer");
    gtk_window_set_default_size(GTK_WINDOW(win), 950, 650);

    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_top(vbox, 14);
    gtk_widget_set_margin_bottom(vbox, 14);
    gtk_widget_set_margin_start(vbox, 14);
    gtk_widget_set_margin_end(vbox, 14);

    GtkWidget* title = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(title),
        "<span size='large' weight='bold'>Установка конфигов Fastfetch</span>");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), title);

    GtkWidget* hint = gtk_label_new(
        "Файлы в папке files/. Жми на 📁 чтобы раскрыть. "
        "Меняй пути и жми «Установить» — они сохранятся в settings.conf.");
    gtk_widget_set_halign(hint, GTK_ALIGN_START);
    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_widget_add_css_class(hint, "dim-label");
    gtk_box_append(GTK_BOX(vbox), hint);

    // Папка files/ рядом с exe
    fs::path src_dir = get_exe_dir() / "files";
    if (!fs::exists(src_dir)) fs::create_directories(src_dir);

    auto saved = load_settings();

    GtkWidget* scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);

    GtkWidget* tree = build_dir_contents(src_dir, src_dir, saved);
    gtk_widget_set_margin_top(tree, 6);
    gtk_widget_set_margin_bottom(tree, 6);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), tree);
    gtk_box_append(GTK_BOX(vbox), scroll);

    // Если папка пустая — показываем подсказку
    if (g_files.empty()) {
        GtkWidget* empty = gtk_label_new(nullptr);
        gtk_label_set_markup(GTK_LABEL(empty),
            "<b>Файлов не найдено</b>\n"
            "Закинь файлы в папку <tt>files/</tt> рядом с программой\n"
            "и перезапусти установщик.");
        gtk_widget_set_halign(empty, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(empty, GTK_ALIGN_CENTER);
        gtk_widget_set_vexpand(empty, TRUE);
        gtk_box_append(GTK_BOX(vbox), empty);
    }

    // Кнопки внизу
    GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);

    GtkWidget* reset = gtk_button_new_with_label("Сбросить пути");
    gtk_widget_set_size_request(reset, -1, 44);
    g_signal_connect(reset, "clicked", G_CALLBACK(on_reset_clicked), nullptr);
    gtk_box_append(GTK_BOX(hbox), reset);

    GtkWidget* install = gtk_button_new_with_label("Установить");
    gtk_widget_add_css_class(install, "suggested-action");
    gtk_widget_set_hexpand(install, TRUE);
    gtk_widget_set_size_request(install, -1, 44);
    g_signal_connect(install, "clicked", G_CALLBACK(on_install_clicked), win);
    gtk_box_append(GTK_BOX(hbox), install);

    gtk_box_append(GTK_BOX(vbox), hbox);

    gtk_window_set_child(GTK_WINDOW(win), vbox);
    gtk_window_present(GTK_WINDOW(win));
}

// main
int main(int argc, char** argv) {
    GtkApplication* app = gtk_application_new(
        "org.local.FastfetchInstaller",
        G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), nullptr);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
