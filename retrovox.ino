/* Copyright (c) 2020, Peter Barrett
**
** Permission to use, copy, modify, and/or distribute this software for
** any purpose with or without fee is hereby granted, provided that the
** above copyright notice and this permission notice appear in all copies.
**
** THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR
** BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
** OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
** WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OpR OTHER TORTIOUS ACTION,
** ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
** SOFTWARE.
*/

#include "esp_wifi.h"
#include "tcpip_adapter.h"
#include "esp_event_loop.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_int_wdt.h"
#include "esp_task_wdt.h"
#include "esp_heap_caps.h"
#include "soc/rtc.h"
#include "freertos/task.h"

#include "driver/i2s.h"
#include "driver/i2c.h"
#include "driver/rmt.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_spiffs.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include <map>
#include <vector>
#include <string>
using namespace std;

#include "src/utils.h"
#include "src/terminal.h"
#include "src/streamer.h"
#include "src/retro_vox.h"

#include "WiFi.h"

void draw_frotz(int keys);
void draw_basic(int keys);

#define PERF
#define PIXEL_IO     26

//================================================================================
//================================================================================

class PixelSPI {
public:
  uint16_t* _ledbuf;
  spi_device_handle_t _spi;
  const int BUF_SIZE = 3*4; // 4 bytes of spi pwm output per r,g and b

  void init()
  {
    spi_bus_config_t buscfg = {0};
    buscfg.miso_io_num = -1;
    buscfg.sclk_io_num = -1;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = BUF_SIZE;
    buscfg.mosi_io_num = PIXEL_IO;
  
    spi_device_interface_config_t devcfg = {0};
    devcfg.clock_speed_hz = 3.2 * 1000 * 1000;
    devcfg.spics_io_num = -1;
    devcfg.queue_size = 1;
  
    spi_bus_initialize(HSPI_HOST, &buscfg, 2);
    spi_bus_add_device(HSPI_HOST, &devcfg, &_spi);
    _ledbuf = (uint16_t*)heap_caps_malloc(BUF_SIZE, MALLOC_CAP_DMA);
  }
  
  const uint16_t pwm_pat[16] = {
    0x8888,0x8C88,0xC888,0xCC88,0x888C,0x8C8C,0xC88C,0xCC8C,
    0x88C8,0x8CC8,0xC8C8,0xCCC8,0x88CC,0x8CCC,0xC8CC,0xCCCC
  };
  
  void write(const uint8_t* p, int n)
  {
    int j = 0;
    for (int i = 0; i < n; i++) {
      _ledbuf[j++] = pwm_pat[(p[i] >> 4) & 0xF];  // 4 bytes of spi output per r,g and b
      _ledbuf[j++] = pwm_pat[p[i] & 0xF];
    }
  
    spi_transaction_t t = {0};
    t.length = BUF_SIZE * 8;        // in bits
    t.tx_buffer = _ledbuf;
    int err = spi_device_transmit(_spi, &t);
    ESP_ERROR_CHECK(err);
  }

  void set(uint8_t r, uint8_t g, uint8_t b)
  {
    uint8_t p[3] = {g,r,b};
    write(p,3);
  }
  
  void set(uint32_t c)
  {
    return set((c >> 16),(c >> 8),c);
  }
};
PixelSPI _pixel;

void pixel_set(uint32_t rgb)
{
   _pixel.set(rgb);
}

int led_main(int argc, const char * argv[])
{
    int r,g,b;
    if (sscanf(argv[1],"%2X%2X%2X",&r,&g,&b) != 3)
        return -1;
    _pixel.set(r,g,b);
    printf("led to %d,%d,%d\n",r,g,b);
    return 0;
}

//================================================================================
//================================================================================
// emulate a 24LC256
// addr is 0x50

#define I2C_SLAVE_SDA_IO     21    /*!<gpio number for i2c slave data */
#define I2C_SLAVE_SCL_IO     23    /*!<gpio number for i2c slave clock  */

uint8_t _bbuf[512];
volatile uint32_t _bwrite = 0;
uint32_t _bread = 0;

enum IC2_state {
  IDLE,
  START,
  BYTE_IN,
  BYTE_OUT,
  BYTE_OUT_FINAL,
  ACK0_IN,
  ACK1_IN,
  NAK1_IN,
  ACK0,
  ACK1,
  NACK,
  STOP,
};

void  ic2_byte(uint8_t b)
{
    _bbuf[_bwrite++ & 0x1FF] = b;
}

inline void IRAM_ATTR gpio_write(int n, int v)
{
    if (v)
      GPIO.out_w1ts = 1 << n;  // set
    else
      GPIO.out_w1tc = 1 << n;  // clear
}

inline int IRAM_ATTR gpio_read(int n)
{
    return (GPIO.in >> n) & 0x1;
}

static void IRAM_ATTR sda_out(uint8_t v)
{
  GPIO.enable_w1ts = ((uint32_t)1 << I2C_SLAVE_SDA_IO); // set for output
  gpio_write(I2C_SLAVE_SDA_IO,v);
}

void IRAM_ATTR sda_in()
{
  GPIO.enable_w1tc = ((uint32_t)1 << I2C_SLAVE_SDA_IO); // clear for input
}

uint8_t IRAM_ATTR scl()
{
  return gpio_read(I2C_SLAVE_SCL_IO);
}

uint8_t IRAM_ATTR sda()
{
  return gpio_read(I2C_SLAVE_SDA_IO);
}

void IRAM_ATTR mark(uint8_t v)
{
    gpio_write(23,v);
}

void IRAM_ATTR mark2(uint8_t v)
{
    gpio_write(19,v);
}

char hex[16+1] = "0123456789ABCDEF";
void IRAM_ATTR nv(const char* n, uint8_t v = 0)
{
  while (*n)
    ic2_byte(*n++);
  if (v) {
    ic2_byte(':');
    ic2_byte(hex[v >> 4]);
    ic2_byte(hex[v & 0x0F]);
  }
  ic2_byte('\n');
}

//================================================================================
//================================================================================
//  emulate a 24C256 eeprom

#if 0
class FlashEmu;
FlashEmu* that = 0;

