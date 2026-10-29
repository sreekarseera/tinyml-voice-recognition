#include "voice_features.h"
#include <iostream>
#include <iomanip>
#include <cstring>
#include <cstdint>
#include <vector>
// Explicit little-endian float32 input, exactly one 16000-sample window.
int main(){
  std::array<float,voice::Window> audio{};
  for(auto &x:audio){
    unsigned char b[4];if(!std::cin.read(reinterpret_cast<char*>(b),4)){std::cerr<<"expected 16000 float32 samples\n";return 2;}
    uint32_t bits=uint32_t(b[0])|(uint32_t(b[1])<<8)|(uint32_t(b[2])<<16)|(uint32_t(b[3])<<24);
    std::memcpy(&x,&bits,4);
    if(!std::isfinite(x)){std::cerr<<"non-finite audio\n";return 2;}
  }
  if(std::cin.peek()!=std::char_traits<char>::eof()){std::cerr<<"excess audio\n";return 2;}
  auto f=voice::extract(audio.data());
  std::cout<<std::setprecision(9);
  for(size_t n=0;n<f.size();++n)std::cout<<(n?",":"")<<f[n];
  std::cout<<"\n";
}
