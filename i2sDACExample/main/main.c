/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "driver/i2s.h"
#include "driver/dac.h"
#include "freertos/queue.h"
#include "test.h"
#include <math.h>



#define REPLAYFROMFILE (0)  //Replay data on test.h or generate samples with code

#if REPLAYFROMFILE
    #define FS    22050.0
    #define BLOCKSIZE   512
#else 
    #define FS    48000.0
    #define BLOCKSIZE   960
#endif


#define TS_PI    1.0/FS*M_PI


static const int i2s_num = 0; // i2s port number
esp_err_t err;
uint8_t new_data=0;           //Flag for generate/replay status
uint8_t bufferout[BLOCKSIZE]; //per-write transaction size

//Configure i2S
static const i2s_config_t i2s_config = {
        .mode = (I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN),
        .sample_rate =  (uint16_t)(FS),
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
	    .communication_format = (I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_LSB),  //LSB for internal DAC!!!!
	    .channel_format =  I2S_CHANNEL_FMT_RIGHT_LEFT,   //Both channels
	    .intr_alloc_flags = 0,
	    .dma_buf_count = 8,
	    .dma_buf_len = 256,
	    .use_apll = false,
};

//configure i2s
void setup_i2s(void){
    err=i2s_driver_install(i2s_num, &i2s_config, 0, NULL);   //install and start i2s driver
    if (err != ESP_OK) {
        printf("Failed installing driver: %d\n", err);
        while (true);
    }
    err=i2s_set_pin(i2s_num, NULL); //for internal DAC, this will enable both of the internal channels
    if (err != ESP_OK) {
        printf("Failed setting pin: %d\n", err);
        while (true);
    }
}

//This task replay continuosly the sine array in test.h 
void replay_audiodata(void *p){
    printf("Playing file example: \n");
    int offset = 0;
    int tot_size = sizeof(sine);
    printf("Samples to play %d\n per channel", tot_size/2);
    size_t bytes_written;
    while(true){
        while (offset < tot_size) {
            int play_len = ((tot_size - offset) > (512)) ? (512) : (tot_size - offset);
            i2s_write_expand(i2s_num, (uint8_t*)(sine+offset), play_len, 8, 16, &bytes_written, portMAX_DELAY);
            offset += play_len;
        }
        offset=0;
    }
}
//This task replays left and righ data in the bufferout array using 8 bit interleaved input -> 16 bit interleaved i2s output 
//Task waits until new_data flag is set
void replay_onthefly(void *p){
    size_t bytes_written;
    while(true){
        if(new_data){
            i2s_write_expand(i2s_num, (uint8_t*)(bufferout), BLOCKSIZE, 8, 16, &bytes_written, portMAX_DELAY);
            new_data=0;
        }
        else{  //no data to send... lets wait 
             vTaskDelay(10 / portTICK_PERIOD_MS); 
        }
    }
}

//This task generates a block of interleaved i2s data for the left and right channels with 8 bit sine signals 
//Task waits until new_data flag is clear
void generate_data_onthefly(void*p){
    uint16_t i,k=0;
    while(1){
        if(new_data==0){
            //Compute i2s DAC data, channels are 1-byte interleaved
            for(i=0;i<BLOCKSIZE;i+=2){
                bufferout[i]=(uint8_t)(((0.5*sinf(TS_PI*100.0*i)+0.5)*255)); //generate sine sample and convert it to 8 bit integer  channel1
                bufferout[i+1]=(uint8_t)(((0.5*sinf(TS_PI*400.0*i+((float)(k*.005)*M_PI))+0.5)*255)); //generate sine sample and convert it to 8 bit integer  channel2
            }
            new_data=1;
            k++;
        }
        else{   //is2 buffer is not done with previous sent data... lets wait
             vTaskDelay(10 / portTICK_PERIOD_MS); 
        }
    }
}


void app_main(void)
{   
    setup_i2s(); //initialize i2s and dac peripherals 

    //run tasks
    #if REPLAYFROMFILE
        xTaskCreate(replay_audiodata, "Replay Stereo Audio", 4096, NULL, 5, NULL);
    #else
        xTaskCreate(generate_data_onthefly, "generate Stereo Audio", 4096, NULL, 5, NULL);
        xTaskCreate(replay_onthefly, "Replay Stereo Audio", 4096, NULL, 5, NULL);
    #endif
    //main loop idle
    while(true){
        vTaskDelay(10 / portTICK_PERIOD_MS); 
    }
    i2s_driver_uninstall(i2s_num); //stop & destroy i2s driver
}