static void IRAM_ATTR sda_isr_(void*);
static void IRAM_ATTR scl_isr_(void*);
    
class FlashEmu {
public:
    IC2_state state;
    uint8_t cmd;
    uint8_t seq;
    uint8_t dirty;
    uint16_t addr;
    uint16_t saddr;
    uint8_t n = 0;
    uint8_t rise_d = 1;
    uint8_t flash_data[32*1024];    // EWWW!
    uint8_t b;
    
    FlashEmu()
    {
        that = this;
    }
    
    uint8_t IRAM_ATTR read_byte()
    {
        if (addr >= 32*1024)
            return 0xFF;
        return flash_data[addr++];
    }
    
    void IRAM_ATTR byte_in(uint8_t b)
    {
        if (cmd == 0) {
            cmd = b;
            seq = 0;  // skip address if reading?
            return;
        }
        switch (seq++) {
            case 0:
                addr = b << 8;
                break;
            case 1:
                addr |= b;
                saddr = addr;
                break;
            default:
                if (addr < 32*1024) {
                    flash_data[addr++] = b;
                    dirty |= 1;
                }
                break;
        }
    }
    
    static void IRAM_ATTR sda_isr(void*)
    {
        if (!scl())
            return;
        uint8_t d = sda();
        if ((that->state == IDLE) && (d == 0)) {              // start condition
            that->state = BYTE_IN;
            that->cmd = 0;
            that->n = 0;
            mark2(0);
        } else if (((that->state == BYTE_IN) || (that->state == BYTE_OUT)) && (d != that->rise_d)) {    // stop condition
            if (that->dirty)
                that->dirty = 2;  // flush needed
            that->state = IDLE;
            that->n = 0;
            mark2(1);
        }
    }
    
    void IRAM_ATTR clk_pos_edge()
    {
        uint8_t d = sda();
        switch (state) {
            case BYTE_IN:
                b = (b << 1) | d;
                if (++n == 8) {
                    byte_in(b);
                    n = 0;
                    state = ACK0;  // wait for falling clock then output ack
                }
                break;
                
            case ACK0_IN:       // capture inbound ack
                state = d ? NAK1_IN : ACK1_IN;
                break;
        }
    }i
    
    // falling clock
    void IRAM_ATTR clk_neg_edge()
    {
        switch (state) {
                // RX
            case ACK0:
                sda_out(0);     // start ack
                state = ACK1;   // hold ack until next falling clock
                break;
                
            case ACK1:        // Falling edge of ACK from last byte
                sda_in();
                state = (cmd&1) ? BYTE_OUT : BYTE_IN;
                if (state == BYTE_OUT) {
                    n = 0;
                    b = read_byte();    // start sending first byte after command FALL THRU
                } else {
                    break;
                }
                
                // TX
            case BYTE_OUT:
                sda_out(b & 0x80);
                b <<= 1;
                n++;              // send another bit
                if (n == 8) {
                    state = BYTE_OUT_FINAL;
                    n = 0;
                }
                break;
                
            case BYTE_OUT_FINAL:  // falling edge of final bit
                sda_in();
                state = ACK0_IN;    // now wait for rising ack clock
                break;
                
            case ACK1_IN:         // falling of ack in clock
                state = BYTE_OUT;
                b = read_byte();
                sda_out(b & 0x80);
                b <<= 1;
                n = 1;               // set first bit
                break;
                
            case NAK1_IN:          // End of read
                state = BYTE_IN;
                n = 0;
                break;
        }
    }
    
    void IRAM_ATTR scl_isr(void*)
    {
        mark(0);
        while (scl())
            ;
        that->clk_neg_edge();
        while (!scl())
            ;
        that->clk_pos_edge();
        that->rise_d = sda();
        mark(1);
    }
    
    void update()
    {
        while (_bread < _bwrite) {
            char b = _bbuf[_bread++ & 0x1FF];
            printf("%c",b);
        }
        if (dirty & 2) {
            printf("WR %04X %04X ",saddr,addr);
            dirty = 0;
            for (int i = saddr; i < addr; i++)
                printf("%02X",flash_data[i]);
            printf("\n");
        }
    }
    
    void init()
    {
        memset(flash_data,0xFF,sizeof(flash_data));
        
        gpio_set_intr_type((gpio_num_t)I2C_SLAVE_SCL_IO,GPIO_INTR_NEGEDGE); // GPIO_INTR_POSEDGE, GPIO_INTR_NEGEDGE
        gpio_set_intr_type((gpio_num_t)I2C_SLAVE_SDA_IO,GPIO_INTR_ANYEDGE);
        gpio_set_direction((gpio_num_t)I2C_SLAVE_SCL_IO,GPIO_MODE_INPUT);
        gpio_set_direction((gpio_num_t)I2C_SLAVE_SDA_IO,GPIO_MODE_INPUT);
        
        gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_IRAM);
        gpio_isr_handler_add((gpio_num_t)I2C_SLAVE_SCL_IO,scl_isr_,0);   // clock only
        gpio_isr_handler_add((gpio_num_t)I2C_SLAVE_SDA_IO,sda_isr_,0);   // data only
        gpio_intr_enable((gpio_num_t)I2C_SLAVE_SCL_IO);
        gpio_intr_enable((gpio_num_t)I2C_SLAVE_SDA_IO);
    }
};

static void IRAM_ATTR sda_isr_(void*)
{
  that->sda_isr(0);
}

static void IRAM_ATTR scl_isr_(void*)
{
  that->scl_isr(0);
}

FlashEmu _flash;
#endif

//================================================================================
//================================================================================
//  SPI input

#define READY_IO 5
#define RDX2_IO 16

char bug[16];
int bi = 0;

static void IRAM_ATTR spi_isr(void*)
{
  uint8_t b = bug[bi++ & 0xF];
  b = bi;
  for (int n = 0; n < 8; n++) {
    while (scl())   // wait for it to go lo
      ;
    sda_out(b & 0x80);
    b <<= 1;
    while (!scl())  // wait for it to go hi
      ;
  }
  sda_in();
}

