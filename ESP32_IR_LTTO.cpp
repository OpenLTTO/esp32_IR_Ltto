
/* Copyright (c) 2018 Richie Mickan. All Rights Reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>. *
 *
 * Based on the Code from Neil Kolban: https://github.com/nkolban/esp32-snippets/blob/master/hardware/infra_red/receiver/rmt_receiver.c
 * Based on the Code from pcbreflux: https://github.com/pcbreflux/espressif/blob/master/esp32/arduino/sketchbook/ESP32_IR_Remote/ir_demo/ir_demo.ino
 * Based on the Code from Xplorer001: https://github.com/ExploreEmbedded/ESP32_RMT
 * Based on the Code from Darryl Scott: https://???????
 */

/* This library uses the RMT hardware in the ESP32 to send/receive LTTO infrared data.
 * It uses a single channel of the RMT, therefore up to 8 instances can be used in a single sketch.
 */

#include "Arduino.h"
#ifdef __cplusplus
extern "C" {
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/ringbuf.h"



#ifdef __cplusplus
}
#endif

#include "ESP32_IR_LTTO.h"


#define ROUND_TO                1 //50      //rounding microseconds timings
#define MARK_EXCESS             100 //220   //tweeked to get the right timing
#define SPACE_EXCESS            50 //190    //tweeked to get the right timing
#define TIMEOUT_US              50          //!< RMT receiver timeout value(us) */

// Clock divisor (base clock is 80MHz)
#define CLK_DIV                 80

// Number of clock ticks that represent 10us.  10 us = 1/100th msec.
#define TICK_10_US              (80000000 / CLK_DIV / 100000)

static RingbufHandle_t          ringBuf;



ESP32_IR::ESP32_IR()
{
}


bool ESP32_IR::ESP32_IRrxPIN(int _rxPin, int _port)
{
    bool _status = true;
    
    if (_rxPin >= GPIO_NUM_0 && _rxPin < GPIO_NUM_MAX)      gpioNum = _rxPin;
    else                                                    _status = false;
    
    if (_port >= RMT_CHANNEL_0 && _port < RMT_CHANNEL_MAX)  rmtPort = _port;
    else                                                    _status = false;
    
    return _status;
}

bool ESP32_IR::ESP32_IRtxPIN(int _txPin, int _port)
{
    bool _status = true;
    
    if (_txPin >= GPIO_NUM_0 && _txPin < GPIO_NUM_MAX)      gpioNum = _txPin;
    else                                                    _status = false;
    
    if (_port >= RMT_CHANNEL_0 && _port < RMT_CHANNEL_MAX)  rmtPort = _port;
    else                                                    _status = false;
    
    return _status;
}

void ESP32_IR::initReceive() {
  rmt_config_t config;
  config.rmt_mode = RMT_MODE_RX;
  config.channel = (rmt_channel_t)rmtPort;
  config.gpio_num = (gpio_num_t)gpioNum;
  gpio_pullup_en((gpio_num_t)gpioNum);
  config.mem_block_num = 1; //how many memory blocks 64 x N (0-7)
  config.rx_config.filter_en = 1;
  config.rx_config.filter_ticks_thresh = 100; // 80000000/100 -> 800000 / 100 = 8000  = 125us
  config.rx_config.idle_threshold = TICK_10_US * 100 * 8;      // 8mS
  config.clk_div = CLK_DIV;
  ESP_ERROR_CHECK(rmt_config(&config));
  ESP_ERROR_CHECK(rmt_driver_install(config.channel, 1000, 0));
  rmt_get_ringbuf_handle(config.channel, &ringBuf);
  rmt_rx_start(config.channel, 1);
}


void ESP32_IR::initTransmit(){
  rmt_config_t config;
  config.channel = (rmt_channel_t)rmtPort;
  config.gpio_num = (gpio_num_t)gpioNum;
  config.mem_block_num = 1;//how many memory blocks 64 x N (0-7)
  config.clk_div = CLK_DIV;
  config.tx_config.loop_en = false;
  config.tx_config.carrier_duty_percent = 50;
  config.tx_config.carrier_freq_hz = 38000;
  config.tx_config.carrier_level = (rmt_carrier_level_t)1;
  config.tx_config.carrier_en = 1;
  config.tx_config.idle_level = (rmt_idle_level_t)0;
  config.tx_config.idle_output_en = true;
  config.rmt_mode = (rmt_mode_t)0;//RMT_MODE_TX;
  rmt_config(&config);
  rmt_driver_install(config.channel, 0, 0);//19     /*!< RMT interrupt number, select from soc.h */
}


