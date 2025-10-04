#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/statvfs.h>

typedef struct {
    GtkWidget *window;
    GtkWidget *cpu_label;
    GtkWidget *cpu_progress;
    GtkWidget *ram_label;
    GtkWidget *ram_progress;
    GtkWidget *gpu_label;
    GtkWidget *gpu_progress;
    GtkWidget *temp_label;
    guint timer_id;
} SystemMonitor;

// Função para obter uso da CPU
double get_cpu_usage() {
    static unsigned long long prev_idle = 0, prev_total = 0;
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) return 0.0;
    
    unsigned long long user, nice, system, idle, iowait, irq, softirq;
    fscanf(fp, "cpu %llu %llu %llu %llu %llu %llu %llu",
           &user, &nice, &system, &idle, &iowait, &irq, &softirq);
    fclose(fp);
    
    unsigned long long total = user + nice + system + idle + iowait + irq + softirq;
    unsigned long long diff_idle = idle - prev_idle;
    unsigned long long diff_total = total - prev_total;
    
    double cpu_percent = 0.0;
    if (diff_total > 0) {
        cpu_percent = 100.0 * (1.0 - ((double)diff_idle / diff_total));
    }
    
    prev_idle = idle;
    prev_total = total;
    
    return cpu_percent;
}

// Função para obter uso de memória RAM
void get_memory_info(double *used_percent, long *total_mb, long *used_mb) {
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) return;
    
    long mem_total = 0, mem_free = 0, mem_available = 0, buffers = 0, cached = 0;
    char line[256];
    
    while (fgets(line, sizeof(line), fp)) {
        sscanf(line, "MemTotal: %ld kB", &mem_total);
        sscanf(line, "MemFree: %ld kB", &mem_free);
        sscanf(line, "MemAvailable: %ld kB", &mem_available);
        sscanf(line, "Buffers: %ld kB", &buffers);
        sscanf(line, "Cached: %ld kB", &cached);
    }
    fclose(fp);
    
    *total_mb = mem_total / 1024;
    long available_mb = mem_available / 1024;
    *used_mb = *total_mb - available_mb;
    *used_percent = (*used_mb * 100.0) / *total_mb;
}

// Função para obter informações da GPU (simplified)
void get_gpu_info(char *gpu_name, double *gpu_usage) {
    // Para GPUs NVIDIA
    FILE *fp = popen("nvidia-smi --query-gpu=name,utilization.gpu --format=csv,noheader,nounits 2>/dev/null", "r");
    if (fp) {
        char line[256];
        if (fgets(line, sizeof(line), fp)) {
            char name[128];
            int usage;
            if (sscanf(line, "%127[^,], %d", name, &usage) == 2) {
                strncpy(gpu_name, name, 127);
                *gpu_usage = (double)usage;
                pclose(fp);
                return;
            }
        }
        pclose(fp);
    }
    
    // Fallback para GPUs integradas
    strcpy(gpu_name, "Placa Gráfica Detectada");
    *gpu_usage = 0.0;
}

// Função para obter temperatura da CPU
double get_cpu_temperature() {
    FILE *fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (!fp) return 0.0;
    
    int temp_millicelsius;
    fscanf(fp, "%d", &temp_millicelsius);
    fclose(fp);
    
    return temp_millicelsius / 1000.0;
}

// Função de callback para atualizar as informações
gboolean update_system_info(gpointer data) {
    SystemMonitor *monitor = (SystemMonitor *)data;
    
    // Atualizar CPU
    double cpu_usage = get_cpu_usage();
    char cpu_text[256];
    snprintf(cpu_text, sizeof(cpu_text), "CPU: %.1f%%", cpu_usage);
    gtk_label_set_text(GTK_LABEL(monitor->cpu_label), cpu_text);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(monitor->cpu_progress), cpu_usage / 100.0);
    
    // Atualizar RAM
    double ram_percent;
    long total_mb, used_mb;
    get_memory_info(&ram_percent, &total_mb, &used_mb);
    char ram_text[256];
    snprintf(ram_text, sizeof(ram_text), "RAM: %ld MB / %ld MB (%.1f%%)", used_mb, total_mb, ram_percent);
    gtk_label_set_text(GTK_LABEL(monitor->ram_label), ram_text);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(monitor->ram_progress), ram_percent / 100.0);
    
    // Atualizar GPU
    char gpu_name[128];
    double gpu_usage;
    get_gpu_info(gpu_name, &gpu_usage);
    char gpu_text[256];
    snprintf(gpu_text, sizeof(gpu_text), "GPU: %s (%.1f%%)", gpu_name, gpu_usage);
    gtk_label_set_text(GTK_LABEL(monitor->gpu_label), gpu_text);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(monitor->gpu_progress), gpu_usage / 100.0);
    
    // Atualizar temperatura
    double temp = get_cpu_temperature();
    char temp_text[256];
    snprintf(temp_text, sizeof(temp_text), "Temperatura CPU: %.1f°C", temp);
    gtk_label_set_text(GTK_LABEL(monitor->temp_label), temp_text);
    
    return TRUE; // Continuar chamando a função
}