// emulation of
void bitbang_spi()
{
    memcpy(bug,"String of chars",16);
    gpio_set_intr_type((gpio_num_t)READY_IO,GPIO_INTR_NEGEDGE); // GPIO_INTR_POSEDGE, GPIO_INTR_NEGEDGE
    gpio_set_direction((gpio_num_t)READY_IO,GPIO_MODE_INPUT);
    gpio_set_direction((gpio_num_t)I2C_SLAVE_SCL_IO,GPIO_MODE_INPUT);

    gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_IRAM);
    gpio_isr_handler_add((gpio_num_t)READY_IO,spi_isr,0);
    gpio_intr_enable((gpio_num_t)READY_IO);
}

//================================================================================
//================================================================================
// Audio
// Single pin with second order software PDM modulation
// see https://www.sigma-delta.de/sdopt/index.html for the tool used to design the Delta Sigma modulator
// 48000hz sample rate, 32x oversampling for PDM
// PDM output on pin 18

void audio_init_hw()
{
    //pinMode(IR_PIN,INPUT);   
    ets_printf("audio_init_hw core%d\n",xPortGetCoreID());
    i2s_config_t i2s_config;
    i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    i2s_config.sample_rate = 44100;
    i2s_config.bits_per_sample = (i2s_bits_per_sample_t)16,
    i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    i2s_config.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S);
    i2s_config.dma_buf_count = 4;
    i2s_config.dma_buf_len = 1024; // 2k * 2 bytes for audio buffers, pretty tight
    i2s_config.use_apll = false;
    i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1 | ESP_INTR_FLAG_SHARED;
    i2s_driver_install((i2s_port_t)1, &i2s_config, 0, NULL);
    
    i2s_pin_config_t pin_config;
    pin_config.bck_io_num = I2S_PIN_NO_CHANGE;
    pin_config.ws_io_num = I2S_PIN_NO_CHANGE;
    pin_config.data_out_num = 18;
    pin_config.data_in_num = I2S_PIN_NO_CHANGE;
    i2s_set_pin((i2s_port_t)1, &pin_config);
}

// for each source sample, emit a certain number of bits
// 44100 - 32
// 22050 - 64
// 10khz - 141.12
int pdm_first_order2(uint16_t* dst, const int16_t* src, int len, int dbl = 0)
{
    static uint16_t acc = 0;
    static uint16_t err = 0;
    uint16_t* start = dst;
    while (len--) {
        uint16_t s = *src++ + 0x8000;
        int j = dbl ? 4 : 2;
        while (j--) {
            int i = 16;
            while (i--) {
                acc <<= 1;
                 if(s >= err) {
                   acc |= 1;
                   err += 0xFFFF-s;
                 } else
                   err -= s;
            }
            *dst++ = acc;
        }
    }
    return dst-start;
}


// 32x oversample
void pdm_second_order(uint16_t* dst, const int16_t* src, int len, int32_t a1 = 0x7FFF*1.18940, int a2 = 0x7FFF*2.12340)
{
    static int32_t _i0;
    static int32_t _i1;
    static int32_t _i2;
    int32_t i0 = _i0;   // force compiler to use registers
    int32_t i1 = _i1;
    int32_t i2 = _i2;
    
    uint32_t b = 0;
    int32_t s = 0;
    len <<= 1;
    while (len--)
    {
        if (len & 1)
            s = *src++;
        //i0 = (i0 + s) >> 1; // lopass
        i0 = s >> 1;
        int n = 16;
        while (n--) {
            b <<= 1;
            if (i2 >= 0) {
                i1 += i0 - a1 - (i2 >> 7);  // feedback
                i2 += i1 - a2;
                b |= 1;
            } else {
                i1 += i0 + a1 - (i2 >> 7);  // feedback
                i2 += i1 + a2;
            }
        }
        *dst++ = b;
    }
    _i0 = i0;
    _i1 = i1;
    _i2 = i2;
}

const int16_t _sin32[32] = {
  0x0000,0xE708,0xCF05,0xB8E4,0xA57F,0x9594,0x89C0,0x8277,
  0x8001,0x8277,0x89C0,0x9594,0xA57F,0xB8E4,0xCF05,0xE708,
  0x0000,0x18F8,0x30FB,0x471C,0x5A81,0x6A6C,0x7640,0x7D89,
  0x7FFF,0x7D89,0x7640,0x6A6C,0x5A81,0x471C,0x30FB,0x18F8,
};

uint8_t _beep;
void beep()
{
  _beep = 5;
}

// this routine turns PCM into PDM. Also handles generating slience and beeps
void write_pcm_16(const int16_t* s, int n, int channels)
{
  uint16_t samples_data[128*2];
  
  //PLOG(PDM_START);
  if (_beep) {
     int16_t* b = (int16_t*)samples_data + 128; // both src and dst buffer to save stack
     for (int i = 0; i < 128; i++)
        b[i] = _sin32[i & 31] >> 2;    // sinewave beep plz
    _beep--;
    pdm_second_order(samples_data,b,128);
  } else {
    if (s)
      pdm_first_order2(samples_data,s,n);
      //pdm_second_order(samples_data,s,n);
    else {
        for (int i = 0; i < 128*2; i++)
          samples_data[i] = 0xAAAA;     // PDM silence
    }
  }
  size_t i2s_bytes_write;
  i2s_write((i2s_port_t)1, samples_data, sizeof(samples_data), &i2s_bytes_write, portMAX_DELAY);  // audio thread will block here
  //PLOG(PDM_END);
}

//================================================================================
//================================================================================
// Store the pts of the current movie in non-volatile storage

nvs_handle _nvs;
static int nv_open()
{
  if (_nvs || (nvs_open("espflix", NVS_READWRITE, &_nvs) == 0))
    return 0;
  return -1;
}

// force key to max 15 chars
static const char* limit_key(const char* key)
{
  int n = strlen(key);
  return (n < 15) ? key : key + (n-15);
}

int64_t nv_read(const char* key)
{
  int64_t pts = 0;
  if (nv_open() == 0)
    nvs_get_i64(_nvs,limit_key(key),&pts);
  return pts;
}

void nv_write(const char* key, int64_t pts)
{
  if (nv_open() == 0)
    nvs_set_i64(_nvs,limit_key(key),pts);
}

