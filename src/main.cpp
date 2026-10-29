// New MFCC/linear baseline, not a reconstruction of the reported Edge Impulse CNN.
#include <Arduino.h>
#include <driver/i2s.h>
#include "features.h"
#if !__has_include("trained_model.h")
#error "Train a model with tools/train.py first. See README.md."
#endif
#include "classifier.h"
constexpr gpio_num_t ProfileDSP=GPIO_NUM_4;
constexpr gpio_num_t ProfileNN=GPIO_NUM_5;
float audio[voice::Window];
void setup(){
  Serial.begin(115200);
  const auto start=millis();while(!Serial&&millis()-start<3000)delay(10);
  pinMode(ProfileDSP,OUTPUT);pinMode(ProfileNN,OUTPUT);
  digitalWrite(ProfileDSP,LOW);digitalWrite(ProfileNN,LOW);
  i2s_config_t c{};
  c.mode=(i2s_mode_t)(I2S_MODE_MASTER|I2S_MODE_RX);c.sample_rate=voice::Rate;
  c.bits_per_sample=I2S_BITS_PER_SAMPLE_32BIT;c.channel_format=I2S_CHANNEL_FMT_ONLY_LEFT;
  c.communication_format=I2S_COMM_FORMAT_STAND_I2S;c.dma_buf_count=8;c.dma_buf_len=128;
  i2s_pin_config_t p{};p.bck_io_num=14;p.ws_io_num=15;p.data_out_num=-1;p.data_in_num=16;
  if(i2s_driver_install(I2S_NUM_0,&c,0,nullptr)!=ESP_OK||i2s_set_pin(I2S_NUM_0,&p)!=ESP_OK){
    Serial.println("I2S init failed");while(true)delay(1000);
  }
  Serial.printf("# model dataset kind: %s\n",DatasetKind);
  Serial.println("capture_us,dsp_us,nn_us,label,probability");
}
void loop(){
  int32_t raw[128];size_t count=0;auto capture_start=micros();
  while(count<voice::Window){
    size_t bytes=0;
    auto err=i2s_read(I2S_NUM_0,raw,sizeof(raw),&bytes,pdMS_TO_TICKS(2000));
    if(err!=ESP_OK||bytes==0){Serial.println("# audio timeout");return;}
    for(size_t j=0;j<bytes/4&&count<voice::Window;++j){
      // INMP441 signed 24-bit left-aligned I2S -> normalized 16-bit scale.
      audio[count++]=float(raw[j]>>16)/32768.0f;
    }
  }
  auto capture_us=micros()-capture_start;
  digitalWrite(ProfileDSP,HIGH);auto a=micros();
  auto f=voice::extract(audio);auto dsp_us=micros()-a;digitalWrite(ProfileDSP,LOW);
  digitalWrite(ProfileNN,HIGH);a=micros();
  auto scores=voice::scores(f);
  auto nn_us=micros()-a;digitalWrite(ProfileNN,LOW);
  int best=0;for(int c=1;c<Classes;++c)if(scores[c]>scores[best])best=c;
  float denominator=0;for(int c=0;c<Classes;++c)denominator+=std::exp(scores[c]-scores[best]);
  Serial.printf("%lu,%lu,%lu,%s,%.6f\n",(unsigned long)capture_us,(unsigned long)dsp_us,(unsigned long)nn_us,Labels[best],1/denominator);
  // No fixed profiling delay. GPIO5 covers linear scoring, excluding softmax/output.
}
