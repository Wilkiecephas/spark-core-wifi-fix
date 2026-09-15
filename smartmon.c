#define SAVE_SCREEN
#define USE_TI89
#define NO_CALC_DETECT
#define NO_AMS_CHECK
#include <tigcclib.h>

// Draw filled rectangle
static inline void fill_rect(short x0, short y0, short x1, short y1, short attr) {
    WIN_RECT r;
    r.x0 = x0; r.y0 = y0; r.x1 = x1; r.y1 = y1;
    DrawClipRect(&r, ScrRect, attr);
}

// Telemetry state
typedef struct {
    unsigned char temp;
    unsigned char hum;
    unsigned short dist;
    unsigned short light;
    unsigned char alert;       // bit 0: motion, bit 1: breach (<20cm)
    unsigned short rx_count;   // Total valid telemetry packets received
} Telemetry;

static unsigned char rx_buf[8];
static int rx_idx = 0;

// Non-blocking drain of incoming DBus packets via TI-OS receive queue
void poll_telemetry(Telemetry *data, unsigned char *history, int *hist_idx, int graph_mode) {
    char ch;
    // OSReadLinkBlock reads from the hardware DBus receive queue in the background
    while (OSReadLinkBlock(&ch, 1) > 0) {
        unsigned char b = (unsigned char)ch;
        if (rx_idx == 0) {
            if (b == 0xAA) {
                rx_buf[0] = b;
                rx_idx = 1;
            }
        } else {
            rx_buf[rx_idx++] = b;
            if (rx_idx == 8) {
                // Complete 8-byte frame: [0xAA, temp, hum, distH, distL, lightH, lightL, alert]
                data->temp  = rx_buf[1];
                data->hum   = rx_buf[2];
                data->dist  = ((unsigned short)rx_buf[3] << 8) | rx_buf[4];
                data->light = ((unsigned short)rx_buf[5] << 8) | rx_buf[6];
                data->alert = rx_buf[7];
                data->rx_count++;
                
                // Add to rolling history based on active graph view
                if (graph_mode == 0) {
                    history[*hist_idx] = (data->dist > 300) ? 255 : (data->dist * 255) / 300;
                } else if (graph_mode == 1) {
                    history[*hist_idx] = (data->temp * 255) / 60;
                } else {
                    history[*hist_idx] = (data->light * 255) / 4095;
                }
                *hist_idx = (*hist_idx + 1) % 60;
                
                rx_idx = 0; // Ready for next packet
            }
        }
    }
}

void draw_dashboard(Telemetry *data, unsigned char *history, int hist_idx, int graph_mode) {
    char buf[32];
    int t_bar, h_bar, d_bar;
    int prev_x, prev_y, i;
    
    ClrScr();
    FontSetSys(F_4x6);
    
    // Top Banner
    DrawLine(0, 0, 159, 0, A_NORMAL);
    DrawLine(0, 8, 159, 8, A_NORMAL);
    fill_rect(0, 0, 159, 8, A_REVERSE);
    DrawStr(3, 2, "SMARTMON TI-89 Ti", A_XOR);
    
    // Link Status & Packet Counter
    if (data->rx_count > 0) {
        sprintf(buf, "RX:%u", data->rx_count);
        DrawStr(78, 2, buf, A_XOR);
    } else {
        DrawStr(75, 2, "LINK:WAIT", A_XOR);
    }
    
    // Alert Status
    if (data->alert & 0x02) {
        DrawStr(124, 2, "[BREACH]", A_XOR);
    } else if (data->alert & 0x01) {
        DrawStr(124, 2, "[MOTION]", A_XOR);
    } else {
        DrawStr(124, 2, "[SECURE]", A_XOR);
    }

    // Left Panel: Gauge Readouts
    // Temperature
    sprintf(buf, "TEMP: %d C", data->temp);
    DrawStr(4, 13, buf, A_NORMAL);
    fill_rect(4, 20, 54, 24, A_NORMAL);
    t_bar = (data->temp * 50) / 60;
    if (t_bar > 48) t_bar = 48;
    fill_rect(5, 21, 5 + t_bar, 23, A_REVERSE);

    // Humidity
    sprintf(buf, "HUM:  %d %%", data->hum);
    DrawStr(4, 28, buf, A_NORMAL);
    fill_rect(4, 35, 54, 39, A_NORMAL);
    h_bar = (data->hum * 50) / 100;
    if (h_bar > 48) h_bar = 48;
    fill_rect(5, 36, 5 + h_bar, 38, A_REVERSE);

    // Distance
    sprintf(buf, "DIST: %d cm", data->dist);
    DrawStr(4, 43, buf, A_NORMAL);
    fill_rect(4, 50, 54, 54, A_NORMAL);
    d_bar = (data->dist * 50) / 300;
    if (d_bar > 48) d_bar = 48;
    fill_rect(5, 51, 5 + d_bar, 53, A_REVERSE);

    // Light
    sprintf(buf, "LUX:  %d", data->light);
    DrawStr(4, 58, buf, A_NORMAL);

    // Right Panel: Real-time Strip-Chart
    DrawLine(62, 10, 62, 88, A_NORMAL);
    
    // Graph Title
    if (graph_mode == 0) DrawStr(66, 12, "LIVE: DISTANCE (cm)", A_NORMAL);
    else if (graph_mode == 1) DrawStr(66, 12, "LIVE: TEMP (C)", A_NORMAL);
    else DrawStr(66, 12, "LIVE: LIGHT (ADC)", A_NORMAL);
    
    // Graph Box (x: 65 to 157, y: 20 to 86)
    fill_rect(65, 20, 157, 86, A_NORMAL);
    
    // Plot rolling history
    prev_x = 0;
    prev_y = 0;
    for (i = 0; i < 60; i++) {
        int idx = (hist_idx + i) % 60;
        int val = history[idx];
        int plot_x = 67 + (i * 90) / 60;
        int plot_y = 84 - (val * 62) / 255;
        if (plot_y < 22) plot_y = 22;
        if (plot_y > 84) plot_y = 84;
        
        if (i > 0) {
            DrawLine(prev_x, prev_y, plot_x, plot_y, A_NORMAL);
        }
        prev_x = plot_x;
        prev_y = plot_y;
    }

    // Bottom Navigation Bar
    DrawLine(0, 90, 159, 90, A_NORMAL);
    DrawStr(3, 93, "F1:DIST  F2:TEMP  F3:LIGHT   [ESC:EXIT]", A_NORMAL);
}

void _main(void) {
    Telemetry data = {31, 50, 150, 800, 0, 0};
    unsigned char history[60];
    int hist_idx = 0;
    int graph_mode = 0; // 0=dist, 1=temp, 2=light
    int running = 1;
    
    memset(history, 50, sizeof(history));
    rx_idx = 0;
    
    // Open hardware DBus link communication via TIOS
    // Enables background interrupt-driven link receive queue
    OSLinkOpen();
    
    while (running) {
        if (kbhit()) {
            short k = ngetchx();
            if (k == KEY_ESC) {
                running = 0;
                break;
            } else if (k == KEY_F1) {
                graph_mode = 0;
            } else if (k == KEY_F2) {
                graph_mode = 1;
            } else if (k == KEY_F3) {
                graph_mode = 2;
            }
        }
        
        // Drain any bytes received by hardware DBus controller into our packet parser
        poll_telemetry(&data, history, &hist_idx, graph_mode);
        
        draw_dashboard(&data, history, hist_idx, graph_mode);
    }
    
    // Close link port and restore OS state
    OSLinkClose();
}