//================================================================================
//================================================================================
// WIFI

const char* AP_SSID     = "retrovox";
const char* AP_PASSWORD = "123456789";


#define PLOG(_x)
#define PLOGV(_x,_v)

extern
EventGroupHandle_t _event_group;
WiFiState _wifi_state = WIFI_NONE;

// manually entered ssid/password
void wifi_join(const char* ssid, const char* pwd)
{
    wifi_config_t wifi_config = {0};
    strcpy((char*)wifi_config.sta.ssid, ssid);
    strcpy((char*)wifi_config.sta.password, pwd);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_connect();
    _wifi_state = CONNECTING;
}

void wifi_scan()
{
    if (_wifi_state == SCANNING)
      return;
    wifi_scan_config_t config = {0};
    config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    esp_wifi_scan_start(&config,false);
    _wifi_state = SCANNING;
}

void wifi_disconnect()
{
    esp_wifi_disconnect();
}

/*
static const int AP_STARTED_BIT    = BIT0;
static const int AP_HAS_IP6_BIT    = BIT1;
static const int AP_HAS_CLIENT_BIT = BIT2;

static const int STA_STARTED_BIT   = BIT3;
static const int STA_CONNECTED_BIT = BIT4;
static const int STA_HAS_IP_BIT    = BIT5;
static const int STA_HAS_IP6_BIT   = BIT6;

static const int WIFI_SCANNING_BIT = BIT11;
static const int WIFI_SCAN_DONE_BIT= BIT12;
*/

vector<string> _ssidv;
std::map<string,int> _ssids;
vector<string> get_ssids()
{
  if (_ssidv.empty())
    wifi_scan();
  return _ssidv;
}

uint16_t _wifi_status = 0;
bool ap_started()
{
  return _wifi_status & AP_STARTED_BIT;
}

bool sta_has_ip()
{
  return _wifi_status & STA_HAS_IP_BIT;
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    //ets_printf("EVENT %d\n",event->event_id);
    tcpip_adapter_ip_info_t ip_info;
    
    switch(event->event_id)
    {
      case SYSTEM_EVENT_STA_START:
          ets_printf("SYSTEM_EVENT_STA_START\n");
          _wifi_status |= STA_STARTED_BIT;
          break;

      case SYSTEM_EVENT_STA_CONNECTED:
          ets_printf("SYSTEM_EVENT_STA_CONNECTED\n");
          _wifi_status |= STA_CONNECTED_BIT;
          break;
        
      case SYSTEM_EVENT_STA_GOT_IP:
          ets_printf("ESP32 Connected to WiFi SYSTEM_EVENT_STA_GOT_IP\n");
          
          tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
          ets_printf("STA ip:  %s\n", ip4addr_ntoa(&ip_info.ip));
          ets_printf("STA gateway:  %s\n", ip4addr_ntoa(&ip_info.gw));
          ets_printf("STA netmask:  %s\n", ip4addr_ntoa(&ip_info.netmask));
      
          tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info);
          ets_printf("AP IP Address:  %s\n", ip4addr_ntoa(&ip_info.ip));
          _wifi_status |= STA_HAS_IP_BIT;
          _wifi_state = CONNECTED;
          pixel_set(0x00FF00); // green
          break;

      case SYSTEM_EVENT_STA_DISCONNECTED:
          ets_printf("SYSTEM_EVENT_STA_DISCONNECTED %d\n",event->event_info.disconnected.reason);
          _wifi_status &= ~STA_CONNECTED_BIT;
          _wifi_state = WIFI_NONE;
          pixel_set(0xFF0000); // red
          break;

      case SYSTEM_EVENT_SCAN_DONE:
          {
            uint16_t apCount = 0;
            esp_wifi_scan_get_ap_num(&apCount);
            ets_printf("SYSTEM_EVENT_SCAN_DONE %d %d\n",apCount,sizeof(wifi_ap_record_t)*apCount);
            wifi_ap_record_t* list = new wifi_ap_record_t[apCount];  // careful of large stack 16*80 bytes. Limitation of the 16 highest power APs
            esp_wifi_scan_get_ap_records(&apCount, list);

            // store best rssi
            _ssids.clear();
            for (uint16_t i = 0; i < apCount; i++) {
                wifi_ap_record_t *ap = list + i;
                const char* s = (const char *)ap->ssid;
                if (!s[0])
                  continue;
               // printf("\"%s\",%d,%d\n",s,ap->rssi,ap->authmode);
                if (_ssids.find(s) == _ssids.end()) {    // uniquify
                  _ssids[s] = ap->rssi;
                  _ssidv.push_back(s);
                 // _ssids[ssid] = (ap->rssi << 8) | ap->authmode;
                } else {
                  
                }
            }
            delete [] list;
          }
          _wifi_status &= ~WIFI_SCANNING_BIT;
          _wifi_status |= WIFI_SCAN_DONE_BIT;
          _wifi_state = SCAN_COMPLETE;
          break;

      case SYSTEM_EVENT_AP_START:
          ets_printf("SYSTEM_EVENT_AP_START\n");
          tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info);
          ets_printf("AP ip: %s\n", ip4addr_ntoa(&ip_info.ip));
          ets_printf("AP gateway: %s\n", ip4addr_ntoa(&ip_info.gw));
          ets_printf("AP netmask: %s\n", ip4addr_ntoa(&ip_info.netmask));
          _wifi_status |= AP_STARTED_BIT;
          break;
          
      case SYSTEM_EVENT_AP_STOP:
          ets_printf("SYSTEM_EVENT_AP_STOP\n");
          _wifi_status &= ~AP_STARTED_BIT;
          break;

      case SYSTEM_EVENT_AP_STACONNECTED:
          ets_printf("SYSTEM_EVENT_AP_STACONNECTED\n"); // wifi_event_ap_staconnected_t
          break;
          
      case SYSTEM_EVENT_AP_STADISCONNECTED:
          ets_printf("SYSTEM_EVENT_AP_STADISCONNECTED\n");
          esp_wifi_disconnect(); // eh?
          esp_wifi_connect(); // eh?
          break;
    }
    return ESP_OK;
}