// Função para aplicar CSS e estilo azul
void apply_css_style(GtkWidget *widget) {
    GtkCssProvider *provider = gtk_css_provider_new();
    const gchar *css_data = 
        "window {"
        "  background: linear-gradient(135deg, #1e3c72 0%, #2a5298 50%, #1e3c72 100%);"
        "  color: white;"
        "}"
        "label {"
        "  color: #e8f4fd;"
        "  font-size: 14px;"
        "  font-weight: bold;"
        "  margin: 5px;"
        "}"
        "progressbar {"
        "  margin: 5px;"
        "  min-height: 20px;"
        "}"
        "progressbar progress {"
        "  background: linear-gradient(90deg, #4fa8d8 0%, #64b5f6 100%);"
        "  border-radius: 10px;"
        "}"
        "progressbar trough {"
        "  background: rgba(255,255,255,0.2);"
        "  border-radius: 10px;"
        "}"
        ".header {"
        "  font-size: 24px;"
        "  font-weight: bold;"
        "  color: #b3e5fc;"
        "  margin: 15px;"
        "}";
    
    gtk_css_provider_load_from_data(provider, css_data, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

// Função de cleanup
void on_window_destroy(GtkWidget *widget, gpointer data) {
    SystemMonitor *monitor = (SystemMonitor *)data;
    if (monitor->timer_id > 0) {
        g_source_remove(monitor->timer_id);
    }
    gtk_main_quit();
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    
    SystemMonitor monitor = {0};
    
    // Criar janela principal
    monitor.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(monitor.window), "Monitor de Sistema - Status do PC");
    gtk_window_set_default_size(GTK_WINDOW(monitor.window), 600, 400);
    gtk_window_set_resizable(GTK_WINDOW(monitor.window), TRUE);
    gtk_container_set_border_width(GTK_CONTAINER(monitor.window), 20);
    
    // Aplicar estilo CSS
    apply_css_style(monitor.window);
    
    // Layout principal
    GtkWidget *main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(monitor.window), main_vbox);
    
    // Título
    GtkWidget *title_label = gtk_label_new("Monitor de Sistema");
    gtk_style_context_add_class(gtk_widget_get_style_context(title_label), "header");
    gtk_box_pack_start(GTK_BOX(main_vbox), title_label, FALSE, FALSE, 0);
    
    // Seção CPU
    GtkWidget *cpu_frame = gtk_frame_new("Processador (CPU)");
    gtk_box_pack_start(GTK_BOX(main_vbox), cpu_frame, TRUE, TRUE, 0);
    GtkWidget *cpu_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(cpu_frame), cpu_vbox);
    gtk_container_set_border_width(GTK_CONTAINER(cpu_vbox), 10);
    
    monitor.cpu_label = gtk_label_new("CPU: Carregando...");
    monitor.cpu_progress = gtk_progress_bar_new();
    gtk_box_pack_start(GTK_BOX(cpu_vbox), monitor.cpu_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(cpu_vbox), monitor.cpu_progress, FALSE, FALSE, 0);
    
    // Seção RAM
    GtkWidget *ram_frame = gtk_frame_new("Memória RAM");
    gtk_box_pack_start(GTK_BOX(main_vbox), ram_frame, TRUE, TRUE, 0);
    GtkWidget *ram_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(ram_frame), ram_vbox);
    gtk_container_set_border_width(GTK_CONTAINER(ram_vbox), 10);
    
    monitor.ram_label = gtk_label_new("RAM: Carregando...");
    monitor.ram_progress = gtk_progress_bar_new();
    gtk_box_pack_start(GTK_BOX(ram_vbox), monitor.ram_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ram_vbox), monitor.ram_progress, FALSE, FALSE, 0);
    
    // Seção GPU
    GtkWidget *gpu_frame = gtk_frame_new("Placa Gráfica (GPU)");
    gtk_box_pack_start(GTK_BOX(main_vbox), gpu_frame, TRUE, TRUE, 0);
    GtkWidget *gpu_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(gpu_frame), gpu_vbox);
    gtk_container_set_border_width(GTK_CONTAINER(gpu_vbox), 10);
    
    monitor.gpu_label = gtk_label_new("GPU: Carregando...");
    monitor.gpu_progress = gtk_progress_bar_new();
    gtk_box_pack_start(GTK_BOX(gpu_vbox), monitor.gpu_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(gpu_vbox), monitor.gpu_progress, FALSE, FALSE, 0);
    
    // Seção Temperatura
    GtkWidget *temp_frame = gtk_frame_new("Temperatura");
    gtk_box_pack_start(GTK_BOX(main_vbox), temp_frame, TRUE, TRUE, 0);
    GtkWidget *temp_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(temp_frame), temp_vbox);
    gtk_container_set_border_width(GTK_CONTAINER(temp_vbox), 10);
    
    monitor.temp_label = gtk_label_new("Temperatura: Carregando...");
    gtk_box_pack_start(GTK_BOX(temp_vbox), monitor.temp_label, FALSE, FALSE, 0);
    
    // Configurar timer para atualização
    monitor.timer_id = g_timeout_add(1000, update_system_info, &monitor);
    
    // Conectar sinal de fechamento
    g_signal_connect(monitor.window, "destroy", G_CALLBACK(on_window_destroy), &monitor);
    
    // Mostrar todos os widgets
    gtk_widget_show_all(monitor.window);
    
    // Primeira atualização imediata
    update_system_info(&monitor);
    
    // Loop principal
    gtk_main();
    
    return 0;
}