//void ESP32_IR::sendIR(unsigned int data[], int IRlength){
//     rmt_config_t config;
//     config.channel = (rmt_channel_t)rmtPort;
//     //build item
//     size_t size = (sizeof(rmt_item32_t) * IRlength);
//     rmt_item32_t* item = (rmt_item32_t*) malloc(size); //allocate memory
//     memset((void*) item, 0, size); //wipe current data in memory
//     int i = 0;
//     int x = 0;
//     int offset = 0;
//     Serial.print("Sending.....:");
//     while(x < IRlength) {
//            Serial.print(data[x]);Serial.print(",");Serial.print(data[x+1]);Serial.print(",");
//            //    To build a series of waveforms.
//            buildItem(item[i],data[x],data[x+1]);
//            x=x+2;
//            i++;
//     }
//     Serial.println();
//     //To send data according to the waveform items.
//     rmt_write_items(config.channel, item, IRlength, true);
//     //Wait until sending is done.
//     rmt_wait_tx_done(config.channel,1);
//     //before we free the data, make sure sending is already done.
//     free(item);
//}

void ESP32_IR::sendIR(rmt_item32_t data[], int IRlength)
{
    rmt_config_t config;
    config.channel = (rmt_channel_t)rmtPort;
    rmt_write_items(config.channel, data, IRlength, false);
    //Wait until sending is done.
//    rmt_wait_tx_done(config.channel,1);
    //before we free the data, make sure sending is already done.
//    free(data);
}

void ESP32_IR::stopIR()
{
    rmt_config_t config;
    config.channel = (rmt_channel_t)rmtPort;
    rmt_rx_stop(config.channel);
    Serial.print("Uninstalling..");
    Serial.print("Port : "); Serial.println(config.channel);
    rmt_driver_uninstall(config.channel);
}


void ESP32_IR::getDataIR(rmt_item32_t item, unsigned int* datato, int index) {
    unsigned int lowValue = (item.duration0) * (10 / TICK_10_US)-SPACE_EXCESS;
    lowValue = ROUND_TO * round((float)lowValue/ROUND_TO);
    //Serial.print(lowValue);Serial.print("L ,");
    datato[index] = lowValue;
    unsigned int highValue = (item.duration1) * (10 / TICK_10_US)+MARK_EXCESS;
    highValue = ROUND_TO * round((float)highValue/ROUND_TO);
    //Serial.print(highValue);Serial.print("H ,");
    datato[index+1] = highValue;
}

//void ESP32_IR::buildItem(rmt_item32_t &item,int high_us,int low_us){
//    item.level0 = 1;
//    item.duration0 = (high_us / 10 * TICK_10_US);
//    item.level1 = 0;
//    item.duration1 = (low_us / 10 * TICK_10_US);
//}


void ESP32_IR::decodeRAW(rmt_item32_t *data, int numItems, unsigned int *datato)
{
    //Serial.print("ESP32_IR::Raw IR Code :");
    int _bitCount = 0;
    for (int index = 0; index < numItems; index++)
    {
        getDataIR(data[index], datato, _bitCount);
        _bitCount = _bitCount + 2;
    }
}


int ESP32_IR::readIR(unsigned int *data, int maxBuf)
{
    RingbufHandle_t rb = NULL;
    rmt_config_t config;
    config.channel = (rmt_channel_t)rmtPort;
    rmt_get_ringbuf_handle(config.channel, &rb);
    while(rb)
    {
        size_t itemSize = 0;    //Size of ringBuffer data
        rmt_item32_t *item = (rmt_item32_t*) xRingbufferReceive(rb, &itemSize, (TickType_t)TIMEOUT_US);
        int numItems = itemSize / sizeof(rmt_item32_t);
        if( numItems == 0)  return 0;
        //Serial.print("ESP32_IR::Found num of Items :");Serial.println(numItems*2-1);
        memset(data, 0, maxBuf);
        decodeRAW(item, numItems, data);
        vRingbufferReturnItem(ringBuf, (void*) item);
        return (numItems*2-1);
    }
  vTaskDelete(NULL);
  return 0;
}