void poll_wifi()
{
  return;
}

void init_wifi()
{
    mem("before tcpip_adapter_init");
    //esp_log_level_set("wifi", ESP_LOG_VERBOSE);  
    tcpip_adapter_init();
    esp_event_loop_init(event_handler, NULL);
    mem("after tcpip_adapter_init");
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.static_tx_buf_num = 8;
    cfg.static_rx_buf_num = 16;
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_APSTA); // WIFI_MODE_STA;

    wifi_config_t wifi_config = {0};
    strcpy((char*)wifi_config.ap.ssid,AP_SSID);
    strcpy((char*)wifi_config.ap.password,AP_PASSWORD);
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));

    delay(100); // TODO

    // force to 192.168.1.1
    tcpip_adapter_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 192, 168, 1, 1);
    IP4_ADDR(&ip_info.gw, 192, 168, 1, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP);
    tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info);
    tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP);
        
    esp_wifi_start();
    esp_wifi_connect();
    mem("after esp_wifi_start");
    
    //wifi_join("Mojo2","DEADC0ED");
}

string to_string(int i);

void join_ssid(const string& ssid, const string& pass)
{
  wifi_join(ssid.c_str(),pass.c_str());
}

string wifi_ssid()
{
    wifi_config_t wifi_config = {0};
    esp_wifi_get_config(ESP_IF_WIFI_STA,&wifi_config);
   // printf("###### %s %s\n",(char*)wifi_config.sta.ssid,(char*)wifi_config.sta.password);
    return (char*)wifi_config.sta.ssid;
}

void wifi_dump()
{
    tcpip_adapter_ip_info_t ip_info = {0};
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
    printf("STA ip:  %s\n", ip4addr_ntoa(&ip_info.ip));
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info);
    printf("AP ip:  %s\n\n", ip4addr_ntoa(&ip_info.ip));
    string id = wifi_ssid();
    if (!id.empty())
      printf("SSID: %s\n\n",id.c_str());

    printf("Wi-Fi Networks:\n");
    vector<string> s;
    for (int i = 0; i < 10; i++) {
      s = get_ssids();
      if (s.size() == 0) {
        usleep(100*1000);
        continue;
      }
    }
    for (auto v : s)
      printf("%s\n",v.c_str());
}


//================================================================================
//================================================================================
// Decide on a video standard

#define PAL 0
#define NTSC 1
#define RXD2 16
#define TXD2 17
#define READY 5

/*
RXD2 16 
READY 5
SDA 21
SCL 23
*/


extern uint8_t text_bits[4+12*120];

// active video needs to be 12230us 12229.4857143
// non-delay portion of yeet uses <80us - so delay calculations can almost ignore non NOP code timings

#define YEET_MASK ((1 << 16) | (1 << 5) | (1 << 21) | (1 << 23))

uint16_t _yeetclks[12];     // calculated delays between nybbles
uint16_t _yeetclks_pf[8];   // calculated delays between nybbles
uint32_t _yeet_fwd[16];     // nybble to word mappings
uint32_t _yeet_rev[16];

//int nn = 0;

inline uint32_t IRAM_ATTR cpu_clk()
{
  uint32_t ccount;
  asm volatile ( "rsr %0, ccount" : "=a" (ccount) );
  return ccount;
}

int64_t us_()
{
  static int64_t clk = 0;
  static uint32_t last = 0;
  uint32_t t = cpu_clk();
  clk += (uint32_t)(t-last);
  last = t;
  return clk/240;
}

int64_t ms_()
{
  return us_()/1000;
}

uint32_t _last;
int _elapsed = 0;

void IRAM_ATTR wait_until(uint32_t c)
{
  while (_elapsed < c) {
    uint32_t n = cpu_clk();
    uint32_t dt = n - _last;
    _last = n;
    _elapsed += dt;
  }
}

#define CYCLE_CPU_TICKS (240.0/(315.0/88.0/3.0))
#define LINE_CPU_TICKS (152*CYCLE_CPU_TICKS)
#define LINE8_CPU_TICKS (152*4*CYCLE_CPU_TICKS)

#define NOP() asm volatile ("nop")
int ya = 18;
uint8_t yi = 0;

// sequence to send sprites, must be in ram
uint8_t yeet_seq[] = {
  4,6,8,10,0,2,
  5,7,9,11,1,3,
};

// 96x96 sprite timing
const uint8_t _time_offs[] = {
  11,
  26,
  41,
  56,
  76,
  91,
};

// 32x192 playfield timing
const uint8_t _time_offs_pf[] = {
  0,
  0+8,
  15,
  15+8,
  30,
  30+8,
  45-3, // reorder here for kernel
  45-3+8,
};

// even/odd field access pattern - must be in RAM
uint8_t _5x3_pattern[60] = {
    0 +6,0 +8,0 +10,
    12+7,12+9,12+11,
    24+6,24+8,24+10,
    36+7,36+9,36+11,
    48+6,48+8,48+10,

    0 +0,0 +2,0 +4,
    12+1,12+3,12+5,
    24+0,24+2,24+4,
    36+1,36+3,36+5,
    48+0,48+2,48+4,

    0 +7,0 +9,0 +11,
    12+6,12+8,12+10,
    24+7,24+9,24+11,
    36+6,36+8,36+10,
    48+7,48+9,48+11,

    0 +1,0 +3,0 +5,
    12+0,12+2,12+4,
    24+1,24+3,24+5,
    36+0,36+2,36+4,
    48+1,48+3,48+5,
};

uint32_t _5x3_yeet_first[30];
uint32_t _5x3_yeet_odd[30];
uint32_t _5x3_yeet_even[30];

const uint16_t _5x3_clocks[90] = {
28,36,43,51,58,66,73,81,88,96,103,111,118,126,133,141,148,156,163,171,178,186,193,201,208,216,223,231,238,246,
256,264,271,279,286,294,323,334,341,349,356,364,397,405,415,423,430,438,445,476,486,494,502,510,546,554,561,569,579,587,
258,266,273,281,288,296,329,337,347,355,362,370,395,410,417,425,432,440,473,481,491,499,511,519,521,552,562,570,577,585,
};

