#include <Arduino.h>
// required to import the actual definitions
// for lcd_init.h
#include <uix.hpp>
using namespace gfx;
using namespace uix;
#include <ui.hpp>
#include <interface.hpp>
#include <tft_spi.hpp>
#include <ili9341.hpp>
using namespace arduino;


using bus_type = tft_spi_ex<3,LCD_SS_PIN,SPI_MODE0>;
using lcd_type = ili9341<LCD_DC,LCD_RESET,LCD_BACKLIGHT,bus_type,3,true,400,200>;

lcd_type lcd;

// label string data
static char cpu_sz[32];
static char gpu_sz[32];

// so we don't redraw temps
static int old_cpu_temp=-1;
static int old_gpu_temp=-1;

// signal timer for disconnection detection
static uint32_t timeout_ts = 0;

static void uix_wait(void* state) {
    draw::wait_all_async(lcd);
}
static void uix_flush(const rect16& bounds, 
                    const void* bmp, 
                    void* state) {
    const_bitmap<screen_t::pixel_type,screen_t::palette_type> cbmp(size16(bounds.width(),bounds.height()),bmp);
    draw::bitmap_async(lcd,bounds,cbmp,cbmp.bounds());
}

void setup() {
    Serial.begin(115200);
    lcd.initialize();
    // initialize the main screen (ui.cpp)
    main_screen.wait_flush_callback(uix_wait);
    main_screen_init(uix_flush);
    main_screen.invalidate(main_screen.bounds());
}

void loop() {
    // timeout for disconnection detection (1 second)
    if(timeout_ts!=0 && millis()>timeout_ts+1000) {
        timeout_ts = 0;
        disconnected_label.visible(true);
        disconnected_svg.visible(true);
    }

    main_screen.update();
    
    // listen for incoming serial
    int i = Serial.read();
    float tmp;
    if(i>-1) { // if data received...
        // reset the disconnect timeout
        timeout_ts = millis(); 
        disconnected_label.visible(false);
        disconnected_svg.visible(false);
        switch(i) {
            case read_status_t::command: {
                read_status_t data;
                if(sizeof(data)==Serial.readBytes((char*)&data,sizeof(data))) {
                    // update the CPU graph buffer (usage)
                    if (cpu_buffers[0].full()) {
                        cpu_buffers[0].get(&tmp);
                    }
                    cpu_buffers[0].put(data.cpu_usage/100.0f);
                    // update the bar and label values (usage)
                    cpu_values[0]=data.cpu_usage/100.0f;
                    // update the CPU graph buffer (temperature)
                    if (cpu_buffers[1].full()) {
                        cpu_buffers[1].get(&tmp);
                    }
                    cpu_buffers[1].put(data.cpu_temp/(float)data.cpu_temp_max);
                    if(data.cpu_temp>cpu_max_temp) {
                        cpu_max_temp = data.cpu_temp;
                    }
                    // update the bar and label values (temperature)
                    cpu_values[1]=data.cpu_temp/(float)data.cpu_temp_max;
                    // force a redraw of the CPU bar and graph
                    cpu_graph.invalidate();
                    cpu_bar.invalidate();
                    // update CPU the label (temperature)
                    if(old_cpu_temp!=data.cpu_temp) {
                        old_cpu_temp = data.cpu_temp;
                        sprintf(cpu_sz,"%dC",data.cpu_temp);
                        cpu_temp_label.text(cpu_sz);
                    }
                    // update the GPU graph buffer (usage)
                    if (gpu_buffers[0].full()) {
                        gpu_buffers[0].get(&tmp);
                    }
                    gpu_buffers[0].put(data.gpu_usage/100.0f);
                    // update the bar and label values (usage)
                    gpu_values[0] = data.gpu_usage/100.0f;
                    // update the GPU graph buffer (temperature)
                    if (gpu_buffers[1].full()) {
                        gpu_buffers[1].get(&tmp);
                    }
                    gpu_buffers[1].put(data.gpu_temp/(float)data.gpu_temp_max);
                    if(data.gpu_temp>gpu_max_temp) {
                        gpu_max_temp = data.gpu_temp;
                    }
                    // update the bar and label values (temperature)
                    gpu_values[1] = data.gpu_temp/(float)data.gpu_temp_max;
                    // force a redraw of the GPU bar and graph
                    gpu_graph.invalidate();
                    gpu_bar.invalidate();
                    // update GPU the label (temperature)
                    if(old_gpu_temp!=data.gpu_temp) {
                        old_gpu_temp = data.gpu_temp;
                        sprintf(gpu_sz,"%dC",data.gpu_temp);
                        gpu_temp_label.text(gpu_sz);   
                    }
                } else {
                    // eat bad data
                    while(-1!=Serial.read());
                }
            }
            break;
            default:
                // eat unrecognized data
                while(-1!=Serial.read());
                break;
        };
    }
}