void init_yeetclks()
{

// generate starting times for 0..151, double line kernel
  int i = 0;
  for (int j = 0; j < 6; j++) {
    int n = _time_offs[j];
    _yeetclks[i++] = (int)(n*CYCLE_CPU_TICKS);
    _yeetclks[i++] = (int)((n+8)*CYCLE_CPU_TICKS);
  }

// generate starting times for 0..75, single line kernel
  for (int i = 0; i < 8; i++)
    _yeetclks_pf[i] = (int)((_time_offs_pf[i] + 4)*CYCLE_CPU_TICKS);  // +4 a 25-27

// gen yeet patterns for 5x3
  int bias = -16;  // 18 works?
  for (i = 0; i < 30; i++)
     _5x3_yeet_first[i] = (_5x3_clocks[i]+bias)*CYCLE_CPU_TICKS;
  for (; i < 60; i++)
     _5x3_yeet_odd[i-30] = (_5x3_clocks[i]+bias)*CYCLE_CPU_TICKS;
  for (; i < 90; i++)
     _5x3_yeet_even[i-60] = (_5x3_clocks[i]+bias)*CYCLE_CPU_TICKS;
}

int _gk = 33;  // wtf?
int _frames = 0;
uint32_t lc = 0;
char _ramstr[16];

void keyhook(int k)
{
  switch (k) {
    case 45: _gk--; break;
    case 46: _gk++; break;
    default: return;
  }
  ets_printf("_gk:%d\n",_gk);
}

uint8_t _joy2600 = 0xFF;
uint8_t _joy2600_last = 0xFF;

void IRAM_ATTR yeet_isr(void*)
{
    uint32_t gpio_intr_status = GPIO.status; 
    uint32_t gpio_intr_status_h = GPIO.status1.val;

/*
    uint32_t c = cpu_clk();
    lc = c-lc;
    if (lc/240000 < 16) {
      ets_printf(_ramstr,lc/240,69);
      GPIO.status_w1tc |= gpio_intr_status;
      GPIO.status1_w1tc.val |= gpio_intr_status_h;
      return;
    }
    lc = c;
*/

    uint32_t first = cpu_clk();
    uint32_t g = GPIO.in & ~YEET_MASK; //
    uint8_t p;
    const uint8_t* src = text_bits;

    GPIO.enable &= ~YEET_MASK;  // input

    // get correct field to display - atari sets RDX2 bit to signal
    int odd = (GPIO.in & (1 << RDX2_IO)) != 0;
    if ((_dmode == Playfield32x192) || (_dmode == Graphics96x96)) {
          src = _frame_buf;
        if (!(_frame_phase & 1))
          src += FRAME_SIZE;
        if (odd && (_dmode == Playfield32x192))
          src += FIELD_SIZE;
    }
    
    // wait for atari to release ready
    while (!(GPIO.in & (1 << READY_IO)))
        ;
    _last = cpu_clk();    // start the clock
    _elapsed = 0;
    uint32_t a = _last;
    
    //  recv 2 bytes of data in 4 nybbles
    //  YeetData in high byte, switches & joystick + in low
    //  
    uint16_t inw = 0;
    uint16_t m = 1;
    for (int i = 0; i < 4; i++) {
      int w = 8*CYCLE_CPU_TICKS;
      wait_until(w);
      _elapsed -= w;
      uint32_t b = GPIO.in;
      if (b & (1 << 23)) inw |= m; m <<= 1;
      if (b & (1 << 21)) inw |= m; m <<= 1;
      if (b & (1 << 5))  inw |= m; m <<= 1;
      if (b & (1 << 16)) inw |= m; m <<= 1;
    }
    // high byte goes to serial buffer
    _joy2600 = inw;
    //ets_printf(_ramstr,inw >> 8,inw & 0xFF);

    //  Send 4 bytes of data along with the frame buffer
    GPIO.enable |= YEET_MASK;  // now in output mode
    int w = 5*CYCLE_CPU_TICKS;
    wait_until(w);
    _elapsed -= w;

    for (int i = 0; i < 4; i++) {
        p = *src++;
        if (i == 3) p = _dmode; // force dmode (TODO)
        GPIO.out = _yeet_fwd[p >> 4] | g;
        wait_until(4*CYCLE_CPU_TICKS);
        GPIO.out = _yeet_fwd[p & 0xF] | g;
        wait_until(22*CYCLE_CPU_TICKS);
        _elapsed -= 22*CYCLE_CPU_TICKS;
    }

    int n = 0;
    if (_dmode == Text24x24)
    {
      n = 62*CYCLE_CPU_TICKS; // hand timed based on kernel
      wait_until(n);
      _elapsed -= n;
      for (int line = 0; line < 24; line++)
      {
          const uint8_t* seq = _5x3_pattern + (odd ? 30 : 0);
          const uint32_t* clks = _5x3_yeet_first;
          for (int i = 0; i < 15; i++) {
              int j = *seq++;
              p = src[j];
              GPIO.out = _yeet_fwd[p >> 4] | g;
              wait_until(*clks++);
              GPIO.out = _yeet_fwd[p & 0xF] | g;
              wait_until(*clks++);
          }
  
          clks = odd ? _5x3_yeet_odd : _5x3_yeet_even;
          for (int i = 0; i < 15; i++) {
              int j = *seq++;
              p = src[j];
              GPIO.out = _yeet_fwd[p >> 4] | g;
              wait_until(*clks++);
              GPIO.out = _yeet_fwd[p & 0xF] | g;
              wait_until(*clks++);
          }
          wait_until(LINE8_CPU_TICKS);
          _elapsed -= LINE8_CPU_TICKS;   // 76*8 (3+5 lines)
          src += 12*5;
      }
    }
    else if (_dmode == Graphics96x96) 
    {
      n = 80*CYCLE_CPU_TICKS;
      wait_until(n);
      _elapsed -= n;
      for (int line = 0; line < 96; line++)
        {
            const uint16_t* clks = _yeetclks;
            const uint8_t* seq = yeet_seq + (odd ? 6 : 0);
            for (int i = 0; i < 6; i++) {
                p = src[*seq++];
                GPIO.out = _yeet_fwd[p >> 4] | g;
                wait_until(*clks++);
                GPIO.out = _yeet_fwd[p & 0xF] | g;
                wait_until(*clks++);
            }
            wait_until(LINE_CPU_TICKS);
            _elapsed -= LINE_CPU_TICKS;   // 152
            src += 12;
            odd ^= 1;
        }
    }
    else if (_dmode == Playfield32x192) {
      n = 152*CYCLE_CPU_TICKS;
      wait_until(n);
      _elapsed -= n;
      for (int line = 0; line < 192; line++)
        {
            const uint16_t* clks = _yeetclks_pf;
            for (int i = 0; i < 2; i++) {
              p = *src++;
              GPIO.out = _yeet_fwd[p >> 4] | g;
              wait_until(*clks++);
              GPIO.out = _yeet_fwd[p & 0xF] | g;
              wait_until(*clks++);
              
              p = *src++;
              GPIO.out = _yeet_rev[p & 0xF] | g;
              wait_until(*clks++);
              GPIO.out = _yeet_rev[p >> 4] | g;
              wait_until(*clks++);
            }
            wait_until(LINE_CPU_TICKS/2);
            _elapsed -= LINE_CPU_TICKS/2;   // 76
        }
    }

    //if ((_frames & 0x1F) == 0)
    //  ets_printf("%d\n",_frames >> 5);
    _frames++;
    
    GPIO.out = g;
    GPIO.enable &= ~((1 << READY_IO) | (1 << RDX2_IO));  // return ready to input

    // let the atari finish otherwise it may immediately retrigger the int
    _elapsed = 0;
    wait_until(16*CYCLE_CPU_TICKS);

    GPIO.status_w1tc |= gpio_intr_status;
    GPIO.status1_w1tc.val |= gpio_intr_status_h;
}

#define IR_IO (gpio_num_t)2

void yeet_init_hw()
{
    printf("yeet_init_hw core%d\n",xPortGetCoreID());
    strcpy(_ramstr,"%02X %02X\n");
    gpio_set_direction((gpio_num_t)RDX2_IO,GPIO_MODE_OUTPUT);
    //gpio_set_direction((gpio_num_t)READY_IO,GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)21,GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)23,GPIO_MODE_OUTPUT);

    gpio_set_direction((gpio_num_t)READY_IO,GPIO_MODE_INPUT);
    gpio_set_intr_type((gpio_num_t)READY_IO,GPIO_INTR_NEGEDGE); // GPIO_INTR_POSEDGE, GPIO_INTR_NEGEDGE

#if 0
    gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_IRAM);
    gpio_isr_handler_add((gpio_num_t)READY_IO,yeet_isr,0);
    gpio_intr_enable((gpio_num_t)READY_IO);
#else
    gpio_isr_register(yeet_isr,0,ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_IRAM,0);
    gpio_intr_enable((gpio_num_t)READY_IO);
#endif

  pinMode(16,INPUT);
 // pinMode(5,INPUT);
  pinMode(21,INPUT);
  pinMode(23,INPUT);
  
    //pinMode(RDX2_IO,OUTPUT); // serial normally
    if (!init_ir(IR_IO))
      printf("ir init failed?");
}

void calc_ticks()
{
    printf("CYCLE_CPU_TICKS:%d, LINE_CPU_TICKS:%d\n",(int)CYCLE_CPU_TICKS,(int)LINE_CPU_TICKS);
    init_yeetclks();
}

//================================================================================
//================================================================================
// IR Keyboard

static rmt_channel_t _channel = RMT_CHANNEL_0;
static RingbufHandle_t _rb = 0;

#define HSYNC_DIVISOR (80/(9/572))

bool init_ir(int pin)
{
    pinMode(pin,INPUT); // weird. fails to init without this
    
    rmt_config_t rmt_rx;
    rmt_rx.channel = _channel;
    rmt_rx.gpio_num = (gpio_num_t)pin;
    rmt_rx.clk_div = 80;              // microseconds
    rmt_rx.mem_block_num = 1;
    rmt_rx.rmt_mode = RMT_MODE_RX;
    rmt_rx.rx_config.filter_en = false;
    rmt_rx.rx_config.filter_ticks_thresh = 100;
    rmt_rx.rx_config.idle_threshold = 16000;  // longish timeout

    if (rmt_config(&rmt_rx) != ESP_OK)
      return false;
    if (rmt_driver_install(_channel, 1024, 0) != ESP_OK)
      return false;
    _rb = NULL;
    rmt_get_ringbuf_handle(_channel, &_rb);
    rmt_rx_start(_channel, 1);
    return true;
}

//  Webtv keyboard
#define WEBTV_KEYBOARD
#include "src/ir_input.h"

static int _last_key = 0;
void keyboard(const uint8_t* d, int len)
{
    int mods = d[1];          // can we use hid mods instead of SDL? TODO
    int key_code = d[3];      // only care about first key
    if (key_code != _last_key) {
        if (key_code) {
            if (_last_key)
                gui_key(_last_key,0,mods);
            keyhook(key_code);
            gui_key(key_code,1,mods);
            _last_key = key_code;
        } else {
            gui_key(_last_key,0,mods);
            _last_key = 0;
        }
    }
}

void ir_evt(int t, int v)
{
  if (!t) t = 0xFF;
  //printf("%d:%d\n",v,t);
  ir_event(t,v);
  uint8_t dst[10];
  if (get_hid_ir(dst)) {
    keyboard(dst+1,9);
  }
}

const uint8_t _gmap[8] = {
  GENERIC_MENU,
  GENERIC_FIRE,
  GENERIC_SELECT,
  GENERIC_RESET,

  GENERIC_UP,
  GENERIC_DOWN,
  GENERIC_LEFT,
  GENERIC_RIGHT,
};

extern int _joy_map;
void poll_joy()
{
  if (_joy2600 == _joy2600_last)
    return;
  uint8_t changed = _joy2600_last ^ _joy2600;
  _joy2600_last = _joy2600;
  for (int i = 0; i < 8; i++)
  {
    uint8_t m = 0x80 >> i;
    if (m & changed) {
      uint16_t e = _gmap[i];
      if (!(m & _joy2600))
        _joy_map |= e;
      else
        _joy_map &= ~e;
    }
  }
}

// called while waiting for char input
void poll_ir()
{
  poll_joy();
  poll_wifi();
  
  UBaseType_t waiting;
  vRingbufferGetInfo(_rb, NULL, NULL, NULL, &waiting);
  if (!waiting)
    return;
    
  size_t len = 0;
  rmt_item32_t *items = (rmt_item32_t*)xRingbufferReceive(_rb, &len, 1);
  if (items) {
      len >>= 2;
      for (int i = 0; i < len; i++) {
        auto s = items[i];
        ir_evt((s.duration0 + 31) >> 6,s.level0);
        ir_evt((s.duration1 + 31) >> 6,s.level1);
      }
      vRingbufferReturnItem(_rb, (void*)items);
  }
}

void yeet_init()
{
    yeet_init_hw();
    calc_ticks();

    for (int i = 0; i < 16; i++) {
        _yeet_fwd[i] =
        (((i >> 0) & 1) << 16) |
        (((i >> 1) & 1) << 5) |
        (((i >> 2) & 1) << 21) |
        (((i >> 3) & 1) << 23);
        _yeet_rev[i] =
        (((i >> 3) & 1) << 16) |
        (((i >> 2) & 1) << 5) |
        (((i >> 1) & 1) << 21) |
        (((i >> 0) & 1) << 23);
    }

    memset(text_bits,0x00,sizeof(text_bits));    
    text_bits[0] = 0x48;
    text_bits[1] = 0xC8;
}

#include "dirent.h"
vector<string> read_directory(const char* name)
{
    vector<string> files;
    DIR* dirp = opendir(name);
    if (dirp) {
        struct dirent * dp;
        while ((dp = readdir(dirp)) != NULL) {
            if (dp->d_type == DT_DIR) {
                files.push_back(dp->d_name + string("/"));
            } else {
                files.push_back(dp->d_name);
            }
        }
        closedir(dirp);
    }
    return files;
}

void check_fs()
{
  vector<string> f = read_directory(".");
  for (auto s : f)
    printf("%s\n",s.c_str());

  /*
  vector<uint8_t> d;
  get_url("file:///demo/tb/lander.txt",d);
  printf("file is %d bytes\n",d.size());
  d.push_back(0);
  printf("%s\n",(const char*)&d[0]);
  */
}

esp_err_t mount_filesystem()
{
  printf("\n\n\nesp_8_bit\n\nmounting spiffs (will take ~15 seconds if formatting for the first time)....\n");
  uint32_t t = millis();
  esp_vfs_spiffs_conf_t conf = {
    .base_path = "",
    .partition_label = NULL,
    .max_files = 5,
    .format_if_mount_failed = true  // force?
  };
  esp_err_t e = esp_vfs_spiffs_register(&conf);
  if (e != 0)
    printf("Failed to mount or format filesystem: %d. Use 'ESP32 Sketch Data Upload' from 'Tools' menu\n",e);
  vTaskDelay(1);
  printf("... mounted in %d ms\n",millis()-t);
  check_fs();
  return e;
}

void mem(const char* s)
{
    printf("mem %s 8:%d,%d 32:%d,%d\n",s,
      heap_caps_get_free_size(MALLOC_CAP_8BIT),heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
      heap_caps_get_free_size(MALLOC_CAP_32BIT),heap_caps_get_largest_free_block(MALLOC_CAP_32BIT));
}

void start_audio_play();
void setup()
{
    printf("setup\n");    
    _pixel.init();
    pixel_set(0xFF0000); // red
    mount_filesystem();
  
    rtc_clk_cpu_freq_set(RTC_CPU_FREQ_240M);  // need this?
    _event_group = xEventGroupCreate();
    mem("setup");
    
   // Serial.begin(115200);// do we need this?

    yeet_init();
    pixel_set(0xFF00FF); // purple
    start_audio_play();
        
    //Serial2.begin(19200, SERIAL_8N1, RXD2, TXD2); // 19200 emulation for atarivox
    //Serial2.begin(199700, SERIAL_8N1, RXD2, TXD2);  // 199700 ~ 19kB/second from
   
    init_wifi();    
    start_httpd();
    mem("setuped");    
}

int16_t _ab[128];
uint8_t _abi = 0;
void audio_write_1(int16_t s)
{
  _ab[_abi++] = s;
  if (_abi == 128)
  {
    write_pcm_16(_ab,_abi,1);
    _abi = 0;
  }
}

int MASTER_AUDIO_FREQUENCY = 44100;
void set_freq(int f)
{
    MASTER_AUDIO_FREQUENCY = f;
    printf("F to %d\n",f);
}

void write_pcm(int16_t* pcm, int samples, int channels, int freq)
{
    if (MASTER_AUDIO_FREQUENCY != freq)
        set_freq(freq);
    int j = 0;
    if (channels == 1) {
        for (int i = 0; i < samples; i++)
            audio_write_1(pcm[j++]);
    } else {
        for (int i = 0; i < samples; i++) {
            int s = pcm[j++];
            s += pcm[j++];
           // s += pcm[j++];
           // s += pcm[j++];
            audio_write_1(s/2);
        }
    }
}

// TODO. freq?
extern "C"
void sam_raw(unsigned char s)
{
  static int16_t slast = 0;
  int16_t s16 = (s-128) << 7;
  audio_write_1((s16 + slast)>>1);
  audio_write_1(s16);
  slast = s16;
}

extern "C"
void silence()
{
  for (int i = 0; i < 1024; i++)
    audio_write_1(0);
}

int inited = 0;
int read_cmd(char* s, int s_max)
{
  s[0] = 0;
  if (!inited) {
    inited++;
    strcpy(s,"sam \"hellow my name is SAM  \"");
    return strlen(s);
  }
  return 0;
}

//          //_streamer.get("http://rossumur.s3.amazonaws.com/espflix/service.txt");
void shell(void*);

// this loop always runs on app_core (1).
void loop()
{
  shell(0); // never returns?
  return;
  
  while (Serial2.available())
  {
      uint8_t c = Serial2.read();
      printf("S:%02X\n",c);
  }
}
